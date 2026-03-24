#ifndef ZOEVM_H
#define ZOEVM_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
// TODO: implement my own mark and sweep gc
#include <gc.h>

#define ZOE_ERROR(...) do { fprintf(stderr, "error: "__VA_ARGS__); exit(1); } while(0)

#ifndef ZOE_STACK_MAX
#define ZOE_STACK_MAX 1024
#endif
#ifndef ZOE_VARS_MAX
#define ZOE_VARS_MAX 1024
#endif
#ifndef ZOE_FUNS_MAX
#define ZOE_FUNS_MAX 1024
#endif
#ifndef ZOE_OBJS_MAX
#define ZOE_OBJS_MAX 1024
#endif

typedef union zoe_value zoe_value;
typedef struct zoe_object zoe_object;
typedef struct zoe_vm zoe_vm;
typedef void(*zoe_foreign)(zoe_vm*, int, zoe_value*);

enum { ZOE_OBJ_PTR, ZOE_OBJ_STR, ZOE_OBJ_ARR };
struct zoe_object {
    int type, size, cap; //, mark;
    union {
        void *as_ptr;
        char *as_str;
        zoe_value *as_arr;
    };
};

union zoe_value { int64_t i; float f; zoe_object *p; };
enum { ZOE_NIL = 0, ZOE_PTR = 0b000, ZOE_INT = 1,  ZOE_REAL = 0b010 };
#define ZOE_TYPE(V) (((V).i & 0b1)? ZOE_INT : ((V).i == ZOE_NIL? ZOE_NIL : (V).i & 0b111))
#define ZOE_ALLOCATED(V) ((V).i != ZOE_NIL && ZOE_TYPE(V) == ZOE_PTR)

enum {
    ZOE_INST_NOP = 0,
    ZOE_INST_PUSH,    // push into stack
    ZOE_INST_POP,     // pop from stack
    ZOE_INST_DUP,     // duplicate value from stack
    ZOE_INST_SWAP,    // swap 2 values on stack
    ZOE_INST_NOT,     // boolean not on top of stack
    ZOE_INST_LOAD,    // push variable onto stack
    ZOE_INST_STORE,   // pop from stack onto variable
    ZOE_INST_CALL,    // jump to subroutine
    ZOE_INST_JUMP,    // unconditional jump
    ZOE_INST_JUMPIF,  // jump if not zero
    ZOE_INST_EXTERN,  // call external C function
    ZOE_INST_BINARY,  // binary operation
    ZOE_INST_RETURN,  // return from subroutine
    ZOE_INST_HALT,
    NUM_ZOE_INSTRUCTIONS,
};

enum {
    ZOE_BIN_EQ, ZOE_BIN_NEQ, ZOE_BIN_LT, ZOE_BIN_GT,
    ZOE_BIN_LEQ, ZOE_BIN_GEQ, ZOE_BIN_ADD, ZOE_BIN_SUB,
    ZOE_BIN_MUL, ZOE_BIN_DIV, ZOE_BIN_MOD,
    ZOE_BIN_AND, ZOE_BIN_OR, ZOE_BIN_XOR,
    ZOE_BIN_SHL, ZOE_BIN_SHR,
    ZOE_BIN_BAND, ZOE_BIN_BOR,
    NUM_ZOE_BINARY_OPS,
};

typedef struct {
    uint8_t type;
    zoe_value value;
} zoe_inst;

typedef struct {
    char header[4];
    uint32_t num_insts, num_strs;
    zoe_inst *insts;
    char **strs;
} zoe_bytecode;

struct zoe_vm {
    uint32_t ip, sp, cp, halt, code_sz, num_funs;
    zoe_value stack[ZOE_STACK_MAX];
    zoe_value vars[ZOE_VARS_MAX];
    uint32_t call_stack[ZOE_STACK_MAX];
    zoe_foreign funs[ZOE_FUNS_MAX];
    zoe_inst *code;
};

zoe_vm *vm_init(void);
void vm_free(zoe_vm *vm);
void vm_execute(zoe_vm *vm);
void vm_load_function(zoe_vm *vm, zoe_foreign fn);
void vm_load_bytecode(zoe_vm *vm, const char *path);

void vm_push(zoe_vm *vm, zoe_value val);
zoe_value vm_pop(zoe_vm *vm);
void vm_dup(zoe_vm *vm);
void vm_swap(zoe_vm *vm);
void vm_not(zoe_vm *vm);
zoe_value vm_load(zoe_vm *vm, uint32_t addr);
void vm_store(zoe_vm *vm, uint32_t addr, zoe_value val);
void vm_call(zoe_vm *vm, uint32_t addr);
void vm_jump(zoe_vm *vm, uint32_t addr);
int vm_jumpif(zoe_vm *vm, uint32_t addr);
void vm_extern(zoe_vm *vm, uint32_t arity);
void vm_binary(zoe_vm *vm, uint32_t op);
void vm_return(zoe_vm *vm);

