"""GS event scripts (resource type 7, `.scd`): header, disassembly and a readable listing.
Opcode table and operand sizes from the cms_opNN handlers (src/game/gs_thread.c, dispatch table
lbl_803155D0); natives from the native table lbl_802E1CF0 (main.dol). docs/engine/script-vm.md.
Read-only reader (no file output): a port candidate (script tools, a debugger), see docs/tools.md."""
import struct

V, G = "verified", "guess"
# opcode -> (mnemonic, operand kind, status of the meaning). Operand sizes are verified from the handlers.
# kinds: none; imm = u8 type + u32 (type 2 int, 3 float); u32; call = u32 function, u16 args, u16 locals;
# d1 / d2 = one / two variable descriptors; d1u16 / d2u16 = descriptor(s) + u16; s1u16 = 1 skipped byte + u16;
# s2u16 = 2 skipped bytes + u16.
OPS = {0: ("nop", "none", V), 1: ("exit", "none", V), 2: ("push_m1", "none", V), 3: ("pop", "s1u16", V),
       4: ("call", "call", V), 5: ("return", "none", V), 6: ("jump", "u32", V), 7: ("jump_false", "u32", V),
       8: ("push", "imm", V), 13: ("and", "d2", G), 14: ("or", "d2", G), 15: ("push_ref", "d1", V),
       16: ("push_vars", "d1u16", V), 17: ("not", "d1", G), 18: ("neg", "d1", G), 19: ("push_var", "d1", G),
       20: ("mul", "d2", G), 21: ("div", "d2", G), 22: ("mod", "d2", G), 23: ("add", "d2", G), 24: ("sub", "d2", G),
       25: ("lt", "d2", G), 26: ("le", "d2", G), 27: ("gt", "d2", G), 28: ("ge", "d2", G), 29: ("eq", "d2", G),
       30: ("ne", "d2", G), 31: ("set", "d2", V), 32: ("copy", "d2u16", G), 33: ("land", "d2", G), 34: ("lor", "d2", G),
       35: ("op35", "s2u16", G), 36: ("op36", "s2u16", G), 37: ("callstd", "s2u16", V)}
BINOP = {"and": "&", "or": "|", "mul": "*", "div": "/", "mod": "%", "add": "+", "sub": "-", "lt": "<", "le": "<=",
         "gt": ">", "ge": ">=", "eq": "==", "ne": "!=", "land": "&&", "lor": "||"}


def parse_header(data):
    """Header (as seen on disc; field meanings partly guessed): +0x00 u16 script id (the high half of a script
    function word, cms_ScriptCreateThread), +0x04 u16 function count (verified, cms_op04_call), +0x0C u32 code start =
    end of the function table (verified on every script), +0x18 u32 function offsets (verified); +0x08 u16 and +0x10
    unknown."""
    nf = struct.unpack(">H", data[4:6])[0]
    return {"words": list(struct.unpack(">6I", data[:0x18])), "functions": list(struct.unpack(f">{nf}I", data[0x18:0x18 + 4 * nf]))}


def disassemble(data, start=0x70, end=None):
    """[(address, mnemonic, kind, operands)] for the bytecode from start to end."""
    end = end or len(data); ip = start; out = []
    while ip < end:
        op = data[ip]; at = ip; ip += 1
        name, kind, _ = OPS.get(op, (f"bad_{op:02x}", "none", G)); a = ()
        try:
            if kind == "imm": a = (data[ip], data[ip + 1:ip + 5]); ip += 5
            elif kind == "u32": a = (struct.unpack(">I", data[ip:ip + 4])[0],); ip += 4
            elif kind == "call": a = struct.unpack(">IHH", data[ip:ip + 8]); ip += 8
            elif kind == "d1": a = (data[ip],); ip += 1
            elif kind == "d2": a = (data[ip], data[ip + 1]); ip += 2
            elif kind == "d1u16": a = (data[ip], struct.unpack(">H", data[ip + 1:ip + 3])[0]); ip += 3
            elif kind == "d2u16": a = (data[ip], data[ip + 1], struct.unpack(">H", data[ip + 2:ip + 4])[0]); ip += 4
            elif kind == "s1u16": a = (struct.unpack(">H", data[ip + 1:ip + 3])[0],); ip += 3
            elif kind == "s2u16": a = struct.unpack(">HH", data[ip:ip + 4]); ip += 4
        except (struct.error, IndexError):
            break
        out.append((at, name, kind, a))
    return out


def imm_text(t, raw):
    if t == 3: return f"{struct.unpack('>f', raw)[0]:g}"
    v = struct.unpack(">i", raw)[0]
    return str(v) if t == 2 else f"{v}/*type{t}*/"


