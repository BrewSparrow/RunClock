#!/usr/bin/env python3
"""Reject a compiled x86-64 resident syscall bridge with the wrong stack ABI."""
from pathlib import Path
import struct, sys

def verify(elf):
    def need(offset,length):
        if offset<0 or length<0 or offset+length>len(elf):raise ValueError('Truncated resident ELF')
    need(0,64)
    if elf[:7]!=bytes.fromhex('7f454c46020101') or struct.unpack_from('<H',elf,18)[0]!=62:
        raise ValueError('Expected little-endian x86-64 ELF')
    shoff=struct.unpack_from('<Q',elf,40)[0]
    stride,count=struct.unpack_from('<HH',elf,58)
    if stride!=64 or not count:raise ValueError('Missing resident section table')
    need(shoff,stride*count)
    sections=[struct.unpack_from('<IIQQQQIIQQ',elf,shoff+i*stride) for i in range(count)]
    symbols={}
    for section in sections:
        if section[1]!=2:continue
        _,_,_,_,offset,size,link,_,_,entry=section
        if entry!=24 or size%24 or link>=len(sections):raise ValueError('Invalid resident symbol table')
        strings=sections[link]
        if strings[1]!=3:raise ValueError('Missing resident strings')
        need(offset,size);need(strings[4],strings[5])
        for at in range(offset,offset+size,entry):
            name,info,other,index,address,length=struct.unpack_from('<IBBHQQ',elf,at)
            if not index:continue
            if name>=strings[5]:raise ValueError('Invalid resident symbol name')
            end=elf.find(b'\0',strings[4]+name,strings[4]+strings[5])
            if end<0:raise ValueError('Unterminated resident symbol')
            text=elf[strings[4]+name:end]
            if text not in {b'__crt_syscall',b'ptr_syscall'}:continue
            if text in symbols:raise ValueError('Ambiguous resident syscall symbol')
            symbols[text]=(index,address,length)
    if len(symbols)!=2:raise ValueError('Missing resident syscall bridge or gateway slot')
    index,address,length=symbols[b'__crt_syscall']
    if index>=len(sections) or length<30:raise ValueError('Invalid resident syscall bridge bounds')
    section=sections[index]
    relative=address-section[3]
    if section[1]!=1 or section[2]&6!=6 or relative<0 or relative+30>section[5]:
        raise ValueError('Resident syscall bridge is not executable file data')
    offset=section[4]+relative;need(offset,30)
    code=elf[offset:offset+30]
    prefix=bytes.fromhex('4889f84889f74889d64889ca4d89c24d89c84c8b4c2408')
    if code[:23]!=prefix or code[23:25]!=b'\xff\x15' or code[29]!=0xc3:
        raise ValueError('Resident syscall bridge stack ABI mismatch: prologue, argument shifts or return changed')
    target=address+29+struct.unpack_from('<i',code,25)[0]
    slotIndex,slotAddress,slotBytes=symbols[b'ptr_syscall']
    if slotIndex>=len(sections) or slotAddress!=target or slotBytes<8 or sections[slotIndex][2]&3!=3:
        raise ValueError('Resident syscall bridge does not call its writable gateway slot')
    return {'bridge':'__crt_syscall','bytes_checked':30,'naked_stack_abi_verified':True}

if __name__=='__main__':
    if len(sys.argv)!=2:raise SystemExit('Usage: verify-resident-syscall.py RESIDENT_ELF')
    verify(Path(sys.argv[1]).read_bytes())
    print('Verified resident syscall bridge: no stack prologue, six arguments, gateway slot and direct return.')
