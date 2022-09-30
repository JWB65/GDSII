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

#include "gds.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <format>
#include <iostream>
#include <string>
#include <vector>

namespace GDS
{

	//
	// The maximum recond length of a GDS record is 0xFFFF including the 4 bytes header. Therefore,
	// the maximum number of pairs in an XY element is (0xFFFF - 4) / 8 = 8191.
	//
	// A path element of n points expands to a polygon
	// 
	// In a path element the max number of pairs therefore is  2 x 8191 + 1 = 16383 pairs maximum.
	//
	const int MAX_PAIRS = 16383;

	struct Bndry
	{
		// GDS BOUNDARY element

		uint16_t layer = 0;

		IPair* pairs = nullptr;
		int size = 0;
	};

	struct Path
	{
		// GDS PATH element

		uint16_t layer = 0;

		IPair* pairs = nullptr;
		int size = 0;

		// Expanded path
		IPair* epairs = nullptr;
		int esize = 0;

		uint16_t pathtype = 0;
		uint32_t width = 0;
	};

	struct GCell; // Forward decleration

	struct SRef
	{
		// GDS SREF element

		int x = 0, y = 0;

		std::string sname;

		// Pointer to the cell referenced
		GCell* cell = nullptr;

		uint16_t strans = 0;;
		float mag = 1.0f, angle = 0.f;
	};

	struct ARef
	{
		// GDS AREF element

		int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;

		std::string sname;

		// Pointer to the cell referenced
		GCell* cell = nullptr;

		int col = 0, row = 0;

		uint16_t strans = 0;
		float mag = 1.f, angle = 0;
	};

	struct GCell
	{
		// GDS structure (or here named "cell")

		std::string strname;

		std::vector<Bndry*> boundaries;
		std::vector<Path*> paths;
		std::vector<SRef*> srefs;
		std::vector<ARef*> arefs;
	};

	struct DB
	{
		std::vector<GCell*> cells;

		std::wstring fpath;

		uint16_t version = 0;

		// Size of db unit in user units
		// Size of db unit in meter
		double uu_per_dbunit = 0.0, meter_per_dbunit = 0.0;

		// UNITS record in binary form
		uint8_t units[16] = { };
	};

	enum RECORD
	{
		GDS_HEADER = 0x0002,
		GDS_BGNLIB = 0x0102,
		GDS_LIBNAME = 0x0206,
		GDS_UNITS = 0x0305,
		GDS_ENDLIB = 0x0400,
		GDS_BGNSTR = 0x0502,
		GDS_STRNAME = 0x0606,
		GDS_ENDSTR = 0x0700,
		GDS_BOUNDARY = 0x0800,
		GDS_PATH = 0x0900,
		GDS_SREF = 0x0a00,
		GDS_AREF = 0x0b00,
		GDS_TEXT = 0x0c00,
		GDS_LAYER = 0x0d02,
		GDS_DATATYPE = 0x0e02,
		GDS_WIDTH = 0x0f03,
		GDS_XY = 0x1003,
		GDS_ENDEL = 0x1100,
		GDS_SNAME = 0x1206,
		GDS_COLROW = 0x1302,
		GDS_TEXTNODE = 0x1400,
		GDS_NODE = 0x1500,
		GDS_TEXTTYPE = 0x1602,
		GDS_PRESENTATION = 0x1701,
		GDS_STRING = 0x1906,
		GDS_STRANS = 0x1a01,
		GDS_MAG = 0x1b05,
		GDS_ANGLE = 0x1c05,
		GDS_REFLIBS = 0x1f06,
		GDS_FONTS = 0x2006,
		GDS_PATHTYPE = 0x2102,
		GDS_GENERATIONS = 0x2202,
		GDS_ATTRTABLE = 0x2306,
		GDS_ELFLAGS = 0x2601,
		GDS_NODETYPE = 0x2a02,
		GDS_PROPATTR = 0x2b02,
		GDS_PROPVALUE = 0x2c06,
		GDS_BOX = 0x2d00,
		GDS_BOXTYPE = 0x2e02,
		GDS_PLEX = 0x2f03,
		GDS_BGNEXTN = 0x3003,
		GDS_ENDEXTN = 0x3103,
		GDS_FORMAT = 0x3602
	};

	struct Line
	{
		double a = 0.0, b = 0.0, c = 0.0;
	};

	struct Trans
	{
		int x = 0, y = 0;
		float mag = 0.f, angle = 0.f;
		short mirror = 0;
	};

	struct BB
	{
		int32_t xmin = 0;
		int32_t xmax = 0;
		int32_t ymin = 0;
		int32_t ymax = 0;
	};

