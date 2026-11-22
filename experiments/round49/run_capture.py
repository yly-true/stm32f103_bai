"""Load a verified round-49 image into SRAM and record the locally started run."""
import argparse
from contextlib import ExitStack
import csv
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import time

import serial
from serial.tools import list_ports

import sram
from stream_protocol import Decoder, records

IMAGE_SHA = "11e8aa4cb44d42c3954aeb140b411e66b1e06383165dcbd6763e61d01fcec45e"
BUILD = Path(__file__).resolve().parents[2] / "build/round49"

SCHEMAS={
    b"BAL1":("status",("ms","phase","ready","keys","adc","battery_cV","encoder","reason",
                       "origin","run_us","n","cycles")),
    b"BALD1":("trace",("index","mcu_us","adc","encoder","requested_pwm","capped_pwm","cycles_before")),
    b"BALE1":("end",("n","reason","run_us","encoder","adc","battery_cV","cycles")),
    b"LED1": ("lamp", ("ms","mode","pad_on")),
    b"BALP1": ("components", ("index","angle_requested_pwm","position_correction_pwm")),
    b"BALS1": ("swing", ("index","mode","omega_millirad_s","energy_milli","flags","adc_span")),
    b"BALT1": ("trajectory", ("index","target_counts","amplitude_counts","speed_counts_s")),
    b"BALIO1": ("pads", ("ms","phase","direction_pad_mask","pwm_pin_high","timer_running","ccer")),
    b"BALADC2": ("angle-confirmation", ("index","attempt","confirmed","prior_adc","first_mean","second_mean",
                   *(f"first_{i}" for i in range(10)),*(f"second_{i}" for i in range(10)))),
}


def run(port_name, proof_path):
    code = (BUILD / "capture.bin").read_bytes()
    if hashlib.sha256(code).hexdigest() != IMAGE_SHA:
        raise ValueError("SRAM image differs from the round-49 readback")
    proof = json.loads(proof_path.read_text(encoding="utf-8-sig"))
    if not re.fullmatch(r"[0-9a-f]{24}",proof["silicon_uid"]):
        raise ValueError("invalid MCU identity in board proof")
    match = re.search(r"\bSER=(\S+)",proof["bridge"])
    if not match or not any(p.device==port_name and (p.vid,p.pid,p.serial_number)==(0x1a86,0x55d4,match[1])
                            for p in list_ports.comports()):
        raise ValueError("expected CH9102 USB bridge absent")
    dest = BUILD / "runs" / datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    dest.mkdir(parents=True, exist_ok=False)
    print(f"Recording to {dest}",flush=True)
    port=serial.Serial(port=None,baudrate=115200,parity="E",timeout=1.5,write_timeout=1)
    port.dtr=False
    port.rts=False
    port.port=port_name
    with port, ExitStack() as stack:
        try:
            info=sram.load(port,code,proof)
        except Exception:
            sram.reset_c03c(port,False)
            raise
        (dest/"load.json").write_text(json.dumps(info,indent=2),encoding="utf8")
        port.timeout=.05
        raw=stack.enter_context((dest/"uart.bin").open("xb"))
        writers={}
        for tag,(name,columns) in SCHEMAS.items():
            f=stack.enter_context((dest/(name+".csv")).open("x",encoding="utf8",newline=""))
            writer=csv.writer(f)
            writer.writerow(("host_s",*columns))
            writers[tag]=(writer,f)
        decoder=Decoder()
        start=time.monotonic()
        first_status=True
        ended=False
        while time.monotonic()-start<1070:
            now=time.monotonic()-start
            if first_status and now>7:
                sram.reset_c03c(port,False)
                raise IOError("no initial status; returned to calibrated flash")
            chunk=port.read(512)
            if not chunk: continue
            raw.write(chunk)
            raw.flush()
            for tag,values in records(decoder.feed(chunk)):
                if tag not in writers: continue
                writer,f=writers[tag]
                writer.writerow((round(now,6),*values))
                f.flush()
                if tag==b"BALE1":
                    print(f"Stopped, reason={values[1]}. Disconnect external 12 V.",flush=True)
                if tag==b"BAL1":
                    ms,phase,ready,keys,adc,cv,enc,reason,origin,run_us,n,cycles=values
                    if first_status:
                        print(f"SRAM verified. First status: phase={phase}, keys={keys}, battery={cv/100:.2f} V, encoder={enc}, cycles={cycles}",flush=True)
                        first_status=False
                    ended=phase==2 and 400<=cv<=600
            if ended: break
        summary=dict(packets=decoder.packets,crc_errors=decoder.crc_errors,discarded_bytes=decoder.discarded_bytes,
                     partial_buffer_bytes=len(decoder.buffer))
        (dest/"transport-summary.json").write_text(json.dumps(summary,indent=2),encoding="utf8")

if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--proof",type=Path,required=True,help="local flash-before.json for this calibrated apparatus")
    parser.add_argument("--port",default="COM8")
    args=parser.parse_args()
    run(args.port,args.proof)
