/*
** A simple garbage collector.
**
** Thanks to:
** http://journal.stuffwithstuff.com/2013/12/08/babys-first-garbage-collector/
**
*/
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define GC_INITIAL_THRESHOLD 8
#define STACK_MAX 256  // maximum stack size for the virtual machine

/**
 * Object and Object Types
 */
typedef enum {
    OBJECT_TYPE_INT,
    OBJECT_TYPE_PAIR,
} ObjectType;

typedef struct sObject {
    // GC related
    unsigned char mark;  // zero: unreached, non-zero: retained
    struct sObject *next;  // the next object chained to it

    ObjectType type;
    union {
        int value;
        struct {
            struct sObject *head;
            struct sObject *tail;
        };
    };
} Object;

/**
 * VM
 */
typedef struct {
    // GC related
    Object *first_object;
    size_t object_num;
    size_t object_max;

    size_t stack_size;
    Object * stack[STACK_MAX];
} VM;

VM* new_vm(void) {
    VM *vm = malloc(sizeof(VM));
#ifndef NDEBUG
    assert("VM allocation failed" && vm);
#endif
    if (vm) {
        vm->stack_size = 0;
        vm->first_object = 0;
        vm->object_num = 0;
        vm->object_max = GC_INITIAL_THRESHOLD;
    }
    return vm;
}

void vm_push(VM *vm, Object *value) {
    assert("stack overflow" && vm->stack_size < STACK_MAX);
    vm->stack[vm->stack_size++] = value;
}

Object* vm_pop(VM *vm) {
    assert("stack underflow" && vm->stack_size > 0);
    return vm->stack[--vm->stack_size];
}

void gc(VM*);

Object* new_object(VM *vm, ObjectType type) {
    if (vm->object_num == vm->object_max) gc(vm);
    Object *obj = malloc(sizeof(Object));
#ifndef NDEBUG
    assert("Object allocation failed" && obj);
#endif
    if (obj) {
        obj->mark = 0;
        obj->type = type;
        obj->next = vm->first_object;
        vm->first_object = obj;
        ++vm->object_num;
    }
    return obj;
}

Object* vm_push_int(VM *vm, int value) {
    Object *obj = new_object(vm, OBJECT_TYPE_INT);
    if (obj) {
        obj->value = value;
        vm_push(vm, obj);
    }
    return obj;
}

Object* vm_push_pair(VM *vm) {
    Object *obj = new_object(vm, OBJECT_TYPE_PAIR);
    if (obj) {
        obj->tail = vm_pop(vm);
        obj->head = vm_pop(vm);
        vm_push(vm, obj);
    }
    return obj;
}

void free_vm(VM *vm) {
    vm->stack_size = 0;
    gc(vm);
    free(vm);
}

/**
 * GC
 */
void gc_mark(Object *obj) {
    if (obj->mark) return;  // for recursive reference
    obj->mark = 1;
    if (obj->type == OBJECT_TYPE_PAIR) {
        gc_mark(obj->head);
        gc_mark(obj->tail);
    }
}

void gc_mark_all(VM *vm) {
    for (size_t i = 0; i < vm->stack_size; ++i) {
        gc_mark(vm->stack[i]);
    }
}

void gc_sweep(VM *vm) {
    Object* *obj = &(vm->first_object);
    while (*obj) {
        if (!(*obj)->mark) {
            Object *unreached = *obj;
            // first_object / previous->next = unreached->next
            *obj = unreached->next;
            free(unreached);
            --vm->object_num;
        } else {
            (*obj)->mark = 0;
            obj = &(*obj)->next;
        }
    }
}

void gc(VM *vm) {
    if (vm->object_num == 0) return;
#ifndef NDEBUG
    size_t object_num = vm->object_num;
#endif
    gc_mark_all(vm);
    gc_sweep(vm);
    // adjust GC threshold
    vm->object_max = vm->object_num ? vm->object_num * 2 : GC_INITIAL_THRESHOLD;
#ifndef NDEBUG
    printf("%zu - %zu = %zu\n",
            object_num, object_num - vm->object_num, vm->object_num);
#endif
}

void test1(void) {
    puts("Test 1");
    VM *vm = new_vm();
    vm_push_int(vm, 1);
    vm_push_int(vm, 2);
    gc(vm);
    assert("GC should skip preserved objects" && vm->object_num == 2);
    free_vm(vm);
}

void test2(void) {
    puts("Test 2");
    VM *vm = new_vm();
    vm_push_int(vm, 1);
    vm_push_int(vm, 2);
    vm_pop(vm);
    vm_pop(vm);
    gc(vm);
    assert("GC should collect unreached objects" && vm->object_num == 0);
    free_vm(vm);
}

void test3(void) {
    puts("Test 3");
    VM *vm = new_vm();
    vm_push_int(vm, 1);
    vm_push_int(vm, 2);
    vm_push_pair(vm);
    vm_push_int(vm, 3);
    vm_push_int(vm, 4);
    vm_push_pair(vm);
    vm_push_pair(vm);
    gc(vm);
    assert("GC should reach nested objects" && vm->object_num == 7);
    free_vm(vm);
}

void test4(void) {
    puts("Test 4");
    VM *vm = new_vm();
    vm_push_int(vm, 1);
    vm_push_int(vm, 2);
    Object *a = vm_push_pair(vm);
    vm_push_int(vm, 3);
    vm_push_int(vm, 4);
    Object *b = vm_push_pair(vm);
    a->tail = b;
    b->tail = a;
    gc(vm);
    assert("GC should deal with recursive reference" && vm->object_num == 4);
    free_vm(vm);
}

void perf_test(void) {
    puts("Performance Test");
    VM *vm = new_vm();
    for (size_t i = 0; i < 1000; ++i) {
        size_t round = rand() % STACK_MAX + 1;
        for (size_t j = 0; j < round; ++j) {
            vm_push_int(vm, i + j);
        }
        for (size_t j = 0; j < round; ++j) {
            vm_pop(vm);
        }
    }
    free_vm(vm);
}

int main(void) {
    time_t seed = time(0);
    srand(seed);
    printf("Testing with a random seed: %zu\n\n", seed);

    test1();
    test2();
    test3();
    test4();
    perf_test();

    printf("\nTested with a random seed: %zu\n", seed);
    return EXIT_SUCCESS;
}
