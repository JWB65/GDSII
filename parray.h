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

// A dynamic array of void pointers

#include <inttypes.h>

typedef struct parray parray;

void parray_create(parray** v);

void parray_release(parray* v);

void parray_add(parray*, void*);

void* parray_get(parray*, uint64_t);

uint64_t parray_size(parray*);
