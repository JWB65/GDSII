/*
 * Copyright (c) 2022 Jan Willem Bos - janwillembos@yahoo.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met :
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and /or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *	  software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "parray.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct parray {
    void** parray;
    uint64_t capacity;
    uint64_t size;
} parray;

#define VEC_INITIAL_CAPACITY 4

parray* parray_create(void)
{
    parray *v = malloc(sizeof(parray));

    v->capacity = VEC_INITIAL_CAPACITY;
    v->size = 0;
    v->parray = calloc(1, sizeof(void*) * v->capacity);

    return v;
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