#pragma once

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

#include "parray.h"

#include <inttypes.h>

typedef enum GDS_ERROR
{
	GDS_SUCCESS = 0,
	GDS_INVALID_ARGUMENT,
	GDS_CELL_NOT_FOUND,
	GDS_FILE_ERROR,
	GDS_INTERUPT,
	GDS_BB_ERROR
} GDS_ERROR;

typedef struct gds_db gds_db;

/**
* Creates a gds database structure from a file
*
* Return: GDS_ERROR type
*/
GDS_ERROR gds_db_create(struct gds_db** gds, const char* file);

/**
* Destroys the memory held by the gds_database structure
*/
void gds_db_release(struct gds_db* gds);

/**
* Flattens the cell "cell" in gds_database structure "gds" and writes the
* output to a polygon vector "pvec"
* 
* @bounds: boundary box (xmin, ymin, xmax, ymin) of polygons to include or
*		   NULL if all polygons are to be included
*
* @max_polys: Max number of polygons to output (to limit memory)
* 
* Return: GDS_ERROR type
*
*/
GDS_ERROR gds_collapse(struct gds_db* gds, const char* cell, const double* bounds,
	uint64_t max_polys, parray* pvec, int (*callback)(uint64_t, uint64_t));

/**
* Writes polyset "pset" to a GDS file
*
* Return: GDS_ERROR type
*
*/
GDS_ERROR gds_write(struct gds_db* gds, const char* dest, parray* pvec);

/**
* Checks if point @p is in polygon @poly. Polygon is closed with @n vertices.
* The nth vertex is the same as the first.
*/
int gds_poly_contains_point(struct gds_ipair* poly, int n, struct gds_ipair p);

/**
* Prints all top cells of a gds database structure
*/
void gds_top_cells(struct gds_db* gds);

// Special function to destroy the memory occupied by the pointers in pset
void gds_polyset_clear(parray* pset);
