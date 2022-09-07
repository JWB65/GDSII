/**
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
*
* Process module to work with GDSII file
*
*/

#pragma once

#include "parray.h"
#include <inttypes.h>

/* Handles to a GDS database structure */
typedef void* HGDS;

/**
* The gds_collapse function will output a pointer array of these polygon
* structures. It contains the coordinates of the polygon vertices and the
* layer id of the polygon in the GDS database.
*/
typedef struct gds_ipair {
	int x, y;
} gds_ipair;

typedef struct {
	gds_ipair* pairs;
	uint16_t size;
	uint16_t layer;
} gds_poly;

/**
* Creates a gds database structure from a file
*
* Return: GDS database handle
*/
HGDS gds_db_create(const char* file, char* error, int elen);

/**
* Destroys the memory held by the GDS database
*/
void gds_db_release(HGDS hGds);

/**
* Flattens the cell "cell" in gds_database structure "gds" and writes the
* output to a pointer array "pvec"
* 
* @bounds: boundary box (xmin, ymin, dx, dy) of polygons to include or
*		   NULL if all polygons are to be included
*
* @callback: a callback function that can be used to update progress on during
*            flattening. If it returns 1 flattening will terminate. Can be
*            set NULL.
* 
* @max_polys: Max number of polygons to output (to limit memory)
* 
* @error and @elen: char array with length for an error message if flattening
*                   failed
* 
* Return: 1 (success) 0 (failure)
*
*/
int gds_collapse(HGDS hGds, const char* cell, const double* bounds,
	uint64_t max_polys, parray* pvec, int (*callback)(uint64_t, uint64_t), char* error, int elen);

/**
* Writes polyset "pset" to a GDS file
* 
* * @error and @elen: char array with length for an error message if flattening
*                   failed
* Return: 1 (success) 0 (failure)
*
*/
int gds_write(HGDS hGds, const char* dest, parray* pvec, char* error, int elen);

/**
* Checks if point @p is in polygon @poly. Polygon is closed with @n vertices.
* The nth vertex is the same as the first.
*/
int gds_poly_contains_point(struct gds_ipair* poly, int n, struct gds_ipair p);

/**
* Prints all top cells of the gds database structure
*/
void gds_top_cells(HGDS hGds);

/**
* Prints all cells of the gds database structure
*/
void gds_all_cells(HGDS hGds);

/**
* Support function to destroy all polygons pointed to by pointer array pset
**/
void gds_polyset_release(parray* pset);