void obj_free(zoe_object *obj);

#ifdef ZOE_IMPLEMENTATION

zoe_vm *vm_init(void) {
    zoe_vm *vm = malloc(sizeof(zoe_vm));
    memset(vm, 0, sizeof(*vm));
    return vm;
}

void vm_free(zoe_vm *vm) {
    for (int i = 0; i < ZOE_VARS_MAX; ++i) {
        if (ZOE_ALLOCATED(vm->vars[i]))
            obj_free(vm->vars[i].p);
    }
    for (int i = 0; i < vm->sp; ++i) {
        if (ZOE_ALLOCATED(vm->stack[i]))
            obj_free(vm->stack[i].p);
    }
    free(vm);
}

void vm_push(zoe_vm *vm, zoe_value val) {
    if (vm->sp >= ZOE_STACK_MAX) ZOE_ERROR("stack overflow\n");
    vm->stack[vm->sp++] = val;
}

zoe_value vm_pop(zoe_vm *vm) {
    if (vm->sp == 0) ZOE_ERROR("stack underflow\n");
    return vm->stack[--vm->sp];
}

void vm_dup(zoe_vm *vm) {
    // TODO: maybe duplicate the object as well
    zoe_value val = vm_pop(vm);
    vm_push(vm, val);
    vm_push(vm, val);
}

void vm_swap(zoe_vm *vm) {
    if (vm->sp < 2) ZOE_ERROR("stack underflow\n");
    zoe_value v = vm->stack[vm->sp-1];
    vm->stack[vm->sp-1] = vm->stack[vm->sp-2];
    vm->stack[vm->sp-2] = v;
}

zoe_value vm_load(zoe_vm *vm, uint32_t addr) {
    if (addr >= ZOE_VARS_MAX) ZOE_ERROR("invalid load addr\n");
    return vm->vars[addr];
}

void vm_store(zoe_vm *vm, uint32_t addr, zoe_value val) {
    if (addr >= ZOE_VARS_MAX) ZOE_ERROR("invalid store addr\n");
    vm->vars[addr] = val;
}

void vm_call(zoe_vm *vm, uint32_t addr) {
    if (vm->cp >= ZOE_STACK_MAX) ZOE_ERROR("call stack overflow\n");
    vm->call_stack[vm->cp++] = vm->ip;
    vm_jump(vm, addr);
}

void vm_jump(zoe_vm *vm, uint32_t addr) {
    if (addr >= vm->code_sz) ZOE_ERROR("invalid jump addr\n");
    vm->ip = addr;
}

int vm_jumpif(zoe_vm *vm, uint32_t addr) {
    zoe_value val = vm_pop(vm);
    // int 0 (0b...001) and ptr 0 (0b...000)
    if (val.i != 1 && val.i != 0) {
        vm_jump(vm, addr);
        return 1;
    }
    return 0;
}

void vm_extern(zoe_vm *vm, uint32_t arity) {
    zoe_value val = vm_pop(vm);
    uint64_t addr = val.i >> 1;
    if (ZOE_TYPE(val) != ZOE_INT || addr >= vm->num_funs) ZOE_ERROR("invalid function addr\n");
    if (vm->sp < arity) ZOE_ERROR("not enough arguments\n");
    zoe_value args[arity];
    memcpy(args, vm->stack+vm->sp-arity, arity*sizeof(zoe_value));
    vm->sp -= arity;
    vm->funs[addr](vm, arity, args);
}

static int _compare(zoe_value a, zoe_value b);

static int _compare_objs(zoe_object *a, zoe_object *b) {
    if (a->type != b->type) return 0;
    switch (a->type) {
    default: return (a->as_ptr == b->as_ptr);
    case ZOE_OBJ_STR:
        if (a->size != b->size) return 0;
        return strncmp(a->as_str, b->as_str, a->size) == 0;
    case ZOE_OBJ_ARR:
        if (a->size != b->size) return 0;
        for (int i = 0; i < a->size; ++i)
            if (!_compare(a->as_arr[i], b->as_arr[i])) return 0;
        return 1;
    }
}

static int _compare(zoe_value a, zoe_value b) {
    if (ZOE_TYPE(a) != ZOE_TYPE(b)) return 0;
    switch (ZOE_TYPE(a)) {
    default: return 1;
    case ZOE_INT: return a.i == b.i;
    case ZOE_REAL:
        a.i >>= 3, b.i >>= 3;
        return a.f == b.f;
    case ZOE_PTR:
        return _compare_objs(a.p, b.p);
    }
}

