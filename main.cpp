#include "gds.h"

#include <format>
#include <iostream>
#include <string>

int StringToWString(std::wstring& ws, const std::string& s)
{
	// https://stackoverflow.com/questions/2573834/c-convert-string-or-char-to-wstring-or-wchar-t

	std::wstring wsTmp(s.begin(), s.end());

	ws = wsTmp;

	return 0;
}

int main(int argc, char** argv)
{
	//
	// Example code to read a GDS file and convert it into polygons
	//

	// Create the GDS database from a file

	std::wstring filename(L"NAND.gds");
	std::wstring error;
	GDS::HGDS hGds = GDS::Create(filename, &error);

	if (hGds)
	{
		std::wcout << std::format(L"The GDS database was succesfully created from file: {}\n", filename);
	}
	else
	{
		std::wcout << std::format(L"Error found reading GDS: {}\n", error);
		return 0;
	}

	// Print all the top cells in the database
	GDS::ListTopCells(hGds);

	// Print all the cells in the database
	GDS::ListAllCells(hGds);

	//
	// Collapse a cell in the database and keep only polygons overlapping with
	// the given bounding box
	//

	// Cell name to flatten/collapse
	char cell[] = "NAND";

	// Bounding box bottom left coordinates followed by width and height
	double bounds[4] = { 28.7, 45.2, 80.0, 60.0 };

	// Vector to write the polygons too
	std::vector<GDS::Poly*> pset;

	if (GDS::ExtractPolygons(hGds, cell, bounds, pset, nullptr, &error))
	{
		std::wstring cellw;
		StringToWString(cellw, cell);
		std::wcout << std::format(L"Polygon extraction from cell \"{}\" in file \"{}\" was succesful\n", cellw, filename);
	}
	else
	{
		std::wcout << std::format(L"Error found extracting polygons: {}\n", error);
		return 0;
	}

	wchar_t out[] = L"out.gds";
	if (GDS::WritePolys(hGds, out, pset, &error))
	{
		std::wcout << std::format(L"The GDS database was written succesfully to file \"{}\"\n", out);
	}
	else
	{
		std::wcout << std::format(L"Error found writing to file: %s", out);
		return 0;
	}

	// Release the memory occupied by the database
	GDS::Release(hGds);

	// Release all the polygons in pset
	GDS::PolysetRelease(pset);
	pset.clear();

	return 0;
}