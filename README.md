# GDSII

A simple library for reading and extracting polygons from a GDS II layout files used in the semiconductor industry.

# Features

* Extract polygons from a GDS II cell. The user of the library can use this pointer array for further programming work.
* Write the polygons to a newly created GDS II file.

# How to add the library to your project

No dependencies except the standard library. Just include
```
gds.h
gds.cpp
```
in your project.

# Instructions to use

* An example of its use is given in the `main.c` and the `gds.h` header gives further information on the use of each function.

* Create a handle to a GDS II database with `HGDS hGds = GDS::Create(name, &error)`.

* Read in the polygons of a given cell into a pointer vector by `GDS::ExtractPolygons(hGds, cell, bounds, pset, NULL, &error)`.

* If desired, create a new GDS II file from the polygons with `GDS::Write(hGds, out, pset, &error)`.

* The function `GDS::PolysetRelease(pset)` can be used to release all memory allocated by polygon pointer array.

# The polygon structure

The polygon structure `Poly` is just an array of pairs with an integer specifying the layer number in the GDS II database.

```
struct GDS::IPair{
	int x, y;
};

struct GDS::Poly{
	GDS::IPair* pairs;
	uint16_t size;
	uint16_t layer;
};
```
The `layer` member of the structure identifies the GDS layer number the polygon belongs to.

# Example GDS II file

The file `main.c` contains and example use of the library. The file `nand.gds` is the example GDS II file used in it.

Questions: janwillembos@yahoo.com
