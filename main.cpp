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
	GDS::HGDS hGds = GDS::create(filename, &error);

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
	GDS::top_cells(hGds);

	// Print all the cells in the database
	GDS::all_cells(hGds);

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

	// Max number of polygons (e.g. to limit memory)
	uint64_t mcount = (uint64_t)10E+6;

	if (GDS::collapse(hGds, cell, bounds, mcount, pset, nullptr, &error))
	{
		std::wstring cellw;
		StringToWString(cellw, cell);
		std::wcout << std::format(L"The GDS collapse of cell \"{}\" from file \"{}\" was succesful\n", cellw, filename);
	}
	else
	{
		std::wcout << std::format(L"Error found collapsing cell: {}\n", error);
		return 0;
	}

	wchar_t out[] = L"out.gds";
	if (GDS::write(hGds, out, pset, &error))
	{
		std::wcout << std::format(L"The GDS database was written succesfully to file \"{}\"\n", out);
	}
	else
	{
		std::wcout << std::format(L"Error found writing to file: %s", out);
		return 0;
	}

	// Release the memory occupied by the database
	GDS::release(hGds);

	// Release all the polygons in pset
	GDS::polyset_release(pset);
	pset.clear();

	return 0;
}