/**
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
*/

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "vec.h"

#define VEC_INITIAL_CAPACITY 4

vec* vec_new()
{
    vec* v = calloc(1, sizeof(vec));
    vec_init(v);
    return v;
}

void vec_delete(vec* v)
{
    vec_free(v);
    free(v);
}

void vec_init(vec* v)
{
    v->capacity = VEC_INITIAL_CAPACITY;
    v->size = 0;
    v->parray = calloc(1, sizeof(void*) * v->capacity);
}

uint64_t vec_size(vec* v)
{
    return v->size;
}

static void vec_resize(vec* v, uint64_t capacity)
{
    void** items = realloc(v->parray, sizeof(void*) * capacity);
    if (items) {
        v->parray = items;
        v->capacity = capacity;
    }
}

void vec_add(vec* v, void* item)
{
    if (v->capacity == v->size)
        vec_resize(v, v->capacity * 2);
    v->parray[v->size++] = item;
}

void vec_set(vec* v, uint64_t index, void* item)
{
    if (index < v->size)
        v->parray[index] = item;
}

void* vec_get(vec* v, uint64_t index)
{
    return v->parray[index];
    //if (index < v->size)
    //    return v->parray[index];
    //return 0;
}

void vec_remove(vec* v, uint64_t index)
{
    uint64_t i;

    if (index >= v->size)
        return;

    v->parray[index] = 0;

    for (i = index; i < v->size - 1; i++) {
        v->parray[i] = v->parray[i + 1];
        v->parray[i + 1] = 0;
    }

    v->size--;

    if (v->size > 0 && v->size == v->capacity / 4)
        vec_resize(v, v->capacity / 2);
}

void vec_free(vec* v)
{
    free(v->parray);
    v->parray = 0;
    v->capacity = 0;
    v->size = 0;
}