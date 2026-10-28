#!/usr/bin/env python3
"""inject_lc.py IN OUT DYLIB_PATH -- add an LC_LOAD_DYLIB to a thin arm64 Mach-O by extending
the load-command area into the header padding (no data shift needed)."""
import struct, sys

def parse(data):
    magic, cputype, cpusub, filetype, ncmds, sizeofcmds, flags, res = struct.unpack_from('<IIIIIIII', data, 0)
    assert magic == 0xfeedfacf, 'not a thin 64-bit Mach-O: %08x' % magic
    off = 32
    min_section_off = len(data)
    for i in range(ncmds):
        cmd, cmdsize = struct.unpack_from('<II', data, off)
        if cmd == 0x19:
            segname = data[off+8:off+24].split(b'\0')[0].decode()
            nsects = struct.unpack_from('<I', data, off+64)[0]
            for s in range(nsects):
                so = off + 72 + s * 80
                size, offset = struct.unpack_from('<QQ', data, so + 40)
                if offset and size and offset < min_section_off:
                    min_section_off = offset
        off += cmdsize
    return dict(filetype=filetype, ncmds=ncmds, sizeofcmds=sizeofcmds, flags=flags,
                end=off, min_section_off=min_section_off)

src, dst, dylib = sys.argv[1], sys.argv[2], sys.argv[3]
data = bytearray(open(src, 'rb').read())
st = parse(bytes(data))
print('in : ncmds=%d sizeofcmds=0x%x cmds_end=0x%x first_section=0x%x slack=0x%x' % (
    st['ncmds'], st['sizeofcmds'], st['end'], st['min_section_off'], st['min_section_off'] - st['end']))

name = dylib.encode() + b'\0'
cmdsize = (24 + len(name) + 7) & ~7
slack = st['min_section_off'] - st['end']
assert slack >= cmdsize, 'not enough header slack: %d < %d' % (slack, cmdsize)

lc = struct.pack('<IIIIII', 0xc, cmdsize, 24, 2, 0x10000, 0x10000)  # LC_LOAD_DYLIB
lc += name + b'\0' * (cmdsize - 24 - len(name))
data[st['end']:st['end'] + cmdsize] = lc
struct.pack_into('<II', data, 16, st['ncmds'] + 1, st['sizeofcmds'] + cmdsize)
open(dst, 'wb').write(data)
st2 = parse(open(dst, 'rb').read())
print('out: ncmds=%d sizeofcmds=0x%x cmds_end=0x%x slack=0x%x load-dylib-name=%s' % (
    st2['ncmds'], st2['sizeofcmds'], st2['end'], st2['min_section_off'] - st2['end'],
    dylib.encode() in bytes(data)))
