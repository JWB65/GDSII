# gds_c

A simple library for work on a GDS II files used in the semiconductor industry.

# Features

* Extract polygons from a GDS II cell to a pointer array of polygon structures described below. The user of the library can use this pointer array for further work.
* Write a the polygons in this pointer array to a newly created GDS II file.

# How to add the library to your project

No dependencies except the standard library. Include
```
gds.h
gds.c
parray.c
parray.h
```
to integrate it in your code.

# Instructions to use

* Create a handle to a GDS II database with `HGDS hGds = gds_db_create(name, error, 256);`

* Create and empty generic pointer array by `parray* pset = parray_create();`

* Read in the polygons of a given cell into the pointer array by `gds_collapse(hGds, cell, bounds, mcount, pset, NULL, error, 256);`

* Create a new GDS II file with these polygons with `gds_write(hGds, out, pset, error, 256);`

* An example of its use is given in the `main.c` and the `gds.h` header gives further information on the use of each function.

# The polygon structure

The polygon structure `gds_poly` is just an array of pairs with an integer specifying the layer number in the GDS II database.

```
typedef struct gds_ipair {
	int x, y;
} gds_ipair;

typedef struct {
	gds_ipair* pairs;
	uint16_t size;
	uint16_t layer;
} gds_poly;
```
The `layer` member of the structure identifies the GDS layer number the polygon belongs to.

# Example GDS II file

`nand.gds` is an example gds file with `out.gds` the output from the example.

Questions: janwillembos@yahoo.com
