/**
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
*/

#include "parray.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct parray {
    void** parray;
    uint64_t capacity;
    uint64_t size;
} parray;

#define VEC_INITIAL_CAPACITY 4

void parray_create(parray** v)
{
    *v = malloc(sizeof(parray));
    (*v)->capacity = VEC_INITIAL_CAPACITY;
    (*v)->size = 0;
    (*v)->parray = calloc(1, sizeof(void*) * (*v)->capacity);
}

static void parray_resize(parray* v, uint64_t capacity)
{
    void** items = realloc(v->parray, sizeof(void*) * capacity);
    v->parray = items;
    v->capacity = capacity;
}

void parray_release(parray* v)
{
    free(v->parray);
    v->parray = NULL;
    v->capacity = 0;
    v->size = 0;

    free(v);
}

void parray_add(parray* v, void* item)
{
    if (v->capacity == v->size)
        parray_resize(v, v->capacity * 2);
    v->parray[v->size++] = item;
}

void* parray_get(parray* v, uint64_t index)
{
    return v->parray[index];
}

uint64_t parray_size(parray* v)
{
    return v->size;
}