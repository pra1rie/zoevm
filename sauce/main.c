#include <stdio.h>
#define ZOE_IMPLEMENTATION
#include "zoe.h"

#ifndef _MIN_ARRAY_SIZE
#define _MIN_ARRAY_SIZE 512
#endif

static void _print_val(zoe_value val, int escape) {
    switch (ZOE_TYPE(val)) {
    default: printf("nil"); break;
    case ZOE_INT: printf("%ld", val.i >> 1); break;
    case ZOE_REAL:
        val.i >>= 3;
        printf("%.2f", val.f); break;
    case ZOE_PTR: {
        switch (val.p->type) {
        case ZOE_OBJ_STR:
            if (escape) printf("\"%.*s\"", val.p->size, val.p->as_str);
            else printf("%.*s", val.p->size, val.p->as_str);
            break;
        case ZOE_OBJ_ARR:
            printf("[");
            for (int j = 0; j < val.p->size; ++j) {
                if (j > 0) printf(", ");
                _print_val(val.p->as_arr[j], 1);
            }
            printf("]");
            break;
        default:
            printf("%p", val.p);
        }
    } break;
    }
}

static void extn_write(zoe_vm *vm, int argc, zoe_value *argv) {
    for (int i = 0; i < argc; ++i) _print_val(argv[i], 0);
}

static void extn_read(zoe_vm *vm, int argc, zoe_value *argv) {
    zoe_object *obj = malloc(sizeof(zoe_object));
    obj->type = ZOE_OBJ_STR;
    obj->size = 0;
    obj->cap = _MIN_ARRAY_SIZE;
    obj->as_str = malloc(obj->cap);
    int c = getc(stdin);
    while (c != EOF && c != 0 && c != '\n') {
        if (obj->size >= obj->cap) obj->as_str = realloc(obj->as_str, (obj->cap *= 1.5));
        obj->as_str[obj->size++] = c;
        c = getc(stdin);
    }
    zoe_value str = { .p = obj };
    vm_push(vm, str);
}

static const char *_obj_type_to_cstr[] = {
    [ZOE_OBJ_PTR] = "a pointer",
    [ZOE_OBJ_STR] = "a string",
    [ZOE_OBJ_ARR] = "an array",
};

static inline zoe_object *_get_object(int type, const char *fn_name, int arity, int argc, zoe_value *argv) {
    const char *argstr = arity == 1? "argument" : "arguments";
    if (argc != arity) ZOE_ERROR("%s: expected %d %s, got %d\n", fn_name, arity, argstr, argc);
    if (!ZOE_ALLOCATED(argv[0])) goto err;
    zoe_object *obj = argv[0].p;
    if (obj->type != type) goto err;
    return obj;
err:
    ZOE_ERROR("%s: expected %s\n", fn_name, _obj_type_to_cstr[type]);
}

static void extn_string_concat(zoe_vm *vm, int argc, zoe_value *argv) {
    zoe_object *s1 = _get_object(ZOE_OBJ_STR, "string_concat", 2, argc, argv);
    zoe_object *s2 = _get_object(ZOE_OBJ_STR, "string_concat", 2, argc, argv+1);
    if (s1->size + s2->size >= s1->cap)
        s1->as_str = realloc(s1->as_str, s1->cap += s1->size + s2->size);
    memmove(s1->as_str+s1->size, s2->as_str, s2->size);
    s1->size += s2->size;
    vm_push(vm, (zoe_value) { .p = s1 });
}

static void extn_string_length(zoe_vm *vm, int argc, zoe_value *argv) {
    zoe_object *obj = _get_object(ZOE_OBJ_STR, "string_length", 1, argc, argv);
    vm_push(vm, (zoe_value) {.i = (obj->size << 1) | ZOE_INT});
}

static void extn_string_at(zoe_vm *vm, int argc, zoe_value *argv) {
    zoe_object *obj = _get_object(ZOE_OBJ_STR, "string_at", 2, argc, argv);
    if (ZOE_TYPE(argv[1]) != ZOE_INT) ZOE_ERROR("string_at: expected an integer\n");
    int index = argv[1].i >> 1;
    if (index < 0 || index >= obj->size) ZOE_ERROR("string_at: index out of range\n");
    vm_push(vm, (zoe_value) {.i = (obj->as_str[index] << 1) | ZOE_INT});
}