	//
	// Global parameter with information that otherwise needs to be passed between
	// recurances
	//
	struct Rinfo
	{
		std::vector<Poly*>* out_pset;
		DB* gds = nullptr;
		BB bbox;
		int use_bb = 0;
		uint64_t scount = 0, pcount = 0;
		int (*callback)(uint64_t, uint64_t) = nullptr;
		int interupt = 0;
	};

	static double buf_read_float(unsigned char* p)
	{
		int i, sign, exp;
		double fraction;

		// The binary representation is with 7 bits in the exponent and 56 bits in
		// the mantissa:
		//
		// SEEEEEEE MMMMMMMM MMMMMMMM MMMMMMMM
		// MMMMMMMM MMMMMMMM MMMMMMMM MMMMMMMM

		// Denominators for the 7 fraction bytes
		uint64_t div[7] = {
			0x0100,
			0x010000,
			0x01000000,
			0x0100000000,
			0x010000000000,
			0x01000000000000,
			0x0100000000000000
		};

		sign = 1;
		exp = 0;
		if (p[0] > 127)
		{
			sign = -1;
			exp = p[0] - 192;
		}
		else
		{
			exp = p[0] - 64;
		}

		fraction = 0.0;
		for (i = 0; i < 7; ++i)
		{
			fraction += p[i + 1] / (double)div[i];
		}

		return (pow(16, exp) * sign * fraction);
	}

	static void buf_write_int(char* p, int offset, int n)
	{
		p[offset + 0] = (n >> 24) & 0xFF;
		p[offset + 1] = (n >> 16) & 0xFF;
		p[offset + 2] = (n >> 8) & 0xFF;
		p[offset + 3] = n & 0xFF;
	}

	static GCell* find_struct(DB* gds, const std::string& name)
	{
		uint64_t i;

		uint64_t size = gds->cells.size();
		for (i = 0; i < size; i++)
		{
			GCell* s;

			s = gds->cells.at(i);
			if (strcmp(s->strname.c_str(), name.c_str()) == 0)
			{
				return s;
			}
		}

		return nullptr;
	}

	static void file_append_record(FILE* file, uint16_t record)
	{
		char buf[4];

		buf[0] = 0;
		buf[1] = 4;
		buf[2] = (record >> 8) & 0xFF;
		buf[3] = record & 0xFF;

		fwrite(buf, 4, 1, file);
	}

	static void file_append_short(FILE* file, uint16_t record, uint16_t data)
	{
		char buf[6];

		buf[0] = 0;
		buf[1] = 6;
		buf[2] = (record >> 8) & 0xFF;
		buf[3] = record & 0xFF;

		buf[4] = (data >> 8) & 0xFF;
		buf[5] = data & 0xFF;

		fwrite(buf, 6, 1, file);
	}

	static void file_append_bytes(FILE* file, uint16_t record, char* data, int len)
	{
		char buf[4];

		buf[0] = ((len + 4) >> 8) & 0xFF;
		buf[1] = (len + 4) & 0xFF;
		buf[2] = (record >> 8) & 0xFF;
		buf[3] = record & 0xFF;

		fwrite(buf, 4, 1, file);
		fwrite(data, len, 1, file);
	}

	static void file_append_string(FILE* file, uint16_t record, const char* string)
	{
		char buf[4];
		size_t text_len, record_len;

		text_len = strlen(string);

		record_len = text_len;
		if (text_len % 2) record_len++;

		buf[0] = ((record_len + 4) >> 8) & 0xFF;
		buf[1] = (record_len + 4) & 0xFF;
		buf[2] = (record >> 8) & 0xFF;
		buf[3] = record & 0xFF;

		fwrite(buf, 4, 1, file);
		fwrite(string, text_len, 1, file);
		if (text_len % 2)
		{
			putc(0, file);
		}
	}

	static void file_append_poly(FILE* pfile, IPair* p, int size, uint16_t layer)
	{
		// store in buffer in the format of the gds standard
		char* buf = new char[8 * size];

		for (int i = 0; i < size; i++)
		{
			buf_write_int(buf, 8 * i, p[i].x);
			buf_write_int(buf, 8 * i + 4, p[i].y);
		}

		file_append_record(pfile, GDS_BOUNDARY);
		file_append_short(pfile, GDS_LAYER, layer);
		file_append_short(pfile, GDS_DATATYPE, 0);
		file_append_bytes(pfile, GDS_XY, buf, 8 * size);
		file_append_record(pfile, GDS_ENDEL);

		delete[] buf;
	}

