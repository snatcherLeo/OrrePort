"""2026-11-07 (the reference playthrough P12's crash, port-gaps 74): read a Windows minidump without a debugger: the exception (code,
address), the faulting thread's registers, and every value on its stack that falls inside a module's image (candidate
return addresses), as 'module+offset', plus the exe's addresses for addr2line.
    python -I dmpstack.py <file.dmp> [exe name, default orrecolo.exe] [max stack words, default 4000]
Then, for the exe's addresses (the exe's base is printed; orrecolo.exe loads at 0x10000000):
    addr2line -f -e orrecolo.exe <addresses>      (MinGW's, in msys64/mingw64/bin)
Dumps: %LOCALAPPDATA%\\CrashDumps\\<exe>.<pid>.dmp (the pid is in the Application log's event 1000 for the crash).
Stale values on the stack mix in; read the chain from the deepest (largest offset) frames upwards.
"""
import struct
import sys

path = sys.argv[1]
EXE = (sys.argv[2] if len(sys.argv) > 2 else "orrecolo.exe").lower()
MAXW = int(sys.argv[3]) if len(sys.argv) > 3 else 4000
data = open(path, "rb").read()

sig, ver, nstreams, dir_rva = struct.unpack_from("<IIII", data, 0)
assert sig == 0x504D444D, "not a minidump"
streams = {}
for i in range(nstreams):
    stype, size, rva = struct.unpack_from("<III", data, dir_rva + i * 12)
    streams.setdefault(stype, []).append((size, rva))


def utf16_at(rva):
    n = struct.unpack_from("<I", data, rva)[0]
    return data[rva + 4: rva + 4 + n].decode("utf-16-le", "replace")


# modules (stream 4)
mods = []
size, rva = streams[4][0]
n = struct.unpack_from("<I", data, rva)[0]
for i in range(n):
    base = rva + 4 + i * 108
    img, imgsize = struct.unpack_from("<QI", data, base)
    name_rva = struct.unpack_from("<I", data, base + 20)[0]
    mods.append((img, img + imgsize, utf16_at(name_rva).split("\\")[-1]))


def modof(a):
    for lo, hi, nm in mods:
        if lo <= a < hi:
            return nm, a - lo, lo
    return None


# memory ranges: Memory64List (9) or MemoryList (5)
ranges = []
if 9 in streams:
    size, rva = streams[9][0]
    cnt, baserva = struct.unpack_from("<QQ", data, rva)
    off = baserva
    for i in range(cnt):
        start, sz = struct.unpack_from("<QQ", data, rva + 16 + i * 16)
        ranges.append((start, sz, off))
        off += sz
if 5 in streams:
    size, rva = streams[5][0]
    cnt = struct.unpack_from("<I", data, rva)[0]
    for i in range(cnt):
        start, sz, mrva = struct.unpack_from("<QII", data, rva + 4 + i * 16)
        ranges.append((start, sz, mrva))


def read(addr, n):
    for start, sz, off in ranges:
        if start <= addr and addr + n <= start + sz:
            return data[off + addr - start: off + addr - start + n]
    return None


# exception (stream 6)
size, rva = streams[6][0]
tid = struct.unpack_from("<I", data, rva)[0]
code, flags, rec, exaddr, nparams = struct.unpack_from("<IIQQI", data, rva + 8)
params = struct.unpack_from("<15Q", data, rva + 8 + 32)
ctx_size, ctx_rva = struct.unpack_from("<II", data, rva + 8 + 152)
m = modof(exaddr)
print(f"thread {tid}: exception {code:#x} at {exaddr:#x} ({m[0]}+{m[1]:#x})" if m else f"exception {code:#x} at {exaddr:#x}")
print("  params:", [hex(p) for p in params[:nparams]])
# CONTEXT (x64): the general registers from offset 0x78
regs = struct.unpack_from("<17Q", data, ctx_rva + 0x78)
names = ["rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rip"]
print("  " + " ".join(f"{nm}={v:#x}" for nm, v in zip(names, regs)))
rsp = regs[4]
for nm, v in zip(names, regs):
    mm = modof(v)
    if mm:
        print(f"  {nm} points into {mm[0]}+{mm[1]:#x}")

print("stack candidates (offset from rsp: module+offset):")
exe_addrs = []
for i in range(MAXW):
    w = read(rsp + i * 8, 8)
    if w is None:
        continue
    v = struct.unpack("<Q", w)[0]
    mm = modof(v)
    if mm:
        print(f"  +{i * 8:#06x}: {mm[0]}+{mm[1]:#x}  ({v:#x})")
        if mm[0].lower() == EXE:
            exe_addrs.append(v)
print("EXE_ADDRS", " ".join(hex(a) for a in exe_addrs))
for lo, hi, nm in mods:
    if nm.lower() == EXE:
        print(f"EXE_BASE {lo:#x}")