void vm_binary(zoe_vm *vm, uint32_t op) {
    zoe_value left = vm_pop(vm);
    zoe_value right = vm_pop(vm);
    zoe_value res = {.i = ZOE_INT};
    if (op == ZOE_BIN_EQ) {
        res.i = (_compare(left, right) << 1) | ZOE_INT;
        return vm_push(vm, res);
    } else if (op == ZOE_BIN_NEQ) {
        res.i = (!_compare(left, right) << 1) | ZOE_INT;
        return vm_push(vm, res);
    }
    int is_real = 0;
    if (ZOE_TYPE(left) == ZOE_REAL || ZOE_TYPE(right) == ZOE_REAL) is_real = 1;
    if (!is_real && (ZOE_TYPE(left) != ZOE_INT || ZOE_TYPE(right) != ZOE_INT))
        ZOE_ERROR("binary operation expects 2 integers\n");
    int64_t ia, ib;
    float fa, fb;
    if (is_real) {
        if (ZOE_TYPE(left) == ZOE_INT) { left.i >>= 1; fb = (float)left.i; }
        else { left.i >>= 3; fb = left.f; }
        if (ZOE_TYPE(right) == ZOE_INT) { right.i >>= 1; fa = (float)right.i; }
        else { right.i >>= 3; fa = right.f; }
    } else {
        if (ZOE_TYPE(left) == ZOE_INT) { left.i >>= 1; ib = left.i; }
        else { left.i >>= 3; ib = (int64_t)left.f; }
        if (ZOE_TYPE(right) == ZOE_INT) { right.i >>= 1; ia = right.i; }
        else { right.i >>= 3; ia = (int64_t)right.f; }
    }
    switch (op) {
    default: ZOE_ERROR("unknown operation\n");
    case ZOE_BIN_LT:  res.i = is_real? fa < fb : ia < ib; is_real = 0; break;
    case ZOE_BIN_GT:  res.i = is_real? fa > fb : ia > ib; is_real = 0; break;
    case ZOE_BIN_LEQ: res.i = is_real? fa <= fb : ia <= ib; is_real = 0; break;
    case ZOE_BIN_GEQ: res.i = is_real? fa >= fb : ia >= ib; is_real = 0; break;
    case ZOE_BIN_ADD: if (is_real) res.f = fa + fb; else res.i = ia + ib; break;
    case ZOE_BIN_SUB: if (is_real) res.f = fa + fb; else res.i = ia - ib; break;
    case ZOE_BIN_MUL: if (is_real) res.f = fa + fb; else res.i = ia * ib; break;
    case ZOE_BIN_DIV:
        if (is_real? fb == 0 : ib == 0) ZOE_ERROR("division by zero\n");
        if (is_real) res.f = fa / fb; else res.i = ia / ib; break;
    case ZOE_BIN_MOD:
        if (is_real) ZOE_ERROR("binary operation expects 2 integers\n");
        if (ib == 0) ZOE_ERROR("division by zero\n");
        res.i = ia % ib; break;
    case ZOE_BIN_AND:
        if (is_real) ZOE_ERROR("binary operation expects 2 integers\n");
        res.i = ia & ib; break;
    case ZOE_BIN_OR:
        if (is_real) ZOE_ERROR("binary operation expects 2 integers\n");
        res.i = ia | ib; break;
    case ZOE_BIN_XOR:
        if (is_real) ZOE_ERROR("binary operation expects 2 integers\n");
        res.i = ia ^ ib; break;
    case ZOE_BIN_SHL:
        if (is_real) ZOE_ERROR("binary operation expects 2 integers\n");
        res.i = ia << ib; break;
    case ZOE_BIN_SHR:
        if (is_real) ZOE_ERROR("binary operation expects 2 integers\n");
        res.i = ia >> ib; break;
    case ZOE_BIN_BAND:
        if (is_real) ZOE_ERROR("binary operation expects 2 integers\n");
        res.i = ia && ib; break;
    case ZOE_BIN_BOR:
        if (is_real) ZOE_ERROR("binary operation expects 2 integers\n");
        res.i = ia || ib; break;
    }
    if (is_real) res.i = (res.i << 3) | ZOE_REAL;
    else res.i = (res.i << 1) | ZOE_INT;
    vm_push(vm, res);
}

void vm_not(zoe_vm *vm) {
    zoe_value val = vm_pop(vm);
    if (ZOE_TYPE(val) != ZOE_INT) ZOE_ERROR("not expects an integer\n");
    zoe_value res = {.i = (!(val.i >> 1)) << 1 | ZOE_INT};
    vm_push(vm, res);
}