	static IPair line_intersection(Line* one, Line* two)
	{
		double xh, yh, wh;

		// line-line intersection (homogenous coordinates):
		//		(B1C2 - B2C1, A2C1 - A1C2, A1B2 - A2B1)
		xh = one->b * two->c - two->b * one->c;
		yh = two->a * one->c - one->a * two->c;
		wh = one->a * two->b - two->a * one->b;

		IPair out = { (int)(xh / wh), (int)(yh / wh) };

		return out;
	}

	static IPair line_project(IPair p, Line* line)
	{
		Line normal;

		// normal to Ax+By+C = 0 through (x1, y1)
		// A'x+B'y+C' = 0 with
		// A' = B  B' = -A  C' = Ay1 - Bx1

		normal.a = line->b;
		normal.b = -line->a;
		normal.c = line->a * p.y - line->b * p.x;

		// intersect the normal through (p.x, p.y) with line
		return line_intersection(line, &normal);
	}

	static IPair extend_vector(IPair tail, IPair head, double length)
	{
		/*
		 * Extends a vector given by @tail and @head in the tail direction by
		 * @length
		 */

		double segx, segy, norm;
		IPair out = { tail.x, tail.y };

		segx = tail.x - head.x;
		segy = tail.y - head.y;
		norm = sqrt(segx * segx + segy * segy);

		if (norm != 0.0)
		{
			out.x = tail.x + (int)((length / norm) * segx);
			out.y = tail.y + (int)((length / norm) * segy);
		}

		return out;
	}

	static IPair vec_sum(IPair one, IPair two)
	{
		IPair out = { one.x + two.x, one.y + two.y };

		return out;
	}

	static void expand_path(IPair* pout, IPair* pin, int size, int width, int pathtype)
	{
		double hwidth = width / 2.0;

		if (size < 2)
		{
			return;
		}

		// Parallel line segments on both sides
		Line* mlines = new Line[size - 1];
		Line* plines = new Line[size - 1];

		for (int i = 0; i < size - 1; i++)
		{
			double a, b, c, c_trans;

			// line thr. (x1, y1) & (x2, y2): Ax + By + C = 0  A = y2 - y1
			//		B = -(x2 - x1)  C = -By1 - Ax1
			a = pin[i + 1].y - pin[i].y;
			b = -(pin[i + 1].x - pin[i].x);
			c = -b * pin[i].y - a * pin[i].x;

			// line parallel to Ax + By + C = 0 at distance d:   Ax + By +
			//		(C +/- d * sqrt(A^2 + B^2))
			c_trans = hwidth * sqrt(a * a + b * b);

			plines[i].a = mlines[i].a = a;
			plines[i].b = mlines[i].b = b;

			// c ooefficient in standard form of parallel segments
			plines[i].c = c + c_trans;
			mlines[i].c = c - c_trans;
		}

		// Head points

		IPair end_point;
		if (pathtype == 2)
		{
			end_point = extend_vector(pin[0], pin[1], hwidth);
		}
		else
		{
			end_point = pin[0];
		}
		pout[0] = line_project(end_point, &plines[0]);
		pout[2 * size - 1] = line_project(end_point, &mlines[0]);
		pout[2 * size] = pout[0];

		// Middle points

		for (int i = 1; i < size - 1; i++)
		{
			pout[i] = line_intersection(&plines[i - 1], &plines[i]);
			pout[2 * size - 1 - i] = line_intersection(&mlines[i - 1],
				&mlines[i]);
		}

		// Tail points

		if (pathtype == 2)
		{
			end_point = extend_vector(pin[size - 1], pin[size - 2], hwidth);
		}
		else
		{
			end_point = pin[size - 1];
		}
		pout[size - 1] = line_project(end_point, &plines[size - 2]);
		pout[size] = line_project(end_point, &mlines[size - 2]);

		delete[] plines;
		delete[] mlines;
	}

	static int poly_overlap_test(IPair* pairs, int size, BB* bb)
	{
		// Tests if polygon in g_out overlaps with bounding box in g_info

		// Returns false if certainly no overlap

		// The final (closing) point of poly is not needed for this evaluation

		int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;

		for (int i = 0; i < size - 1; i++)
		{
			if (pairs[i].x > maxx)  maxx = pairs[i].x;
		}
		if (maxx < bb->xmin) return 0;

		for (int i = 0; i < size - 1; i++)
		{
			if (pairs[i].y > maxy)  maxy = pairs[i].y;
		}
		if (maxy < bb->ymin) return 0;

		for (int i = 0; i < size - 1; i++)
		{
			if (pairs[i].x < minx)  minx = pairs[i].x;
		}
		if (minx > bb->xmax) return 0;

		for (int i = 0; i < size - 1; i++)
		{
			if (pairs[i].y < miny)  miny = pairs[i].y;
		}
		if (miny > bb->ymax) return 0;

		return 1;
	}

