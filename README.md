# gds_c

Code to extract polygons from any cell, or region in the cell, contained in an GDSII file.

Read a GDSII file and collapse/flatten the hierarchy of any cell contained in it to a pointer array of simple polygons of type `gds_poly`.

The polygons can be written to a new output GDSII file.

An example of its use is given in the `main.c` and `gds.h` files.

No dependencies except the standard library. Include
```
gds.h
gds.c
parray.c
parray.h
```
to integrate it in your code.

`nand.gds` is an example gds file with `out.gds` the output from the example.

Questions: janwillembos@yahoo.com
