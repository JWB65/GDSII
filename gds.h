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
#include <stdio.h>
#include <stdbool.h>
#include "vec.h"

typedef enum GDS_ERROR
{
	SUCCESS = 0,
	INVALID_ARGUMENT,
	CELL_NOT_FOUND,
	FILE_ERROR
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
* Return: error code
*/
GDS_ERROR gds_db_new(struct gds_db** gds, const char* file);

/**
* Destroys the memory held by the gds_database structure
*/
void gds_db_delete(struct gds_db* gds);

/**
* Flattens cell @cell in gds_database structure and writes the output to a
* GDS file AND/OR an existing polygon vector
* 
* @bounds: boundary box (xmin, ymin, xmax, ymin) of polygons to include or
*		   NULL is all polygons are to be included
*
* @max_polys: Max number of polygons to write
* 
* @dest: output file or NULL if no output to file
* 
* @pvec: pointer to a vector (dynamic array of pointers) to place the polygons
*		 in or NULL if no output to polygons
* 
* Returns 1 if @cell == NULL, @cell is not found in the GDS database, the
* bounding box @bounds is invalid, or a failure opening file @dest if
* @dest != NULL
*
*/
GDS_ERROR gds_collapse(struct gds_db* gds, const char* cell, const double* bounds,
	uint64_t max_polys, const char* dest, vec* pvec);

/**
* Checks if point @p is in polygon @poly. Polygon is closed with @n vertices.
* The nth vertex is the same as the first.
*/
bool gds_poly_contains_point(struct gds_ipair* poly, int n, struct gds_ipair p);

/**
* Prints all top cells of a gds database structure
*/
void gds_top_cells(struct gds_db* gds);