	static void trans_and_add_poly(IPair* pin, int size, Trans tra, uint16_t layer, Rinfo* info)
	{
		// Reflect with respect to x axis before rotation

		float sign = 1.f;

		if (tra.mirror)
		{
			sign = -1.f;
		}

		float s = sinf(tra.angle);
		float c = cosf(tra.angle);

		//ipair* g_out = (ipair*)malloc(size * sizeof(ipair));

		// Having a fixed size array
		IPair tpairs[MAX_PAIRS];


		for (int i = 0; i < size; i++)
		{
			tpairs[i].x = tra.x + (int)(tra.mag * (pin[i].x * c - sign * pin[i].y * s));
			tpairs[i].y = tra.y + (int)(tra.mag * (pin[i].x * s + sign * pin[i].y * c));
		}

		info->scount++;

		if (!info->use_bb || poly_overlap_test(tpairs, size, &info->bbox))
		{
			// Define the location of the bounding box as the origin of the collapsed cell
			for (int i = 0; i < size; i++)
			{
				tpairs[i].x = tpairs[i].x - info->bbox.xmin;
				tpairs[i].y = tpairs[i].y - info->bbox.ymin;
			}

			Poly* p = new Poly;
			p->pairs = new IPair[size];
			p->size = (uint16_t)size;
			p->layer = layer;

			memcpy(p->pairs, &tpairs[0], size * sizeof(IPair));

			info->out_pset->push_back(p);

			// pcount is the polygon counter
			info->pcount++;
		}

		if (info->scount % 1000000 == 0)
		{
			int result = info->callback(info->pcount, info->scount);
			if (result)
			{
				info->interupt = 1;
			}
		}

		//free(g_out);
	}

	static IPair transform_point(IPair in, Trans tra)
	{
		// Reflect with respect to x axis before rotation
		float sign = 1.f;
		if (tra.mirror)
			sign = -1.f;

		float s = sinf(tra.angle);
		float c = cosf(tra.angle);

		IPair out;
		out.x = (int)(tra.x + tra.mag * (in.x * c - sign * in.y * s));
		out.y = (int)(tra.y + tra.mag * (in.x * s + sign * in.y * c));

		return out;
	}

	static void collapse_cell(GCell* top, Trans tra, Rinfo* info)
	{
		if (info->pcount >= MAX_POLYS)
		{
			return;
		}

		// BOUNDARY elements
		uint64_t size = top->boundaries.size();
		for (int i = 0; i < size; i++)
		{
			Bndry* b = top->boundaries.at(i);

			trans_and_add_poly(b->pairs, b->size, tra, b->layer, info);

			if (info->pcount >= MAX_POLYS || info->interupt)
			{
				return;
			}
		}

		// PATH elements
		size = top->paths.size();
		for (int i = 0; i < size; i++)
		{

			Path* p = top->paths.at(i);

			trans_and_add_poly(p->epairs, p->esize, tra, p->layer, info);

			if (info->pcount >= MAX_POLYS || info->interupt)
			{
				return;
			}
		}

		// SREF elements
		size = top->srefs.size();
		for (int i = 0; i < size; i++)
		{
			SRef* p = top->srefs.at(i);
			//struct gds_cell* str = find_struct(g_info.gds, p->sname);

			if (!p->cell)
			{
				p->cell = find_struct(info->gds, p->sname);
				if (!p->cell)
				{
					continue;
				}
			}

			GCell* str = p->cell;

			// Calculate the origin of the sub cell with the accumulated transformation
			IPair ori;
			IPair coord = { p->x, p->y };
			ori = transform_point(coord, tra);

			// Accumulate the transformation for vertices in the sub cell
			Trans acc_tra;

			acc_tra.x = ori.x;
			acc_tra.y = ori.y;
			acc_tra.mag = tra.mag * p->mag;
			acc_tra.angle = tra.angle + p->angle;
			acc_tra.mirror = tra.mirror ^ (p->strans & 0x8000);

			// Deeper level
			collapse_cell(str, acc_tra, info);

			if (info->pcount >= MAX_POLYS || info->interupt)
			{
				return;
			}
		}

		// Expand AREF elements
		size = top->arefs.size();
		for (int i = 0; i < size; i++)
		{
			ARef* p;

			p = top->arefs.at(i);

			if (!p->cell)
			{
				p->cell = find_struct(info->gds, p->sname);
				if (!p->cell)
				{
					continue;
				}
			}

			GCell* str = p->cell;

			// (v_col_x, v_col_y) vector in column direction
			float v_col_x = ((float)(p->x2 - p->x1)) / p->col;
			float v_col_y = ((float)(p->y2 - p->y1)) / p->col;

			// (v_row_x, v_row_y) vector in row direction
			float v_row_x = ((float)(p->x3 - p->x1)) / p->row;
			float v_row_y = ((float)(p->y3 - p->y1)) / p->row;

			// Cosine and sine of the accumulated transformation
			float sin = sinf(tra.angle);
			float cos = cosf(tra.angle);

			// Reflect with respect to x axis before rotation
			float sign = 1.f;
			if (tra.mirror)
				sign = -1.f;

			// loop through the reference points of the array
			for (int c = 0; c < p->col; c++)
			{
				for (int r = 0; r < p->row; r++)
				{

					// Position of the sub cell being referenced
					float x_ref = (p->x1 + c * v_col_x + r * v_row_x);
					float y_ref = (p->y1 + c * v_col_y + r * v_row_y);

					// Origin of the sub cell with the accumulated transformation

					int x = (int)(tra.x + tra.mag * (x_ref * cos - sign * y_ref * sin));
					int y = (int)(tra.y + tra.mag * (x_ref * sin + sign * y_ref * cos));

					// Cell referenced still needs to be transformed
					Trans acc_tra;

					acc_tra.x = x;
					acc_tra.y = y;
					acc_tra.mag = tra.mag * p->mag;
					acc_tra.angle = tra.angle + p->angle;
					acc_tra.mirror = tra.mirror ^ (p->strans & 0x8000);

					// Down a level
					collapse_cell(str, acc_tra, info);

					if (info->pcount >= MAX_POLYS || info->interupt)
					{
						return;
					}
				}
			}
		}
	}

