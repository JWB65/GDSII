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

//
// NOTES
//
// PATHTYPE 1 is not supported and expanded as PATHTYPE 0 instead. PATHTYPE 2
// is supported.
//
// The following GDS elements are ignored: text, node, box, property
//
// Up to 8191 points in an boundary or path element
//
// The ExtractPolygons function extracts up to 20M polygons. The size of a
// polygon is 4 + 8 * n bytes, with n the number of points in the polygon. So
// with 5 pairs 44 bytes per polygon this could take up to 880MB in memory.
//

#pragma once

#include <string>
#include <vector>

namespace GDS
{
	const int MAX_POLYS = (int) 20E6;

	// Opaque handle to the GDS database structure
	typedef void* HGDS;
	
	// IPair and Poly structures needed to receive the polygons contained in 
	// the GDS databae

	struct IPair {
		int x, y;
	};

	struct Poly {
		IPair* pairs;
		uint16_t size;
		uint16_t layer;
	};

	//
	// Creates a gds database structure from a file
	// 
	// @file: file path of GDS file
	// 
	// @msg: receives the error message on failure. Can be nullptr.
	//
	// returns: GDS database handle or nullptr if failed.
	//
	HGDS Create(const std::wstring& file, std::wstring* msg);

	//
	// Releases the memory held by the GDS database handle @hGds
	//
	void Release(HGDS hGds);

	//
	// Extracts all polygons in @cell within bounds @bounds (if not nullptr)
	// 
	// The polygons are written to a vector of Poly pointers @pvec. The caller
	// needs to delete the pointers for which is helper function is provided
	// in the below.
	// 
	// A callback function is provided to report progress of the extraction.
	// 
	// @bounds: boundary box (xmin, ymin, dx, dy) of polygons to include or
	//		    nullptr if the entire cell is to be included
	//
	// @callback: a callback function that can be used to update progress on during
	//            flattening. If it returns 1 flattening will terminate. Can be
	//            set nullptr. The first parameter is the number of polygons added
	//            so far and the second parameter the number of polygons scanned.
	//
	// Return: 1 (success) 0 (failure). In case of failure the failure reason
	//         is reported in @msg (if not nullptr).
	//
	int ExtractPolygons(HGDS hGds, const std::string& cell, const double* bounds,
		std::vector<Poly*> &pvec, int (*callback)(uint64_t, uint64_t), std::wstring* msg);

	//
	// Writes polyset @pset to a newly created GDS II file
	//
	// @msg: receives the error message if failure. Can be nullptr.
	//
	// Return: 1 (success) 0 (failure)
	//
	int WritePolys(HGDS hGds, const std::wstring& dest, std::vector<Poly*>& pvec, std::wstring* msg);

	//
	// Checks if point @p is in polygon @poly. Polygon is closed with @n vertices.
	// The nth vertex is the same as the first.
	//
	int PointInPolygon(IPair* poly, int n, IPair p);

	//
	// Prints all top cells of the gds database structure
	//
	void ListTopCells(HGDS hGds);

	//
	// Prints all cells of the gds database structure
	//
	void ListAllCells(HGDS hGds);

	//
	// Support function to destroy all polygons in polygon pointer vector
	//
	void PolysetRelease(std::vector<Poly*> & pset);

	// Retrieve the GDS file used to create the given GDS database */
	std::wstring& GetFilepath(HGDS hGds);

	// Retrieve the size of the db unit in user units */
	float Getuu(HGDS hGds);
}
