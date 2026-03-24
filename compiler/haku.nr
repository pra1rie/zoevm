#!/usr/bin/env nero

def enum(list) {
    dict = {}
    for elem, i = list { dict[elem] = i }
    return dict
}

def merge(list, val) {
    if typeof(val) == 'list' {
        for v = val { push(list, v) }
    } else {
        push(list, val)
    }
}

def escape_string(str) {
    res = ''
    i = -1
    while (i = i + 1) < len(str) {
        if str[i] == '\\' {
            i = i + 1
            if str[i] == '\\' { push(res, '\\') }
            elif str[i] == 'n' { push(res, '\n') }
            elif str[i] == 'r' { push(res, '\r') }
            elif str[i] == 't' { push(res, '\t') }
            elif str[i] == '0' { push(res, chr(0)) }
            elif str[i] == '\'' { push(res, '\'') }
            elif str[i] == '\"' { push(res, '\"') }
            else {
                echo("error: invalid string escape: \\", str[i])
                exit(1)
            }
        } else {
            push(res, str[i])
        }
    }
    return res
}

def is_space(c) { contains(['', ' ', '\r', '\n', '\t'], c) }
def is_digit(c) { ord(c) >= ord('0') && ord(c) <= ord('9') }
def is_number(s) {
    ss = if len(s) > 1 && (s[0] == '-' || s[0] == '+') { range(s, 1, 0) } else { s }
    for c = ss { if !is_digit(c) { return false } else { true } }
}

def string_to_bytes(str) {
    bytes = []
    for char = str { push(bytes, ord(char)) }
    return bytes
}

def bytes_to_string(bytes) {
    str = ""
    for byte = bytes { push(str, chr(byte)) }
    return str
}

def number_to_bytes(num, size) {
    bytes = []
    for i = size {
        push(bytes, (num & 0xff))
        num = num >> 8
    }
    return bytes
}

TYPES = {"NIL" = 0, "PTR" = 0x0, "INT" = 0x1, "REAL" = 0x2}
def as_int(v) { (v << 1) | TYPES.INT }
def as_flo(v) { (v << 3) | TYPES.REAL }
def as_ptr(v) { (v << 3) | TYPES.PTR }

INSTRUCTIONS = enum([
    "NOP", "PUSH", "POP", "DUP", "SWAP", "NOT",
    "LOAD", "STORE", "CALL", "JUMP", "JUMPIF",
    "EXTERN", "BINARY", "RETURN", "HALT",
])

BINARY_OPERATORS = enum([
    "EQ", "NEQ", "LT", "GT", "LEQ", "GEQ",
    "ADD", "SUB", "MUL", "DIV", "MOD",
    "XOR", "AND", "OR", "SHL", "SHR",
    "BAND", "BOR",
])

STRING_OPERATORS = {
    "==" = "EQ", "!=" = "NEQ", "<" = "LT", ">" = "GT",
    "<=" = "LEQ", ">=" = "GEQ", "+" = "ADD", "-" = "SUB",
    "*" = "MUL", "/" = "DIV", "%" = "MOD",
    "^" = "XOR", "&" = "AND", "|" = "OR",
    "<<" = "SHL", ">>" = "SHR",
    "and" = "BAND", "or" = "BOR",
}

BUILTIN_FUNCTIONS = enum([
    "WRITE", "READ", "STRING_CONCAT", "STRING_LENGTH", "STRING_AT",
    "ARRAY_CREATE", "ARRAY_PUSH", "ARRAY_POP", "ARRAY_LENGTH", "ARRAY_AT",
])

def compile_instruction(inst) {
    bytes = []
    merge(bytes, number_to_bytes(inst.type, 8))
    if contains(keys(inst), 'value') {
        merge(bytes, number_to_bytes(inst.value, 8))
    } else {
        merge(bytes, number_to_bytes(0, 8))
    }
    return bytes
}

def compile_bytecode(bytecode) {
    bytes = string_to_bytes('.ZOE')
    bytes = merge(bytes, number_to_bytes(len(bytecode.insts), 4))
    bytes = merge(bytes, number_to_bytes(len(bytecode.strs), 4))
    for inst = bytecode.insts {
        bytes = merge(bytes, compile_instruction(inst))
    }
    for str = bytecode.strs {
        bytes = merge(bytes, string_to_bytes(str))
        bytes = merge(bytes, 0)
    }
    return bytes_to_string(bytes)
}

def tokenize(text) {
    toks = []
    tok = ''
    is_string = is_comment = false
    for char = text {
        if char == '#' && !is_string {
            if tok { push(toks, tok) }
            tok = ''
            is_comment = true
        }
        if is_comment {
            if char == '\n' { is_comment = false }
            next
        }
        if is_space(char) && !is_string {
            if tok { push(toks, tok) }
            tok = ''
            next
        }
        if char == '\"' {
            if is_string && tok {
                if tok[len(tok)-1] == '\\' {
                    push(tok, '\"')
                    next
                }
            }
            is_string = !is_string
            if tok {
                # make sure strings are quoted
                push(toks, if !is_string { string('\"', tok, '\"') } else { tok })
                tok = ''
                next
            }
            next
        }
        push(tok, char)
    }
    return if tok { push(toks, tok) } else { toks }
}