	static void buf_to_string(std::string& dest, unsigned char* buf, int buf_size)
	{
		// Write GDS II style string to std::string

		for (int i = 0; i < buf_size; i++)
		{
			if (buf[i] != 0)
			{
				dest.push_back(buf[i]);
			}
		}
	}

	static void gds_cell_clear(GCell* s)
	{
		uint64_t i;

		uint64_t size = s->srefs.size();
		for (i = 0; i < size; i++)
		{
			delete s->srefs.at(i);
		}

		size = s->arefs.size();
		for (i = 0; i < size; i++)
		{
			delete s->arefs.at(i);
		}

		size = s->boundaries.size();
		for (i = 0; i < size; i++)
		{
			delete[] s->boundaries.at(i)->pairs;
			delete s->boundaries.at(i);
		}

		size = s->paths.size();
		for (i = 0; i < size; i++)
		{
			delete[] s->paths.at(i)->epairs;
			delete[] s->paths.at(i)->pairs;
			delete s->paths.at(i);
		}
	}


	// Public functions


	HGDS Create(const std::wstring& file, std::wstring* msg)
	{
		DB* gds = new DB;

		// Current GDS structure being read

		GCell* cur_cell = nullptr;

		// The different current elements being read

		Bndry* cur_boundary = nullptr;
		Path* cur_path = nullptr;
		SRef* cur_sref = nullptr;
		ARef* cur_aref = nullptr;

		FILE* p_file = nullptr;
		_wfopen_s(&p_file, file.c_str(), L"rb");

		if (!p_file)
		{
			if (msg)
			{
				*msg = std::format(L"Failed opening file {}", file);
			}

			/* RETURN */
			return nullptr;
		}

		gds->fpath = file;

		uint64_t bytes_read = 0; // number of bytes read
		uint64_t cells_read = 0; // number of structures read

		int endlib = 0; // 1 if ENDLIN record is read

		uint8_t header[4];
		while (!endlib && fread(header, 1, 4, p_file) == 4)
		{
			uint16_t buf_size, record_len, record_type;

			bytes_read += 4;

			// First 2 bytes: record length; second 2 bytes record type and data type
			record_len = (header[0] << 8) | header[1];
			record_type = header[2] << 8 | header[3];

			// The size of the additional data in bytes
			buf_size = record_len - 4;

			// Read the data
			unsigned char* buf = nullptr;
			if (buf_size > 0)
			{
				buf = new unsigned char[buf_size];
				bytes_read += fread(buf, 1, buf_size, p_file);
			}

			// Handle the GDS records
			switch (record_type)
			{
			case GDS_HEADER:
				gds->version = buf[0] << 8 | buf[1];
				break;
			case GDS_BGNLIB:
				break;
			case GDS_ENDLIB:
				endlib = 1;
				break;
			case GDS_LIBNAME:
				break;
			case GDS_BGNSTR:
				cells_read++;
				cur_cell = new GCell;
				break;
			case GDS_ENDSTR:
				gds->cells.push_back(cur_cell);
				cur_cell = nullptr;
				break;
			case GDS_UNITS:
				{
					gds->uu_per_dbunit = buf_read_float(buf);
					gds->meter_per_dbunit = buf_read_float(buf + 8);
					memcpy(gds->units, buf, 16); // also store the buffer raw data
					break;
				}
			case GDS_STRNAME:
				{
					buf_to_string(cur_cell->strname, buf, buf_size);
					break;
				}
			case GDS_BOUNDARY:
				// Create a boundary element on the heap
				cur_boundary = new Bndry;
				break;
			case GDS_PATH:
				// Create a path element on the heap
				cur_path = new Path;
				break;
			case GDS_SREF:
				// Create a sref element on the heap
				cur_sref = new SRef;
				cur_sref->mag = 1.0; // default
				break;
			case GDS_AREF:
				// Create a aref element on the heap
				cur_aref = new ARef;
				cur_aref->mag = 1.0; // default
				break;
			case GDS_TEXT:
				break;
			case GDS_NODE:
				break;
			case GDS_BOX:
				break;
			case GDS_ENDEL:
				// Add element to the current structure
				if (cur_boundary)
				{
					cur_cell->boundaries.push_back(cur_boundary);
					cur_boundary = nullptr;
				}
				else if (cur_path)
				{
					cur_cell->paths.push_back(cur_path);
					cur_path = nullptr;
				}
				else if (cur_sref)
				{
					cur_cell->srefs.push_back(cur_sref);
					cur_sref = nullptr;
				}
				else if (cur_aref)
				{
					cur_cell->arefs.push_back(cur_aref);
					cur_aref = nullptr;
				}
				break;
			case GDS_SNAME: // SREF, AREF
				if (cur_sref)
				{
					buf_to_string(cur_sref->sname, buf, buf_size);
				}
				else if (cur_aref)
				{
					buf_to_string(cur_aref->sname, buf, buf_size);
				}
				break;
			case GDS_COLROW: // AREF
				if (cur_aref)
				{
					cur_aref->col = buf[0] << 8 | buf[1];
					cur_aref->row = buf[2] << 8 | buf[3];
				}
				break;
			case GDS_PATHTYPE:
				if (cur_path)
				{
					cur_path->pathtype = buf[0] << 8 | buf[1];
				}
				break;
			case GDS_STRANS: // SREF, AREF, TEXT
				if (cur_sref)
				{
					cur_sref->strans = buf[0] << 8 | buf[1];
				}
				else if (cur_aref)
				{
					cur_aref->strans = buf[0] << 8 | buf[1];
				}
				break;
			case GDS_ANGLE: // SREF, AREF, TEXT
				if (cur_sref)
				{
					cur_sref->angle = (float)(M_PI * buf_read_float(buf) / 180.0);
				}
				else if (cur_aref)
				{
					cur_aref->angle = (float)(M_PI * buf_read_float(buf) / 180.0);
				}
				break;
			case GDS_MAG: // SREF, AREF, TEXT
				if (cur_sref)
				{
					cur_sref->mag = (float)buf_read_float(buf);
				}
				else if (cur_aref)
				{
					cur_aref->mag = (float)buf_read_float(buf);
				}
				break;
			case GDS_XY:
				{
					if (cur_boundary)
					{
						// Store the boundary coordinates

						int count = buf_size / 8; // number of pairs

						cur_boundary->size = count;
						cur_boundary->pairs = new IPair[count];

						for (int n = 0; n < count; n++)
						{
							int i = 8 * n;
							int x = buf[i] << 24 | buf[i + 1] << 16 | buf[i + 2] << 8 | buf[i + 3];
							int y = buf[i + 4] << 24 | buf[i + 5] << 16 | buf[i + 6] << 8 | buf[i + 7];

							cur_boundary->pairs[n].x = x;
							cur_boundary->pairs[n].y = y;
						}
					}
					else if (cur_sref)
					{
						// Store the sref coordinates

						cur_sref->x = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
						cur_sref->y = buf[4] << 24 | buf[5] << 16 | buf[6] << 8 | buf[7];
					}
					else if (cur_aref)
					{
						// Store the 3 vectors of the aref

						cur_aref->x1 = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
						cur_aref->y1 = buf[4] << 24 | buf[5] << 16 | buf[6] << 8 | buf[7];
						cur_aref->x2 = buf[8] << 24 | buf[9] << 16 | buf[10] << 8 | buf[11];
						cur_aref->y2 = buf[12] << 24 | buf[13] << 16 | buf[14] << 8 | buf[15];
						cur_aref->x3 = buf[16] << 24 | buf[17] << 16 | buf[18] << 8 | buf[19];
						cur_aref->y3 = buf[20] << 24 | buf[21] << 16 | buf[22] << 8 | buf[23];
					}
					else if (cur_path)
					{
						// store the coordinates of the path

						int count = buf_size / 8; // number of pairs

						cur_path->size = count;
						cur_path->pairs = new IPair[count];

						for (int n = 0; n < count; n++)
						{
							int i = 8 * n;
							int x = buf[i] << 24 | buf[i + 1] << 16 | buf[i + 2] << 8 | buf[i + 3];
							int y = buf[i + 4] << 24 | buf[i + 5] << 16 | buf[i + 6] << 8 | buf[i + 7];

							cur_path->pairs[n].x = x;
							cur_path->pairs[n].y = y;
						}
					}
					break;
				}
			case GDS_LAYER: // BOUNDARY, PATH, TEXT, NODE, BOX
				if (cur_boundary)
				{
					cur_boundary->layer = buf[0] << 8 | buf[1];
				}
				else if (cur_path)
				{
					cur_path->layer = buf[0] << 8 | buf[1];
				}

				break;
			case GDS_WIDTH: // PATH, TEXT
				if (cur_path)
				{
					cur_path->width = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
				}
				break;
			case GDS_DATATYPE:
				break;
			case GDS_TEXTNODE:
				break;
			case GDS_TEXTTYPE:
				break;
			case GDS_PRESENTATION:
				break;
			case GDS_STRING:
				break;
			case GDS_REFLIBS:
				break;
			case GDS_FONTS:
				break;
			case GDS_ATTRTABLE:
				break;
			case GDS_ELFLAGS:
				break;
			case GDS_PROPATTR:
				break;
			case GDS_PROPVALUE:
				break;
			case GDS_BOXTYPE:
				break;
			case GDS_PLEX:
				break;
			case GDS_BGNEXTN:
				break;
			case GDS_ENDEXTN:
				break;
			case GDS_FORMAT:
				break;
			}

			delete[] buf;
		}

		if (p_file)
		{
			fclose(p_file);
		}

		// Expand all path elements into polygons

		uint64_t cells_size = gds->cells.size();

		for (int i = 0; i < cells_size; i++)
		{
			GCell* cell = gds->cells.at(i);

			uint64_t size = cell->paths.size();
			for (int j = 0; j < size; j++)
			{
				Path* p = cell->paths.at(j);

				p->esize = 2 * p->size + 1;
				p->epairs = new IPair[p->esize];

				expand_path(p->epairs, p->pairs, p->size, p->width, p->pathtype);
			}
		}

		return gds;
	}

