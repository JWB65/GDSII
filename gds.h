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

#pragma once

#include "parray.h"
#include <inttypes.h>

/* Maximum number of characters in a cell name */
#define GDS_MAX_STR_NAME 127

/* Opaque handle to the GDS database structure */
typedef void* HGDS;

/* 
 * The user of the library needs to define a pointer array of gds_poly
 * structures defined below.
 */

typedef struct {
	int x, y;
} gds_ipair_t;

typedef struct {
	gds_ipair_t* pairs;
	uint16_t size;
	uint16_t layer;
} gds_poly_t;

/*
 * Creates a gds database structure from a file
 * 
 * @error and @elen: char array with a max length for an error message
 *
 * Return: GDS database handle or NULL if failed
 */
HGDS gds_db_create(const char* file, char* error, int elen);

/*
 * Releases the memory held by the GDS database handle
 */
void gds_db_release(HGDS hGds);

/*
 * Flattens the cell "cell" in gds_database structure "gds" and writes the
 * output to a pointer array "pvec"
 * 
 * @bounds: boundary box (xmin, ymin, dx, dy) of polygons to include or
 *		    NULL if the entire cell is to be included
 *
 * @callback: a callback function that can be used to update progress on during
 *            flattening. If it returns 1 flattening will terminate. Can be
 *            set NULL. The first parameter is the number of polygons added
 *            so far and the second parameter the number of polygons scanned.
 * 
 * @max_polys: Max number of polygons to output (to limit memory)
 * 
 * @error and @elen: char array with max length for an error message if flattening
 *                   failed
 * 
 * Return: 1 (success) 0 (failure)
 */
int gds_collapse(HGDS hGds, const char* cell, const double* bounds, uint64_t max_polys,
	parray* pvec, int (*callback)(uint64_t, uint64_t), char* error, int elen);

/*
 * Writes polyset "pset" to a GDS file
 * 
 * @error and @elen: char array with max length for an error message if there
 *					was a failure
 * 
 * Return: 1 (success) 0 (failure)
 */
int gds_write(HGDS hGds, const char* dest, parray* pvec, char* error, int elen);

/*
 * Checks if point @p is in polygon @poly. Polygon is closed with @n vertices.
 * The nth vertex is the same as the first.
 */
int gds_poly_contains_point(gds_ipair_t* poly, int n, gds_ipair_t p);

/*
 * Prints all top cells of the gds database structure
 */
void gds_top_cells(HGDS hGds);

/*
 * Prints all cells of the gds database structure
 */
void gds_all_cells(HGDS hGds);

/*
 * Support function to destroy all polygons pointed to by pointer array pset
 */
void gds_polyset_release(parray* pset);

/* Retrieve the GDS file used to create the given GDS database */
char* gds_getfile(HGDS hGds);

/* Retrieve the size of the db unit in user units */
float gds_getuu(HGDS hGds);
