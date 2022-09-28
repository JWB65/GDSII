/*
 * Copyright (c) 2022 Jan Willem Bos - janwillembos@yahoo.com
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met :
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and /or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *	  software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <string>
#include <vector>

namespace GDS
{

	/* Opaque handle to the GDS database structure */
	typedef void* HGDS;

	/*
	 * Polygon structure with member denoting the GDS layer in which the polygon
	 * is located
	 */

	struct IPair {
		int x, y;
	};

	struct Poly {
		IPair* pairs;
		uint16_t size;
		uint16_t layer;
	};

	/*
	 * Creates a gds database structure from a file
	 *
	 * @msg: receives the error message if failure. Can be nullptr.
	 *
	 * returns: GDS database handle or nullptr if failed.
	 */
	HGDS create(const std::wstring& file, std::wstring* msg);

	/*
	 * Releases the memory held by the GDS database handle
	 */
	void release(HGDS hGds);

	/*
	 * Flattens the cell "cell" in gds_database structure "gds" and writes the
	 * output to a pointer array "pvec"
	 *
	 * @bounds: boundary box (xmin, ymin, dx, dy) of polygons to include or
	 *		    nullptr if the entire cell is to be included
	 *
	 * @callback: a callback function that can be used to update progress on during
	 *            flattening. If it returns 1 flattening will terminate. Can be
	 *            set nullptr. The first parameter is the number of polygons added
	 *            so far and the second parameter the number of polygons scanned.
	 *
	 * @max_polys: Max number of polygons to output (to limit memory)
	 *
	 * @msg: receives the error message if failure. Can be nullptr.
	 *
	 * Return: 1 (success) 0 (failure)
	 */
	int collapse(HGDS hGds, const std::string& cell, const double* bounds, uint64_t max_polys,
		std::vector<Poly*> &pvec, int (*callback)(uint64_t, uint64_t), std::wstring* msg);

	/*
	 * Writes polyset "pset" to a GDS file
	 *
	 * @msg: receives the error message if failure. Can be nullptr.
	 *
	 * Return: 1 (success) 0 (failure)
	 */
	int write(HGDS hGds, const std::wstring& dest, std::vector<Poly*>& pvec, std::wstring* msg);

	/*
	 * Checks if point @p is in polygon @poly. Polygon is closed with @n vertices.
	 * The nth vertex is the same as the first.
	 */
	int gds_poly_contains_point(IPair* poly, int n, IPair p);

	/*
	 * Prints all top cells of the gds database structure
	 */
	void top_cells(HGDS hGds);

	/*
	 * Prints all cells of the gds database structure
	 */
	void all_cells(HGDS hGds);

	/*
	 * Support function to destroy all polygons pointed to by pointer array pset
	 */
	void polyset_release(std::vector<Poly*> & pset);

	/* Retrieve the GDS file used to create the given GDS database */
	std::wstring& getfile(HGDS hGds);

	/* Retrieve the size of the db unit in user units */
	float getuu(HGDS hGds);
}
