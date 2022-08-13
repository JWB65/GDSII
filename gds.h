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

#include <inttypes.h>
#include <stdbool.h>
#include "vec.h"

typedef enum GDS_ERROR
{
	GDS_SUCCESS = 0,
	GDS_INVALID_ARGUMENT,
	GDS_CELL_NOT_FOUND,
	GDS_FILE_ERROR,
	GDS_BB_ERROR
} GDS_ERROR;

#define MAX_STR_NAME 32

struct gds_ipair {
	int x;
	int y;
};

struct gds_poly {
	// special polygon structure with layer information

	struct gds_ipair* pairs;
	uint16_t size;
	uint16_t layer;
};

struct gds_bndry {
	// GDS BOUNDARY element

	uint16_t layer;

	struct gds_ipair* pairs;
	int size;
};

struct gds_path {
	// GDS PATH element

	uint16_t layer;

	struct gds_ipair* pairs;
	int size;

	// Expanded path
	struct gds_ipair* epairs;
	int esize;

	uint16_t pathtype;
	uint32_t width;
};

struct gds_sref {
	// GDS SREF element

	int x, y;

	char sname[MAX_STR_NAME + 1];
	
	// Pointer to the structure referenced
	struct gds_cell* cell;

	uint16_t strans;
	float mag, angle;
};

struct gds_aref {
	// GDS AREF element

	int32_t x1, y1, x2, y2, x3, y3;

	char sname[MAX_STR_NAME + 1];

	// Pointer to the structure referenced
	struct gds_cell* cell;

	int col, row;

	uint16_t strans;
	float mag, angle;
};

struct gds_cell {
	// GDS structure (or here named "cell")

	char strname[MAX_STR_NAME + 1];

	vec* boundaries;
	vec* paths;
	vec* srefs;
	vec* arefs;
};

struct gds_db {
	// GDS database

	vec* cells;

	const char* file;

	uint16_t version;

	double uu_per_dbunit, meter_per_dbunit;

	// UNITS record in binary form
	uint8_t units[16];
};

/**
* Creates a gds database structure from a file
*
* Return: GDS_ERROR type
*/
GDS_ERROR gds_db_new(struct gds_db** gds, const char* file);

/**
* Destroys the memory held by the gds_database structure
*/
void gds_db_delete(struct gds_db* gds);

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
	uint64_t max_polys, vec* pvec);

/**
* Writes polyset "pset" to a GDS file
*
* Return: GDS_ERROR type
*
*/
GDS_ERROR gds_write(struct gds_db* gds, const char* dest, vec* pvec);

/**
* Checks if point @p is in polygon @poly. Polygon is closed with @n vertices.
* The nth vertex is the same as the first.
*/
bool gds_poly_contains_point(struct gds_ipair* poly, int n, struct gds_ipair p);

/**
* Prints all top cells of a gds database structure
*/
void gds_top_cells(struct gds_db* gds);

/**
* Deletes all polygons in pset
*/
void gds_polyset_delete(vec* pset);
