"""Incremental, CRC-checked decoder for the STM32 streamed trial packets."""
import binascii
import struct

class Decoder:
    LENGTHS={1:48,2:36,3:28,4:12,5:36,6:24,7:16,8:24,9:64}
    def __init__(self):
        self.buffer=bytearray()
        self.crc_errors=0
        self.discarded_bytes=0
        self.packets=0

    def feed(self,data):
        self.buffer.extend(data)
        result=[]
        while len(self.buffer)>=4:
            start=self.buffer.find(b'\xa5\x5a')
            if start<0:
                n=len(self.buffer)-1
                self.discarded_bytes+=n
                del self.buffer[:n]
                break
            if start:
                self.discarded_bytes+=start
                del self.buffer[:start]
            if len(self.buffer)<4: break
            tag,length=self.buffer[2:4]
            if self.LENGTHS.get(tag)!=length:
                self.discarded_bytes+=1
                del self.buffer[0]
                continue
            if len(self.buffer)<length+6: break
            packet=self.buffer[:length+6]
            if binascii.crc_hqx(packet[2:-2],0xffff)!=int.from_bytes(packet[-2:],'little'):
                self.crc_errors+=1
                self.discarded_bytes+=1
                del self.buffer[0]
                continue
            values=struct.unpack('<'+'i'*(length//4),packet[4:-2])
            result.append((tag,values))
            self.packets+=1
            del self.buffer[:length+6]
        return result

def records(packets):
    for tag,values in packets:
        if tag==1: yield b'BAL1',values
        elif tag==2:
            yield b'BALD1',values[:7]
            yield b'BALP1',(values[0],values[7],values[8])
        elif tag==3: yield b'BALE1',values
        elif tag==4: yield b'LED1',values
        elif tag==5:
            yield b'BALD1',values[:7]
            yield b'ADCA1',(values[0],values[7],values[8])
        elif tag==6: yield b'BALS1',values
        elif tag==7: yield b'BALT1',values
        elif tag==8: yield b'BALIO1',values
        elif tag==9:
            readings=tuple(sample for pair in values[6:] for sample in (pair&0xffff,(pair>>16)&0xffff))
            yield b'BALADC2',(*values[:6],*readings)