void vm_return(zoe_vm *vm) {
    if (vm->cp == 0) ZOE_ERROR("call stack underflow\n");
    vm->ip = vm->call_stack[--vm->cp];
}

static int _exec_instruction(zoe_vm *vm, const zoe_inst inst) {
    switch (inst.type) {
    default: break;
    case ZOE_INST_HALT:
        vm->halt = 1;
        return 1;
    case ZOE_INST_PUSH:
        vm_push(vm, inst.value);
        break;
    case ZOE_INST_POP:
        vm_pop(vm);
        break;
    case ZOE_INST_DUP:
        vm_dup(vm);
        break;
    case ZOE_INST_SWAP:
        vm_swap(vm);
        break;
    case ZOE_INST_NOT:
        vm_not(vm);
        break;
    case ZOE_INST_LOAD:
        vm_push(vm, vm_load(vm, inst.value.i));
        break;
    case ZOE_INST_STORE:
        vm_store(vm, inst.value.i, vm_pop(vm));
        break;
    case ZOE_INST_CALL:
        vm_call(vm, inst.value.i);
        return 1;
    case ZOE_INST_JUMP:
        vm_jump(vm, inst.value.i);
        return 1;
    case ZOE_INST_JUMPIF:
        return vm_jumpif(vm, inst.value.i);
    case ZOE_INST_EXTERN:
        vm_extern(vm, inst.value.i);
        break;
    case ZOE_INST_BINARY:
        vm_binary(vm, inst.value.i);
        break;
    case ZOE_INST_RETURN:
        vm_return(vm);
        break;
    }
    return 0;
}

void vm_execute(zoe_vm *vm) {
    while (vm->ip < vm->code_sz && !vm->halt) {
        if (!_exec_instruction(vm, vm->code[vm->ip]))
            ++vm->ip;
    }
}

void vm_load_function(zoe_vm *vm, zoe_foreign fn) {
    if (vm->num_funs >= ZOE_FUNS_MAX) ZOE_ERROR("too many foreign functions loaded\n");
    vm->funs[vm->num_funs++] = fn;
}

void vm_load_bytecode(zoe_vm *vm, const char *path) {
    zoe_bytecode bytecode = {0};
    FILE *file = fopen(path, "rb");
    if (!file) ZOE_ERROR("could not load bytecode from file '%s'\n", path);
    fread(bytecode.header, sizeof(char), 4, file);
    if (strncmp(bytecode.header, ".ZOE", 4) != 0)
        ZOE_ERROR("file '%s' is not valid zoe bytecode\n", path);
    fread(&bytecode.num_insts, sizeof(uint32_t), 1, file);
    fread(&bytecode.num_strs, sizeof(uint32_t), 1, file);
    bytecode.insts = malloc(bytecode.num_insts * sizeof(zoe_inst));
    if (!bytecode.insts) ZOE_ERROR("could not allocate instructions\n");
    bytecode.strs = bytecode.num_strs? malloc(bytecode.num_strs * sizeof(char*)) : NULL;
    // read all instructions
    fread(bytecode.insts, sizeof(zoe_inst), bytecode.num_insts, file);
    // allocate and read all strings
    for (int i = 0; i < bytecode.num_strs; ++i) {
        int cap = 1024, sz = 0;
        char *buf = malloc(cap * sizeof(char));
        for (;;) {
            if (sz >= cap) buf = realloc(buf, (cap *= 2) * sizeof(char));
            if ((buf[sz++] = fgetc(file)) <= 0) break;
        }
        bytecode.strs[i] = strndup(buf, sz-1);
        free(buf);
    }
    // backpatch instructions to point to the strings allocated above
    for (int i = 0; i < bytecode.num_insts; ++i) {
        if (bytecode.insts[i].type != ZOE_INST_PUSH) continue;
        if (ZOE_TYPE(bytecode.insts[i].value) != ZOE_PTR) continue;
        char *str = bytecode.strs[bytecode.insts[i].value.i >> 3];
        zoe_object *obj = malloc(sizeof(zoe_object));
        obj->type = ZOE_OBJ_STR;
        obj->size = obj->cap = strlen(str);
        obj->as_str = str;
        bytecode.insts[i].value.p = obj;
    }
    free(bytecode.strs); // free the array, the strings are still allocated!
    fclose(file);
    vm->code = bytecode.insts;
    vm->code_sz = bytecode.num_insts;
}

void obj_free(zoe_object *obj) {
    if (obj->type == ZOE_OBJ_STR) free(obj->as_str);
    else {
        for (int i = 0; i < obj->size; ++i) {
            if (ZOE_ALLOCATED(obj->as_arr[i]))
                obj_free(obj->as_arr[i].p);
        }
        free(obj->as_arr);
    }
    free(obj);
}

#endif

#endif