	void Release(HGDS hGds)
	{
		int i;

		DB* gds = (DB*)hGds;

		if (hGds == nullptr) return;

		uint64_t size = gds->cells.size();
		for (i = 0; i < size; i++)
		{
			GCell* cell = gds->cells.at(i);
			gds_cell_clear(cell);
			delete cell;
		}

		delete gds;
	}

	int ExtractPolygons(HGDS hGds, const std::string& cell, const double* bounds,
		std::vector<Poly*>& pvec, int (*callback)(uint64_t, uint64_t), std::wstring* msg)
	{
		Rinfo g_info;
		//memset(&g_info, 0, sizeof(g_info));

		DB* gds = (DB*)hGds;

		if (!gds)
		{
			if (msg)
			{
				*msg = L"Invalid GDS handle";
			}

			/* RETURN */
			return 0;
		}

		g_info.gds = gds;
		g_info.out_pset = &pvec;

		if (bounds != nullptr)
		{
			g_info.bbox.xmin = (int32_t)(bounds[0] / gds->uu_per_dbunit);
			g_info.bbox.xmax = (int32_t)((bounds[0] + bounds[2]) / gds->uu_per_dbunit);
			g_info.bbox.ymin = (int32_t)(bounds[1] / gds->uu_per_dbunit);
			g_info.bbox.ymax = (int32_t)((bounds[1] + bounds[3]) / gds->uu_per_dbunit);
			g_info.use_bb = 1;
			g_info.callback = callback;
			g_info.interupt = 0;
		}
		else
		{
			g_info.use_bb = 0;
		}

		// Find the gds structure to expand
		GCell* top = find_struct(gds, cell);
		if (top == nullptr)
		{
			if (msg)
			{
				*msg = std::format(L"GDS cell name not found");
			}
			return 0;
		}

		// Initial transformation of top level cell
		Trans Trans;

		Trans.x = 0;
		Trans.y = 0;
		Trans.mag = 1.0;
		Trans.angle = 0.0;
		Trans.mirror = 0;

		// Start the recursion
		collapse_cell(top, Trans, &g_info);

		return 1;
	}

