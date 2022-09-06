/**
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
*
* Collapses the hierarchy in a cell in a GDS file and writes to an output GDS
* file with only polygons (BOUNDARY elements)
*
*/

#include "gds.h"
#include "parray.h"

#include <stdio.h>
#include <stdlib.h>

// Callback function called during GDS collapse to display progress
int progress(uint64_t a, uint64_t b)
{
	printf("%d out of %d\n", (int)a, (int)b);

	return 1;
}

int main(int argc, char** argv)
{
	GDS_ERROR result;


	// Pointer to the GDS database object
	gds_db* db = NULL;
	
	// Create the GDS database object from a GDS file
	char name[] = "NAND.gds";
	if ((result = gds_db_create(&db, name)) == GDS_SUCCESS)
	{
		printf("The GDS database was succesfully created from file \"%s\"\n", name);
	} else {
		printf("Error %d creating the GDS database from file \"%s\"\n", result, name);
		return 0;
	}

	// Bottom left coordinates followed by width and height
	double bounds[4] = { 28.7, 45.2, 80.0, 60.0 };

	// Pointer array to polygons
	parray* pset = NULL;
	parray_create(&pset);

	uint64_t mcount = (uint64_t)10E+6;

	char cell[] = "NAND";

	// Note: use NULL instead of bounds to collapse the entire cell
	if ((result = gds_collapse(db, cell, bounds, mcount, pset, progress)) == GDS_SUCCESS)
	{
		printf("The GDS collapse of cell \"%s\" from file \"%s\" was succesful\n", cell, name);
	} else {
		printf("Error %d found executing gds_collapse\n", result);
	}

	char out[] = "out.gds";
	if ((result = gds_write(db, out, pset)) == GDS_SUCCESS)
	{
		printf("The GDS database was written succesfully to file \"%s\"\n", out);
	} else {
		printf("Error %d found in writing to GDS file \"%s\"\n", result, out);
	}

	// Release the memory occupied by the database
	gds_db_release(db);

	// Release the polygons objects in pset
	gds_polyset_release(pset);

	// Release the memory occupied by pset
	parray_release(pset);

	return 0;
}