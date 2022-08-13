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

int main(int argc, char** argv)
{
	GDS_ERROR result;

	uint64_t mcount = (uint64_t)10E+6;

	// create the gds database from the input file
	struct gds_db* gds_db = NULL;
	
	if ((result = gds_db_new(&gds_db, "NAND.gds")) != SUCCESS) {
		printf("Error %d creating the GDS database\n", result);
		return 0;
	}

	double bounds[4] = { 28.7, 45.2, 50.0, 85.5 };
	//double cut[4] = { 544.44, 40376.0, 48.0, 48.0 };
	//double bounds[4] = { cut[0], cut[1], cut[0] + cut[2], cut[1] + cut[3] };

	vec* pset = vec_new();
	if ((result = gds_collapse(gds_db, "0_PM01B_mask", bounds, mcount, "out.gds", pset)) != SUCCESS) {
		printf("Error %d found in cell collapse\n", result);
	} else {
		printf("Found %d polygons in cell\n", (int) pset->size);
	}

	// delete the database
	gds_db_delete(gds_db);

	return 0;
}