	int WritePolys(HGDS hGds, const std::wstring& dest, std::vector<Poly*>& pvec, std::wstring* msg)
	{
		DB* gds = (DB*)hGds;

		FILE* pdest = nullptr;
		_wfopen_s(&pdest, dest.c_str(), L"wb");
		if (pdest == nullptr)
		{
			if (msg)
			{
				msg->assign(L"Error opening destination file");
			}
			return 0;
		}

		char access[24] = { 0 }; // Just write zero to BGNLIB and BGNSTR
		file_append_short(pdest, GDS_HEADER, 600);
		file_append_bytes(pdest, GDS_BGNLIB, access, 24);
		file_append_string(pdest, GDS_LIBNAME, "");
		file_append_bytes(pdest, GDS_UNITS, (char*)gds->units, 16);
		file_append_bytes(pdest, GDS_BGNSTR, access, 24);
		file_append_string(pdest, GDS_STRNAME, "TOP");

		uint64_t size = pvec.size();
		for (int i = 0; i < size; i++)
		{
			Poly* p = pvec.at(i);

			file_append_poly(pdest, p->pairs, p->size, p->layer);
		}

		// write the tail headers to the outfile

		file_append_record(pdest, GDS_ENDSTR);
		file_append_record(pdest, GDS_ENDLIB);

		if (pdest != nullptr)
		{
			fclose(pdest);
		}

		return 1;
	}

