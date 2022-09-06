# gds_c
Read a GDSII file and collapse the hierarchy of any cell contained in it.

Code to read in a GDSII file and write any cell from it (or the top cell) into a new GDSII file with a flat hierarchy with only
basic polygons (i.e. no SREF, AREF or PATH structures).

An example of its use is given in the `main.c` file. It's a two step process. First the input file is read in and stored in a `gds_db` structure. Second.
this structure can be use to flatten (collapse) any GDS cell contained in it and write it to an output GDS file or a pointer array of pointers
to a polygon structures `gds_poly`.

Questions: janwillembos@yahoo.com
