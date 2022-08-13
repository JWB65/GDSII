#pragma once

/**
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
* 
* 
* Dynamic array of void pointers
* 
*/

#include <inttypes.h>

typedef struct vec {
    void** parray;
    uint64_t capacity;
    uint64_t size;
} vec;

vec* vec_new();
void vec_delete(vec*);

void vec_init(vec*);
uint64_t vec_size(vec*);
static void vec_resize(vec*, uint64_t);
void vec_add(vec*, void*);
void vec_set(vec*, uint64_t, void*);
void* vec_get(vec*, uint64_t);
void vec_remove(vec*, uint64_t);
void vec_free(vec*);