"""Build the round-49 SRAM image and verify it against the board readback."""
import hashlib
import os
from pathlib import Path
import subprocess

base = Path(__file__).resolve().parent
src = base.parents[1]
here = base / "hand-capture-20261106"
swing = base / "swingup-20261106"
pulse = base / "motor-direction-20261106"
out = src / "build/round49"
out.mkdir(parents=True, exist_ok=True)
hal = src / "Drivers/STM32F1xx_HAL_Driver"
cmsis = src / "Drivers/CMSIS"
profile_options = [
    "-DRELEASE_TRIAL", "-DHALF_ANGLE_KP", "-DPOSITION_TRIAL", "-DSTREAM_TRIAL",
    "-DHYBRID_TRIAL", "-DTRIAL_LENGTH_US=150000000", "-DTRIAL_POSITION_CAP=1440",
    "-DSOLO_ENTRY_DIAGNOSTIC", "-DSOLO_ENTRY_WINDOW_ADC=278", "-DLIMITED_HOLD",
    "-DMERGED_HOLD", "-DCARRY_HOLD",
]

def run(args):
    subprocess.run([str(s) for s in args], check=True, cwd=out)

host = os.environ.get("CXX", "g++")
for name, sources in [
    ("check_limits", [here/"control.cpp", here/"check_limits.cpp"]),
    ("check_swing", [swing/"check_swing.cpp"]),
    ("check_capture", [here/"control.cpp", swing/"check_capture.cpp"]),
]:
    exe = out / (name + (".exe" if os.name == "nt" else ""))
    run([host, "-std=c++14", "-Wall", "-Wextra", "-Werror", "-O2", *profile_options,
         "-I", base/"calibration", "-I", src, *sources, "-o", exe])
    run([exe])

opts=["-mcpu=cortex-m3","-mthumb","-Os","-ffunction-sections","-fdata-sections","-ffreestanding",
      "-fno-builtin","-DSTM32F103xB","-DUSE_HAL_DRIVER",*profile_options]
link_optimization=["-flto","-Oz","-freciprocal-math"]
opts+=link_optimization
for path in [base/"calibration",src,src/"Platform",hal/"Inc",hal/"Inc/Legacy",cmsis/"Include",cmsis/"Device/ST/STM32F1xx/Include",pulse]:
    opts+=["-I",path]
library=[src/"Platform/adc.c",src/"Platform/system_stm32f1xx.c",
         *(hal/"Src"/("stm32f1xx_hal_"+s+".c") for s in ("adc","adc_ex","gpio","rcc","rcc_ex"))]
cxxopts=["-std=c++17","-fno-exceptions","-fno-rtti","-fno-use-cxa-atexit","-fno-threadsafe-statics"]
objects=[]
entry=swing/"hybrid.c"
for path in [entry,here/"control.cpp",*library]:
    cpp=path.suffix==".cpp"
    obj=out/(path.stem+".o")
    local=["-Wall","-Wextra","-Werror","-Wno-unused-function"] if path.parent in (here,swing) else []
    run(["arm-none-eabi-g++" if cpp else "arm-none-eabi-gcc",*opts,*(cxxopts if cpp else []),
         *local,"-c",path,"-o",obj])
    objects.append(obj)
run(["arm-none-eabi-gcc","-mcpu=cortex-m3","-mthumb","-nostdlib",*link_optimization,"-Wl,--gc-sections",
     "-Wl,-Map,capture.map","-T",pulse/"ram.ld",*objects,"-lc_nano","-lgcc","-o","capture.elf"])
run(["arm-none-eabi-objcopy","-O","binary","capture.elf","capture.bin"])
run(["arm-none-eabi-size","capture.elf"])
image=(out/"capture.bin").read_bytes()
if len(image)>12288:
    raise ValueError("exceeds loader's 12 KiB SRAM image window")
symbols=subprocess.check_output(["arm-none-eabi-nm","-n",out/"capture.elf"],text=True)
def addr(name):
    return int(next(l.split()[0] for l in symbols.splitlines() if l.split()[-1]==name),16)
assert addr("vectors")==0x20001000 and addr("__stack_top")==0x20005000
assert addr("__stack_top")-addr("__bss_end")>=4096
digest = hashlib.sha256(image).hexdigest()
if digest != "11e8aa4cb44d42c3954aeb140b411e66b1e06383165dcbd6763e61d01fcec45e":
    raise SystemExit(f"Image differs from round 49: {digest}. Check the toolchain version.")
print(f"PASS board readback match: {len(image)} bytes, SHA256 {digest}")