static void extn_array_create(zoe_vm *vm, int argc, zoe_value *argv) {
    zoe_object *obj = malloc(sizeof(zoe_object));
    obj->type = ZOE_OBJ_ARR;
    obj->size = argc;
    obj->cap = argc == 0? _MIN_ARRAY_SIZE : argc;
    obj->as_arr = malloc(obj->cap * sizeof(zoe_value));
    memmove(obj->as_arr, argv, obj->cap * sizeof(zoe_value));
    vm_push(vm, (zoe_value) { .p = obj });
}

static void extn_array_push(zoe_vm *vm, int argc, zoe_value *argv) {
    zoe_object *obj = _get_object(ZOE_OBJ_ARR, "array_push", 2, argc, argv);
    if (obj->size >= obj->cap)
        obj->as_arr = realloc(obj->as_arr, (obj->cap *= 1.5) * sizeof(zoe_value));
    obj->as_arr[obj->size++] = argv[1];
}

static void extn_array_pop(zoe_vm *vm, int argc, zoe_value *argv) {
    zoe_object *obj = _get_object(ZOE_OBJ_ARR, "array_pop", 1, argc, argv);
    if (obj->size) --obj->size;
}

static void extn_array_length(zoe_vm *vm, int argc, zoe_value *argv) {
    zoe_object *obj = _get_object(ZOE_OBJ_ARR, "array_length", 1, argc, argv);
    vm_push(vm, (zoe_value) { .i = (obj->size << 1) | ZOE_INT });
}

static void extn_array_at(zoe_vm *vm, int argc, zoe_value *argv) {
    zoe_object *obj = _get_object(ZOE_OBJ_ARR, "array_at", 2, argc, argv);
    if (ZOE_TYPE(argv[1]) != ZOE_INT) ZOE_ERROR("array_at: expected an integer\n");
    int index = argv[1].i >> 1;
    if (index < 0 || index >= obj->size) ZOE_ERROR("array_at: index out of range\n");
    vm_push(vm, obj->as_arr[index]);
}

#ifdef _DEBUG
static const char *const _inst_to_str[NUM_ZOE_INSTRUCTIONS] = {
    [ZOE_INST_NOP] = "NOP",
    [ZOE_INST_PUSH] = "PUSH",
    [ZOE_INST_POP] = "POP",
    [ZOE_INST_DUP] = "DUP",
    [ZOE_INST_SWAP] = "SWAP",
    [ZOE_INST_LOAD] = "LOAD",
    [ZOE_INST_STORE] = "STORE",
    [ZOE_INST_CALL] = "CALL",
    [ZOE_INST_JUMP] = "JUMP",
    [ZOE_INST_JUMPIF] = "JUMPIF",
    [ZOE_INST_EXTERN] = "EXTERN",
    [ZOE_INST_BINARY] = "BINARY",
    [ZOE_INST_RETURN] = "RETURN",
    [ZOE_INST_HALT] = "HALT",
};

static const char *const _type_to_str[] = { "ptr", "int", "str", };
#endif

int main(int argc, char **argv) {
    if (argc < 2) ZOE_ERROR("missing input file\n");
    zoe_vm *vm = vm_init();
    vm_load_function(vm, extn_write);
    vm_load_function(vm, extn_read);
    vm_load_function(vm, extn_string_concat);
    vm_load_function(vm, extn_string_length);
    vm_load_function(vm, extn_string_at);
    vm_load_function(vm, extn_array_create);
    vm_load_function(vm, extn_array_push);
    vm_load_function(vm, extn_array_pop);
    vm_load_function(vm, extn_array_length);
    vm_load_function(vm, extn_array_at);
    vm_load_bytecode(vm, argv[1]);
#ifdef _DEBUG
    for (int i = 0; i < vm->code_sz; ++i) {
        if (vm->code[i].type == ZOE_INST_PUSH) {
            printf("%02d %-8s :: { %s: %ld; %p }\n",
                    i, _inst_to_str[vm->code[i].type],
                    _type_to_str[ZOE_TYPE(vm->code[i].value)],
                    vm->code[i].value.i >> 1, vm->code[i].value.p);
        } else {
            printf("%02d %-8s :: { %ld; %p }\n",
                    i, _inst_to_str[vm->code[i].type],
                    vm->code[i].value.i, vm->code[i].value.p);
        }
    }
#endif
    vm_execute(vm);
    // vm_free(vm);
    return 0;
}
