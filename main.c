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

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
	GDS_ERROR result;

	uint64_t mcount = (uint64_t)10E+6;

	// create the gds database from the input file
	struct gds_db* gds_db = NULL;
	
	if ((result = gds_db_new(&gds_db, "NAND.gds")) == GDS_SUCCESS) {
		printf("GDS database was succesfully created\n");
	} else {
		printf("Error %d creating the GDS database\n", result);
		return 0;
	}

	//double bounds[4] = { 28.7, 45.2, 50.0, 85.5 };
	double bounds[4] = { 28.7, 45.2, 80.0, 60.0 };

	//double cut[4] = { 544.44, 40376.0, 48.0, 48.0 };
	//double bounds[4] = { cut[0], cut[1], cut[0] + cut[2], cut[1] + cut[3] };

	vec* pset = vec_new();
	if ((result = gds_collapse(gds_db, "NAND", bounds, mcount, pset)) == GDS_SUCCESS) {
		printf("gds_collapse was succesful\n");
	} else {
		printf("Error %d found executing gds_collapse\n", result);
	}

	if ((result = gds_write(gds_db, "out.gds", pset)) == GDS_SUCCESS) {
		printf("GDS was written succesfully\n");
	} else {
		printf("Error %d found in writing to GDS file\n", result);
	}

	// delete the database
	gds_db_delete(gds_db);

	// caller still needs to delete the polygon set
	for (int i = 0; i < pset->size; i++) {
		struct gds_poly* p = vec_get(pset, i);
		free(p->pairs);
		free(p);
	}
	vec_delete(pset);

	return 0;
}