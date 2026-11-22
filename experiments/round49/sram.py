"""Board-checked ROM protocol for the round-49 SRAM image."""
from functools import reduce
from operator import xor
import hashlib
import struct
import time

FLASH = 0x08000000
ORIGIN = 0x20001000
FLASH_SHA = "a1b8251b4762d8355768098f118f607f7b611538d7bdb572bd79ec1e4f2f4b0b"

def exact(port, size):
    data = port.read(size)
    if len(data) != size:
        raise IOError(f"expected {size} bytes, got {len(data)}")
    return data


def ack(port, label):
    value = exact(port, 1)
    if value != b"\x79":
        raise IOError(f"{label}: expected ACK, received {value.hex()}")


def readonly_command(port, command):
    if command not in (0x00, 0x02, 0x11):
        raise ValueError("unsupported read-only command")
    port.write(bytes((command, command ^ 255)))
    ack(port, f"command {command:02x}")


def descriptor(port, command):
    readonly_command(port, command)
    n = exact(port, 1)[0]
    data = exact(port, n + 1)
    ack(port, "descriptor end")
    return data


def read_memory(port, address, size):
    if not 1 <= size <= 256:
        raise ValueError("invalid read length")
    readonly_command(port, 0x11)
    addr = address.to_bytes(4, "big")
    port.write(addr + bytes([reduce(xor, addr)]))
    ack(port, "address")
    port.write(bytes([size - 1, (size - 1) ^ 255]))
    ack(port, "read length")
    return exact(port, size)


def reset_c03c(port, rom):
    # CH9102 DTR#/RTS# are active-low; Q1 pulls NRST down when DTR#=H,
    # RTS#=L. Q2 selects BOOT0 high while RTS#=L.
    port.dtr = False
    port.rts = False
    time.sleep(0.06)
    port.rts = True
    time.sleep(0.12)
    if rom:
        port.dtr = True
    else:
        port.rts = False
    time.sleep(0.2)


def enter(port, proof):
    reset_c03c(port, True)
    port.reset_input_buffer()
    port.write(b"\x7f"); ack(port, "sync")
    capabilities = descriptor(port, 0)
    if (descriptor(port, 2) != b"\x04\x10" or not {0x11, 0x21, 0x31}.issubset(capabilities[1:])
            or read_memory(port, 0x1ffff7e8,12).hex() != proof["silicon_uid"]):
        raise IOError("wrong MCU or bootloader capabilities")


def address(port, addr):
    a = addr.to_bytes(4, "big")
    port.write(a + bytes([reduce(xor, a)]))
    ack(port, "address")


def load(port, code, proof):
    max_image_bytes = 0x3000
    code += bytes((-len(code)) % 4)
    sp, reset = struct.unpack_from("<II", code)
    if not (64 < len(code) <= max_image_bytes and sp == 0x20005000 and reset & 1 and ORIGIN < reset < ORIGIN+len(code)):
        raise ValueError("invalid SRAM image or vectors")
    enter(port, proof)
    data = b"".join(read_memory(port, FLASH+i, 256) for i in range(0,65536,256))
    if hashlib.sha256(data).hexdigest() != FLASH_SHA:
        raise ValueError("calibrated flash identity changed")
    for i in range(0, len(code), 256):
        addr = ORIGIN+i
        chunk = code[i:i+256]
        if addr < ORIGIN or addr+len(chunk) > ORIGIN+max_image_bytes or len(chunk)%4:
            raise ValueError("only aligned SRAM writes are supported")
        port.write(b"\x31\xce"); ack(port, "RAM write")
        address(port, addr)
        packet = bytes([len(chunk)-1])+chunk
        port.write(packet+bytes([reduce(xor,packet)])); ack(port, "RAM payload")
        if read_memory(port, addr, len(chunk)) != chunk:
            raise IOError("SRAM readback mismatch")
    complete=b"".join(read_memory(port,ORIGIN+i,min(256,len(code)-i))
                      for i in range(0,len(code),256))
    if complete!=code: raise IOError("complete SRAM readback mismatch before GO")
    # Release BOOT0 without asserting reset; the next physical reset will load flash.
    port.rts = False
    port.dtr = False
    port.write(b"\x21\xde"); ack(port, "GO")
    address(port, ORIGIN)
    port.parity = "N"
    port.timeout = 0.18
    return {"sram_base":hex(ORIGIN),"ram_verified_bytes":len(code),
            "sram_sha256":hashlib.sha256(code).hexdigest(),"protected_flash_sha256":FLASH_SHA}