	void ListTopCells(HGDS hGds)
	{
		DB* gds = (DB*)hGds;

		uint64_t i;
		uint64_t count = gds->cells.size();

		if (gds == nullptr)
			return;

		wprintf(L"\nTop cell in: %s\n", gds->fpath.c_str());

		for (i = 0; i < count; i++)
		{
			GCell* struc = gds->cells.at(i);

			int is_referenced = 0;

			uint64_t j;
			for (j = 0; j < count; j++)
			{
				uint64_t ref_count;

				if (i == j) continue; // skip self

				GCell* test_struct = gds->cells.at(j);

				ref_count = test_struct->srefs.size();
				for (int k = 0; k < ref_count; k++)
				{
					if (test_struct->srefs.at(k)->sname.compare(struc->strname) == 0)
					{
						is_referenced = 1;
						break;
					}
				}
				if (is_referenced) break;

				ref_count = test_struct->arefs.size();
				for (int k = 0; k < ref_count; k++)
				{
					if (test_struct->arefs.at(k)->sname.compare(struc->strname) == 0)
					{
						is_referenced = 1;
						break;
					}
				}
				if (is_referenced) break;
			}

			if (!is_referenced)
			{
				std::cout << "--> " << struc->strname << "\n";
			}


		}

		std::cout << "\n";
	}

	void ListAllCells(HGDS hGds)
	{
		DB* gds = (DB*)hGds;

		uint64_t i;
		uint64_t count = gds->cells.size();

		if (gds == nullptr)
			return;

		wprintf(L"\nAll cells in: %s\n", gds->fpath.c_str());

		for (i = 0; i < count; i++)
		{
			GCell* struc = gds->cells.at(i);

			std::cout << "--> " << struc->strname << "\n";
		}

		std::cout << "\n";
	}

	std::wstring& GetFilepath(HGDS hGds)
	{
		DB* gds = (DB*)hGds;

		return gds->fpath;
	}

	float Getuu(HGDS hGds)
	{
		DB* gds = (DB*)hGds;

		return (float)gds->uu_per_dbunit;
	}

	void PolysetRelease(std::vector<Poly*>& pset)
	{
		uint64_t size = pset.size();
		for (int i = 0; i < size; i++)
		{
			Poly* p = pset.at(i);
			delete[] p->pairs;
			delete p;
		}
	}

	int PointInPolygon(IPair* poly, int n, IPair p)
	{
		/**
		* Evaluate if test point P is inside the polygon @poly.
		* Returns true if yes and false if no
		*
		* Caller needs to guarantee @poly != nullptr
		*/

		int count, i;

		// counts the segments crossed by a vertical line downwards from test point P
		count = 0;
		for (i = 0; i < n - 1; i++)
		{

			// Check if segment i will cross a vertical line through the test point
			if (((poly[i].x <= p.x) && (poly[i + 1].x > p.x)) || ((poly[i].x > p.x) && (poly[i + 1].x <= p.x)))
			{

				// add count if it crosses
				if (p.y < poly[i].y + ((p.x - poly[i].x) * (poly[i + 1].y - poly[i].y)) / (poly[i + 1].x - poly[i].x))
				{
					count++;
				}
			}
		}

		// if the coint is off the point is outside the polygon
		return (count % 2);
	}
}