def compile(toks) {
    out = { insts = [], strs = ['\n'] }
    vars = {}
    labels = {}
    funs = {}
    fun_info = nil
    nvars = i = -1 # TODO: figure out when to reuse old variables
    while (i = i + 1) < len(toks) {
        if is_number(toks[i]) {
            push(out.insts, { type = INSTRUCTIONS.PUSH, value = as_int(number(toks[i])) })
        } elif toks[i][0] == '\"' {
            push(out.strs, escape_string(range(toks[i], 1, -1)))
            push(out.insts, { type = INSTRUCTIONS.PUSH, value = as_ptr(len(out.strs)-1) })
        } elif contains(keys(STRING_OPERATORS), toks[i]) {
            push(out.insts, {
                type = INSTRUCTIONS.BINARY,
                value = BINARY_OPERATORS[STRING_OPERATORS[toks[i]]]
            })
        } elif toks[i][0] == '!' {
            varname = range(toks[i], 1, len(toks[i]))
            var = if contains(keys(vars), varname) {
                vars[varname]
            } else {
                vars[varname] = (nvars = nvars + 1)
            }
            push(out.insts, { type = INSTRUCTIONS.STORE, value = var })
        } elif toks[i][0] == '@' {
            varname = range(toks[i], 1, len(toks[i]))
            if !contains(keys(vars), varname) {
                echo('error: unknown variable: ', varname)
                exit(1)
            }
            push(out.insts, { type = INSTRUCTIONS.LOAD, value = vars[varname] })
        } elif toks[i][0] == ':' {
            label = range(toks[i], 1, len(toks[i])-1)
            labels[label] = len(out.insts)
        } elif toks[i] == 'pop' {
            push(out.insts, { type = INSTRUCTIONS.POP })
        } elif toks[i] == 'nop' {
            push(out.insts, { type = INSTRUCTIONS.NOP })
        } elif toks[i] == 'halt' {
            push(out.insts, { type = INSTRUCTIONS.HALT })
        } elif toks[i] == 'dup' {
            push(out.insts, { type = INSTRUCTIONS.DUP })
        } elif toks[i] == 'swap' {
            push(out.insts, { type = INSTRUCTIONS.SWAP })
        } elif toks[i] == 'not' {
            push(out.insts, { type = INSTRUCTIONS.NOT })
        } elif toks[i] == 'jump' || toks[i] == 'jumpif' {
            is_jumpif = (toks[i] == 'jumpif')
            i = i + 1
            label = toks[i]
            push(out.insts, {
                type = if is_jumpif { INSTRUCTIONS.JUMPIF } else { INSTRUCTIONS.JUMP },
                value = label
            })
        } elif contains(keys(BUILTIN_FUNCTIONS), toks[i]) {
            fn = toks[i]
            i = i + 1
            if !is_number(toks[i]) {
                echo('error: \'', fn, '\' expected number of arguments, got: ', toks[i])
                exit(1)
            }
            push(out.insts, { type = INSTRUCTIONS.PUSH, value = as_int(BUILTIN_FUNCTIONS[fn]) })
            push(out.insts, { type = INSTRUCTIONS.EXTERN, value = number(toks[i]) })
        } elif toks[i] == 'def' {
            i = i + 1
            fun_info = { name = toks[i], pos = len(out.insts) }
            funs[fun_info.name] = fun_info.pos
            push(out.insts, { type = INSTRUCTIONS.JUMP, value = -1 })
        } elif toks[i] == 'ret' {
            if !fun_info {
                echo('error: unexpected \'ret\'')
                exit(1)
            }
            push(out.insts, { type = INSTRUCTIONS.RETURN })
            out.insts[fun_info.pos].value = len(out.insts)
            fun_info = nil
        } elif toks[i] == 'call' {
            i = i + 1
            if !contains(keys(funs), toks[i]) {
                echo('error: unknown function: ', toks[i])
                exit(1)
            }
            fun = funs[toks[i]] + 1
            push(out.insts, { type = INSTRUCTIONS.CALL, value = fun })
        } else {
            echo('error: unknown instruction: ', toks[i])
            exit(1)
        }
    }
    for inst = out.insts {
        if inst.type != INSTRUCTIONS.JUMP && inst.type != INSTRUCTIONS.JUMPIF { next }
        if typeof(inst.value) == 'int' { next } # skip already patched/auto generated jumps
        if !contains(keys(labels), inst.value) {
            echo('error: unknown label: ', inst.value)
            exit(1)
        }
        inst.value = labels[inst.value]
    }
    return out
}

def main(args) {
    if len(args) < 2 {
        echo("usage: ", args[0], " [-r|-o output] <input>")
        exit(1)
    }

    run_flag = false
    out_file = in_file = nil
    i = -1
    while (i = i + 1) < len(args) {
        if args[i] == '-o' && i+1 < len(args) {
            out_file = args[i+1]
            i = i + 1
        } elif args[i] == '-r' {
            run_flag = true
        } else {
            in_file = args[i]
        }
    }

    if in_file == nil {
        echo("error: missing input file")
        exit(1)
    }

    if out_file == nil { out_file = string(in_file, '.zoe') }
    text = read_file(in_file)
    code = tokenize(text)
    bytecode = compile(code)
    out = compile_bytecode(bytecode)
    write_file(out_file, out)
    if run_flag {
        if system(string('./zoe 2> /dev/null ', out_file)) != 0 {
            echo("error: failed to execute zoe!")
            exit(1)
        }
    } else {
        echo("written ", len(out), " bytes to file ", out_file)
    }
}

main(arguments())
