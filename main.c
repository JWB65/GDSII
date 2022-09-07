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


int main(int argc, char** argv)
{
	// Example code

	// Char array for error reporting
	char error[256];

	// Create the GDS database from a file
	char name[] = "NAND.gds";
	HGDS hGds = gds_db_create(name, error, 256);

	if (hGds)
	{
		printf("The GDS database was succesfully created from file \"%s\"\n", name);
	} else {
		printf("Error found reading GDS: %s", error);
		return 0;
	}

	// Print all the top cells in the database
	gds_top_cells(hGds);

	// Print all the cells in the database
	gds_all_cells(hGds);

	//
	// Collapse a cell in the database and keep only polygons overlapping with
	// the given bounding box
	//

	// Bounding box nottom left coordinates followed by width and height
	double bounds[4] = { 28.7, 45.2, 80.0, 60.0 };

	// Pointer array to polygons for the resulting polygons to place in
	parray* pset = parray_create();

	// Max number of polygons (e.g. to limit memory)
	uint64_t mcount = (uint64_t)10E+6;
	
	// Cell name to flatten/collapse
	char cell[] = "NAND";

	if (gds_collapse(hGds, cell, bounds, mcount, pset, NULL, error, 256))
	{
		printf("The GDS collapse of cell \"%s\" from file \"%s\" was succesful\n", cell, name);
	} else {
		printf("Error found collapsing cell: %s", error);
		return 0;
	}

	char out[] = "out.gds";
	if (gds_write(hGds, out, pset, error, 256))
	{
		printf("The GDS database was written succesfully to file \"%s\"\n", out);
	} else {
		printf("Error found writing to file: %s", error);
		return 0;
	}

	// Release the memory occupied by the database
	gds_db_release(hGds);

	// Release the polygons objects in pset
	gds_polyset_release(pset);

	// Release the memory occupied by pset
	parray_release(pset);

	return 0;
}