def listing(data, natives, annotate=None, end=None):
    """Readable listing: function labels, jump labels, and native calls rebuilt from the stack code
    (`name(args)`); statements are emitted where values are consumed (pop / set / jumps / return).
    natives: {index: name}; annotate(native_name, args) -> comment or ''."""
    # start at the first function (the lowest non-zero function offset; slot 0 = no function). A fixed start of 0x70
    # hid 223 functions in 130 scripts (found 2026-10-16); the table's end (header +0x0C) can be followed by a few
    # data bytes (D1_labo_1F), so the first function is the safe start.
    hdr = parse_header(data)
    ins = disassemble(data, start=min([o for o in hdr["functions"] if o] or [hdr["words"][3]]), end=end)
    targets = {a[0] for at, n, k, a in ins if n in ("jump", "jump_false")}
    funcs = {off: i for i, off in enumerate(hdr["functions"])}
    lines, stack, calls = [], [], []
    def popv(): return stack.pop() if stack else "?"
    def operand(d):
        if d & 0x80: return popv()
        idx = popv(); return f"{'L' if d & 0x40 else 'G'}{idx}" if str(idx).lstrip("-").isdigit() else f"{'L' if d & 0x40 else 'G'}[{idx}]"
    def emit(s): lines.append(f"    {s}")
    for at, n, k, a in ins:
        if at in funcs: lines.append(f"\nfunc{funcs[at]}:   ; offset {at:#x}")
        if at in targets: lines.append(f"  L_{at:04x}:")
        if n == "nop": continue
        if n == "push": stack.append(imm_text(*a))
        elif n == "push_m1": stack.append("-1")
        elif n == "push_vars":
            d, cnt = a; base = popv()
            for i in range(cnt):
                stack.append(f"{'L' if d & 0x40 else 'G'}{int(base) + i}" if str(base).isdigit() else f"var[{base}+{i}]")
        elif n in ("push_var", "push_ref"):
            v = operand(a[0]); stack.append(("&" if n == "push_ref" else "") + str(v))
        elif n in BINOP:
            # the VM reads the FIRST operand as the right-hand side and the second as the left (gs_vm.c,
            # cms_op24_subtract / cms_op25_lessThan: rhsDesc first, then lhsDesc; result lhs OP rhs). Evaluate in that
            # order (a stack operand pops the right-hand value first) and print left OP right. *Fixed 2026-11-11:*
            # until then the listings printed the first operand on the left, so every -, /, %, <, <=, > and >= was
            # shown backwards (+, *, ==, !=, &, | are unaffected).
            r = operand(a[0]); l = operand(a[1]); stack.append(f"({l} {BINOP[n]} {r})")
        elif n in ("not", "neg"):
            v = operand(a[0]); stack.append(("!" if n == "not" else "-") + str(v))
        elif n == "set":
            src = operand(a[0]); dst = operand(a[1]); emit(f"{dst} = {src}"); stack.append(str(dst))
        elif n == "copy":
            src = operand(a[0]); dst = operand(a[1]); emit(f"copy {a[2]} values {src} -> {dst}")
        elif n in ("callstd", "op35", "op36"):
            argc = a[1]; args = [popv() for _ in range(argc)][::-1]
            if n == "callstd":
                idx = args[0] if args else "?"
                name = natives.get(int(idx), f"native{idx}") if str(idx).lstrip("-").isdigit() else f"native[{idx}]"
                call = f"{name}({', '.join(map(str, args[1:]))})"
                note = annotate(name, args[1:]) if annotate else ""
                if note: call += f"  ; {note}"
            else:
                call = f"{n}<{a[0]:#x}>({', '.join(map(str, args))})"
            if stack and stack[-1] in ("L0",): stack.pop()  # the caller's handle pushed before every call
            stack.append(call)
        elif n == "call":
            # Script calls use the natives' convention (likely, from the code shapes): push 0 (the result slot) and
            # the caller's handle L0, then the arguments; the value lands in the result slot, and a following
            # "pop" drops it when unused. A call with no result slot below it (argc 0) is a plain statement: the old
            # "emit on pop" lost those (found, 2026-10-16: S1_out's Willie battle call).
            f, argc, nloc = a; args = [popv() for _ in range(argc)][::-1]
            handle = bool(args) and args[0] == "L0"  # the caller's handle: only then is a 0 below it the result slot
            if handle: args = args[1:]
            # A call with no arguments takes its locals' starting values from the stack: the caller pushes them just
            # before the call (verified 2026-10-17: D4_casino_colo pushes 431, 432, 2164 before func16, which reads
            # L0 / L1 as battle numbers and L2 as a story flag). Shown only when all of them are plain numbers.
            init = stack[-nloc:] if argc == 0 and 0 < nloc <= len(stack) else []
            if init and all(str(v).lstrip("-").isdigit() for v in init):
                del stack[-nloc:]
                args = ["locals: " + ", ".join(map(str, init))]
            call = f"func{f}({', '.join(map(str, args))})"
            if handle and stack and stack[-1] == "0": stack[-1] = call
            else: emit(call)
        elif n == "pop":
            for _ in range(a[0]):
                if stack: emit(str(stack.pop()))
        elif n == "jump_false": emit(f"if !({popv()}) goto L_{a[0]:04x}")
        elif n == "jump": emit(f"goto L_{a[0]:04x}")
        elif n == "return": emit("return" + (f" {stack.pop()}" if stack and not str(stack[-1]).startswith("L") else "")); stack = []
        elif n == "exit": emit("exit"); stack = []
        else: emit(f"{n} {a}")
    return hdr, lines


def native_table(read, count=247):
    """{index: (function address, [8 argument-type bytes])} from lbl_802E1CF0 (read(addr, n) -> bytes)."""
    out = {}
    for i in range(count):
        r = read(0x802E1CF0 + 12 * i, 12)
        fn = struct.unpack(">I", r[:4])[0]
        if fn: out[i] = (fn, list(r[4:12]))
    return out
