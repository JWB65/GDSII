/*
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
*/

#include "gds.h"
#include "parray.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gds_bndry {
	// GDS BOUNDARY element

	uint16_t layer;

	struct gds_ipair* pairs;
	int size;
};

struct gds_path {
	// GDS PATH element

	uint16_t layer;

	struct gds_ipair* pairs;
	int size;

	// Expanded path
	struct gds_ipair* epairs;
	int esize;

	uint16_t pathtype;
	uint32_t width;
};

struct gds_sref {
	// GDS SREF element

	int x, y;

	char sname[GDS_MAX_STR_NAME + 1];

	// Pointer to the structure referenced
	struct gcell* cell;

	uint16_t strans;
	float mag, angle;
};

struct gds_aref {
	// GDS AREF element

	int32_t x1, y1, x2, y2, x3, y3;

	char sname[GDS_MAX_STR_NAME + 1];

	// Pointer to the structure referenced
	struct gcell* cell;

	int col, row;

	uint16_t strans;
	float mag, angle;
};

struct gcell {
	// GDS structure (or here named "cell")

	char strname[GDS_MAX_STR_NAME + 1];

	parray* boundaries;
	parray* paths;
	parray* srefs;
	parray* arefs;
};

struct gds_db {
	// GDS database

	parray* cells;

	char* file;
	int file_len;

	uint16_t version;

	// Size of db unit in user units
	// Size of db unit in meter
	// (1) / (2) is the size of the user unit in meters
	double uu_per_dbunit, meter_per_dbunit;

	// UNITS record in binary form
	uint8_t units[16];
} gds_db;

enum GDS_RECORD {
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

// Standard form of a line
struct gds_line {
	double a, b, c;
};

// Transformation parameters
struct gds_trans {
	int x, y;
	float mag, angle;
	short mirror;
};

struct bb {
	int32_t xmin;
	int32_t xmax;
	int32_t ymin;
	int32_t ymax;
};

// File global variable with information that otherwise needed to be passed
// during recursion
static struct rinfo {
	parray* out_pset;
	struct gds_db* gds;
	struct bb bb;
	int use_bb;
	uint64_t scount, pcount, max_polys;
	int (*callback)(uint64_t, uint64_t);
	int interupt;
} g_info;

// Converts buffer into a double in the format used by the GDS standard
static double buf_read_float(unsigned char* p)
{
	int i, sign, exp;
	double fraction;

	// The binary representation is with 7 bits in the exponent and 56 bits in
	// the fraction:
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
	if (p[0] > 127) {
		sign = -1;
		exp = p[0] - 192;
	} else {
		exp = p[0] - 64;
	}

	fraction = 0.0;
	for (i = 0; i < 7; ++i) {
		fraction += p[i + 1] / (double)div[i];
	}

	return (pow(16, exp) * sign * fraction);
}

// Writes int to buffer in format used by the GDS standard
static void buf_write_int(char* p, int offset, int n)
{
	p[offset + 0] = (n >> 24) & 0xFF;
	p[offset + 1] = (n >> 16) & 0xFF;
	p[offset + 2] = (n >> 8) & 0xFF;
	p[offset + 3] = n & 0xFF;
}

// Find cell with name @name in @gds
static struct gcell* find_struct(struct gds_db* gds, char* name)
{
	uint64_t i;

	uint64_t size = parray_size(gds->cells);
	for (i = 0; i < size; i++)
	{
		struct gcell* s;

		s = parray_get(gds->cells, i);
		if (strcmp(s->strname, name) == 0)
			return s;
	}

	return NULL;
}

// Appends generic GDS record to file
static void file_append_record(FILE* file, uint16_t record)
{
	char buf[4];

	buf[0] = 0;
	buf[1] = 4;
	buf[2] = (record >> 8) & 0xFF;
	buf[3] = record & 0xFF;

	fwrite(buf, 4, 1, file);
}

// Appends record with short to file
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

// Appends record with array of bytes to file
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

// Appends record with string to file (pad with zero if needed)
static void file_append_string(FILE* file, uint16_t record, char* string)
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
	if (text_len % 2) putc(0, file);
}

// Append polygon to GDS file
static void file_append_poly(FILE* pfile, struct gds_ipair* p, int size, uint16_t layer)
{
	// store in buffer in the format of the gds standard
	char* buf = malloc(8 * size * sizeof(char));

	if (buf == NULL)
		return;

	for (int i = 0; i < size; i++) {
		buf_write_int(buf, 8 * i, p[i].x);
		buf_write_int(buf, 8 * i + 4, p[i].y);
	}

	file_append_record(pfile, GDS_BOUNDARY);
	file_append_short(pfile, GDS_LAYER, layer);
	file_append_short(pfile, GDS_DATATYPE, 0);
	file_append_bytes(pfile, GDS_XY, buf, 8 * size);
	file_append_record(pfile, GDS_ENDEL);

	free(buf);
}

// Append polygon to polygon set
static void polyset_add_poly(parray* pset, struct gds_ipair* in, int size, uint16_t layer)
{
	if (in == NULL || pset == NULL)
		return;

	// Copy input polygon
	struct gds_ipair* pnew = malloc(size * sizeof(struct gds_ipair));
	memcpy(pnew, &in[0], size * sizeof(struct gds_ipair));

	// Add to the polygon structure with the layer name
	struct gds_poly* poly = malloc(sizeof(struct gds_poly));
	poly->pairs = pnew;
	poly->size = (uint16_t)size;
	poly->layer = layer;

	parray_add(pset, poly);
}

// Intersection between two lines in the standard form
static struct gds_ipair line_intersection(struct gds_line* one, struct gds_line* two)
{
	double xh, yh, wh;

	// line-line intersection (homogenous coordinates):
	//		(B1C2 - B2C1, A2C1 - A1C2, A1B2 - A2B1)
	xh = one->b * two->c - two->b * one->c;
	yh = two->a * one->c - one->a * two->c;
	wh = one->a * two->b - two->a * one->b;

	return (struct gds_ipair) { (int)(xh / wh), (int)(yh / wh) };
}

// Project a point onto a line
static struct gds_ipair line_project(struct gds_ipair p, struct gds_line* line)
{
	struct gds_line normal;

	// normal to Ax+By+C = 0 through (x1, y1)
	// A'x+B'y+C' = 0 with
	// A' = B  B' = -A  C' = Ay1 - Bx1

	normal = (struct gds_line){ line->b, -line->a, line->a * p.y -
		line->b * p.x };

	// intersect the normal through (p.x, p.y) with line
	return line_intersection(line, &normal);
}

// Extends a vector given by @tail and @head in the tail direction by @length
static struct gds_ipair extend_vector(struct gds_ipair tail, struct gds_ipair head, double length)
{
	double segx, segy, norm;
	struct gds_ipair out = { tail.x, tail.y };

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

// Sums two pairs
static struct gds_ipair vec_sum(struct gds_ipair one, struct gds_ipair two)
{
	return (struct gds_ipair) { one.x + two.x, one.y + two.y };
}

// Expands a GDS path into a simple polygon
static void expand_path(struct gds_ipair* pout, struct gds_ipair* pin, int size, int width,
	int pathtype)
{
	int i;
	double hwidth = width / 2.0;
	struct gds_line* mlines, * plines;
	struct gds_ipair end_point;

	if (size < 2) return;

	// parallel line segments on both sides
	mlines = malloc((size - 1) * sizeof(struct gds_line));
	plines = malloc((size - 1) * sizeof(struct gds_line));
	if (!mlines || !plines) return;

	for (i = 0; i < size - 1; i++) {
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

	// head points
	if (pathtype == 2) {
		end_point = extend_vector(pin[0], pin[1], hwidth);
	} else {
		end_point = pin[0];
	}
	pout[0] = line_project(end_point, &plines[0]);
	pout[2 * size - 1] = line_project(end_point, &mlines[0]);
	pout[2 * size] = pout[0];

	// middle points
	for (i = 1; i < size - 1; i++) {
		pout[i] = line_intersection(&plines[i - 1], &plines[i]);
		pout[2 * size - 1 - i] = line_intersection(&mlines[i - 1],
			&mlines[i]);
	}

	// tail points
	if (pathtype == 2) {
		end_point = extend_vector(pin[size - 1], pin[size - 2], hwidth);
	} else {
		end_point = pin[size - 1];
	}
	pout[size - 1] = line_project(end_point, &plines[size - 2]);
	pout[size] = line_project(end_point, &mlines[size - 2]);

	free(plines);
	free(mlines);
}

// Cursory test if poly overlaps with bounding box in global g_info structure
static int poly_overlap_test(struct gds_ipair* p, int size)
{
	// Returns false if certainly no overlap

	// The final (closing) point of poly is not needed for this evaluation

	int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;

	for (int i = 0; i < size - 1; i++) {
		if (p[i].x > maxx)  maxx = p[i].x;
	}
	if (maxx < g_info.bb.xmin) return 0;

	for (int i = 0; i < size - 1; i++) {
		if (p[i].y > maxy)  maxy = p[i].y;
	}
	if (maxy < g_info.bb.ymin) return 0;

	for (int i = 0; i < size - 1; i++) {
		if (p[i].x < minx)  minx = p[i].x;
	}
	if (minx > g_info.bb.xmax) return 0;

	for (int i = 0; i < size - 1; i++) {
		if (p[i].y < miny)  miny = p[i].y;
	}
	if (miny > g_info.bb.ymax) return 0;

	return 1;
}

// Add polygon to the file and/or polygon set
static void add_poly(struct gds_ipair* pairs, int size, uint16_t layer)
{
	g_info.scount++;

	if (!g_info.use_bb || poly_overlap_test(pairs, size)) {


		// Define the location of the bounding box as the origin of the collapsed cell
		for (int i = 0; i < size; i++) {
			pairs[i].x = pairs[i].x - g_info.bb.xmin;
			pairs[i].y = pairs[i].y - g_info.bb.ymin;
		}

		polyset_add_poly(g_info.out_pset, pairs, size, layer);

		// pcount is the polygon counter
		g_info.pcount++;
	}

	if (g_info.scount % 1000000 == 0)
	{
		int result = g_info.callback(g_info.pcount, g_info.scount);
		if (result)
			g_info.interupt = 1;
	}
}

// Perform a transformation: reflection -> rotation -> scaling -> translation
static void transform_poly(struct gds_ipair* pout, struct gds_ipair* pin, int size,
	struct gds_trans tra)
{
	// Reflect with respect to x axis before rotation
	float sign = 1.f;
	if (tra.mirror)
		sign = -1.f;

	float s = sinf(tra.angle);
	float c = cosf(tra.angle);

	for (int i = 0; i < size; i++) {
		pout[i].x = tra.x + (int)(tra.mag * (pin[i].x * c - sign * pin[i].y * s));
		pout[i].y = tra.y + (int)(tra.mag * (pin[i].x * s + sign * pin[i].y * c));
	}
}

// Perform a transformation: reflection -> rotation -> scaling -> translation
static struct gds_ipair transform_point(struct gds_ipair in, struct gds_trans tra)
{
	// Reflect with respect to x axis before rotation
	float sign = 1.f;
	if (tra.mirror)
		sign = -1.f;

	float s = sinf(tra.angle);
	float c = cosf(tra.angle);

	struct gds_ipair out;
	out.x = (int)(tra.x + tra.mag * (in.x * c - sign * in.y * s));
	out.y = (int)(tra.y + tra.mag * (in.x * s + sign * in.y * c));

	return out;
}

// Collapse cell
static void collapse_cell(struct gcell* top, struct gds_trans tra)
{
	struct gds_ipair out[400];

	if (g_info.pcount >= g_info.max_polys)
		return;

	// BOUNDARY elements
	uint64_t size = parray_size(top->boundaries);
	for (int i = 0; i < size; i++) {
		struct gds_bndry* b = parray_get(top->boundaries, i);

		transform_poly(out, b->pairs, b->size, tra);
		add_poly(out, b->size, b->layer);

		if (g_info.pcount >= g_info.max_polys || g_info.interupt)
			return;
	}

	// PATH elements
	size = parray_size(top->paths);
	for (int i = 0; i < size; i++) {

		struct gds_path* p = parray_get(top->paths, i);

		// Expand and transform
		transform_poly(out, p->epairs, p->esize, tra);
		add_poly(out, p->esize, p->layer);

		if (g_info.pcount >= g_info.max_polys || g_info.interupt)
			return;
	}

	// SREF elements
	size = parray_size(top->srefs);
	for (int i = 0; i < size; i++) {
		struct gds_sref* p = ((struct gds_sref*)parray_get(top->srefs, i));
		//struct gds_cell* str = find_struct(g_info.gds, p->sname);

		if (!p->cell) {
			p->cell = find_struct(g_info.gds, p->sname);
			if (!p->cell) continue;
		}

		struct gcell* str = p->cell;

		// Calculate the origin of the sub cell with the accumulated transformation
		struct gds_ipair ori;
		ori = transform_point((struct gds_ipair) { p->x, p->y }, tra);

		// Accumulate the transformation for vertices in the sub cell
		struct gds_trans acc_tra;
		acc_tra = (struct gds_trans){
			.x = ori.x,
			.y = ori.y,
			.mag = tra.mag * p->mag,
			.angle = tra.angle + p->angle,
			.mirror = tra.mirror ^ (p->strans & 0x8000)
		};

		// Deeper level
		collapse_cell(str, acc_tra);

		if (g_info.pcount >= g_info.max_polys || g_info.interupt)
			return;
	}

	// Expand AREF elements
	size = parray_size(top->arefs);
	for (int i = 0; i < size; i++) {
		struct gds_aref* p;

		p = ((struct gds_aref*)parray_get(top->arefs, i));

		if (!p->cell) {
			p->cell = find_struct(g_info.gds, p->sname);
			if (!p->cell) continue;
		}

		struct gcell* str = p->cell;

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
		for (int c = 0; c < p->col; c++) {
			for (int r = 0; r < p->row; r++) {

				// Position of the sub cell being referenced
				float x_ref = (p->x1 + c * v_col_x + r * v_row_x);
				float y_ref = (p->y1 + c * v_col_y + r * v_row_y);

				// Origin of the sub cell with the accumulated transformation

				int x = (int)(tra.x + tra.mag * (x_ref * cos - sign * y_ref * sin));
				int y = (int)(tra.y + tra.mag * (x_ref * sin + sign * y_ref * cos));

				// Cell referenced still needs to be transformed
				struct gds_trans acc_tra;
				acc_tra = (struct gds_trans){
					.x = x,
					.y = y,
					.mag = tra.mag * p->mag,
					.angle = tra.angle + p->angle,
					.mirror = tra.mirror ^ (p->strans & 0x8000)
				};

				// Down a level
				collapse_cell(str, acc_tra);

				if (g_info.pcount >= g_info.max_polys || g_info.interupt)
					return;
			}
		}
	}
}

// Free memory allocated in GDS structure @s
static void gds_cell_clear(struct gcell* s)
{
	uint64_t i;

	uint64_t size = parray_size(s->srefs);
	for (i = 0; i < size; i++) {
		free(((struct gds_sref*)parray_get(s->srefs, i)));
	}
	parray_release(s->srefs);

	size = parray_size(s->arefs);
	for (i = 0; i < size; i++) {
		free(((struct gds_aref*)parray_get(s->arefs, i)));
	}
	parray_release(s->arefs);

	size = parray_size(s->boundaries);
	for (i = 0; i < size; i++) {
		free(((struct gds_bndry*)parray_get(s->boundaries, i))->pairs);
		free(((struct gds_bndry*)parray_get(s->boundaries, i)));
	}
	parray_release(s->boundaries);

	size = parray_size(s->paths);
	for (i = 0; i < size; i++) {
		free(((struct gds_path*)parray_get(s->paths, i))->pairs);
		free(((struct gds_path*)parray_get(s->paths, i))->epairs);
		free(((struct gds_path*)parray_get(s->paths, i)));
	}
	parray_release(s->paths);
}

// User functions

HGDS gds_db_create(const char* file, char* error, int elen)
{
	struct gds_db* gds = malloc(sizeof(struct gds_db));

	gds->cells = parray_create();

	// current GDS structure being read
	struct gcell* cur_str = NULL;

	// the different elements in a GDS structure
	struct gds_bndry* cur_boundary = NULL;
	struct gds_path* cur_path = NULL;
	struct gds_sref* cur_sref = NULL;
	struct gds_aref* cur_aref = NULL;

	FILE* p_file = NULL;
	fopen_s(&p_file, file, "rb");

	if (!p_file)
	{
		snprintf(error, elen, "%s", "Open file error");
		return NULL;
	}

	gds->file_len = (int)strlen(file);
	gds->file = malloc(gds->file_len + 1);
	strcpy_s(gds->file, gds->file_len + 1, file);

	uint64_t read = 0; // number of bytes read
	uint64_t scount = 0; // number of structures read

	int endlib = 0;
	uint8_t header[4];
	while (!endlib && fread(header, 1, 4, p_file) == 4)
	{
		uint16_t buf_size, record_len, record_type;

		read += 4;

		// first 2 bytes: record length; second 2 bytes record type and data type
		record_len = (header[0] << 8) | header[1];
		record_type = header[2] << 8 | header[3];

		// the size of the additional data in bytes
		buf_size = record_len - 4;

		// read the data
		uint8_t* buf = NULL;
		if (buf_size > 0)
		{
			buf = malloc(buf_size * sizeof(uint8_t));
			read += fread(buf, 1, buf_size, p_file);
		}

		// handle the GDS records
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
			scount++;

			cur_str = calloc(1, sizeof(struct gcell));

			cur_str->boundaries = parray_create();
			cur_str->paths = parray_create();
			cur_str->srefs = parray_create();
			cur_str->arefs = parray_create();

			break;
		case GDS_ENDSTR:

			parray_add(gds->cells, cur_str);
			cur_str = NULL;

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
				// strname is already filled with zeros
				memcpy_s(cur_str->strname, GDS_MAX_STR_NAME + 1, buf, buf_size);

				break;
			}
		case GDS_BOUNDARY:

			// create a boundary element on the heap
			cur_boundary = calloc(1, sizeof(struct gds_bndry));

			break;
		case GDS_PATH:

			// create a path element on the heap
			cur_path = calloc(1, sizeof(struct gds_path));

			break;
		case GDS_SREF:

			// create a sref element on the heap
			cur_sref = calloc(1, sizeof(struct gds_sref));
			cur_sref->mag = 1.0; // default

			break;
		case GDS_AREF:

			// create a aref element on the heap
			cur_aref = calloc(1, sizeof(struct gds_aref));
			cur_aref->mag = 1.0; // default

			break;
		case GDS_TEXT:
			break;
		case GDS_NODE:
			break;
		case GDS_BOX:
			break;
		case GDS_ENDEL:
			// add element to the current structure

			if (cur_boundary)
			{
				parray_add(cur_str->boundaries, cur_boundary);
				cur_boundary = NULL;
			} else if (cur_path) {
				parray_add(cur_str->paths, cur_path);
				cur_path = NULL;
			} else if (cur_sref) {
				parray_add(cur_str->srefs, cur_sref);
				cur_sref = NULL;
			} else if (cur_aref) {
				parray_add(cur_str->arefs, cur_aref);
				cur_aref = NULL;
			}

			break;
		case GDS_SNAME: // SREF, AREF

			if (cur_sref)
			{
				memcpy_s(cur_sref->sname, GDS_MAX_STR_NAME + 1, buf, buf_size);
				cur_sref->sname[buf_size] = 0;
			} else if (cur_aref) {
				memcpy_s(cur_aref->sname, GDS_MAX_STR_NAME + 1, buf, buf_size);
				cur_aref->sname[buf_size] = 0;
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
			} else if (cur_aref) {
				cur_aref->strans = buf[0] << 8 | buf[1];
			}

			break;
		case GDS_ANGLE: // SREF, AREF, TEXT

			if (cur_sref)
			{
				cur_sref->angle = (float)(M_PI * buf_read_float(buf) / 180.0);
			} else if (cur_aref) {
				cur_aref->angle = (float)(M_PI * buf_read_float(buf) / 180.0);
			}

			break;
		case GDS_MAG: // SREF, AREF, TEXT
			//printf("--> MAG read\n");

			if (cur_sref)
			{
				cur_sref->mag = (float)buf_read_float(buf);
			} else if (cur_aref) {
				cur_aref->mag = (float)buf_read_float(buf);
			}

			break;
		case GDS_XY:
			{
				if (cur_boundary)
				{
					// store the boundary coordinates

					int count = buf_size / 8; // number of pairs

					cur_boundary->size = count;
					cur_boundary->pairs = malloc(count * sizeof(struct gds_ipair));

					for (int n = 0; n < count; n++) {
						int i = 8 * n;
						int x = buf[i] << 24 | buf[i + 1] << 16 | buf[i + 2] << 8 | buf[i + 3];
						int y = buf[i + 4] << 24 | buf[i + 5] << 16 | buf[i + 6] << 8 | buf[i + 7];

						cur_boundary->pairs[n].x = x;
						cur_boundary->pairs[n].y = y;
					}
				} else if (cur_sref) {
					// store the sref coordinates

					cur_sref->x = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
					cur_sref->y = buf[4] << 24 | buf[5] << 16 | buf[6] << 8 | buf[7];
				} else if (cur_aref) {
					// store the 3 vectors of the aref

					cur_aref->x1 = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
					cur_aref->y1 = buf[4] << 24 | buf[5] << 16 | buf[6] << 8 | buf[7];
					cur_aref->x2 = buf[8] << 24 | buf[9] << 16 | buf[10] << 8 | buf[11];
					cur_aref->y2 = buf[12] << 24 | buf[13] << 16 | buf[14] << 8 | buf[15];
					cur_aref->x3 = buf[16] << 24 | buf[17] << 16 | buf[18] << 8 | buf[19];
					cur_aref->y3 = buf[20] << 24 | buf[21] << 16 | buf[22] << 8 | buf[23];
				} else if (cur_path) {
					// store the coordinates of the path

					int count = buf_size / 8; // number of pairs

					cur_path->size = count;
					cur_path->pairs = malloc(count * sizeof(struct gds_ipair));

					for (int n = 0; n < count; n++) {
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
			} else if (cur_path) {
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

		free(buf);
	}

	if (p_file)
		fclose(p_file);

	// expand all path elements into polygons

	uint64_t cells_size = parray_size(gds->cells);
	for (int i = 0; i < cells_size; i++)
	{
		struct gcell* cell = parray_get(gds->cells, i);

		uint64_t size = parray_size(cell->paths);
		for (int j = 0; j < size; j++)
		{
			struct gds_path* p = parray_get(cell->paths, j);

			p->esize = 2 * p->size + 1;
			p->epairs = malloc(p->esize * sizeof(struct gds_ipair));

			expand_path(p->epairs, p->pairs, p->size, p->width, p->pathtype);
		}
	}

	return gds;
}

void gds_db_release(HGDS hGds)
{
	int i;

	struct gds_db* gds = (struct gds_db*)hGds;

	if (hGds == NULL) return;

	uint64_t size = parray_size(gds->cells);
	for (i = 0; i < size; i++)
	{
		struct gcell* cell = parray_get(gds->cells, i);
		gds_cell_clear(cell);
		free(cell);
	}

	free(gds->file);
	parray_release(gds->cells);

	free(gds);
}

int gds_collapse(HGDS hGds, const char* cell, const double* bounds, uint64_t max_polys,
	parray* pvec, int (*callback)(uint64_t, uint64_t), char* error, int elen)
{
	memset(&g_info, 0, sizeof(g_info));

	struct gds_db* gds = (struct gds_db*)hGds;


	if (cell == NULL)
	{
		snprintf(error, elen, "Invalid cell argument");
		return 0;
	}

	if (gds == NULL)
	{
		snprintf(error, elen, "Invalid GDS handle");
		return 0;
	}

	g_info.gds = gds;
	g_info.max_polys = max_polys;
	g_info.out_pset = pvec;

	if (bounds != NULL) {
		g_info.bb.xmin = (int32_t)(bounds[0] / gds->uu_per_dbunit);
		g_info.bb.xmax = (int32_t)((bounds[0] + bounds[2]) / gds->uu_per_dbunit);
		g_info.bb.ymin = (int32_t)(bounds[1] / gds->uu_per_dbunit);
		g_info.bb.ymax = (int32_t)((bounds[1] + bounds[3]) / gds->uu_per_dbunit);
		g_info.use_bb = 1;
		g_info.callback = callback;
		g_info.interupt = 0;
	} else {
		g_info.use_bb = 0;
	}

	// Find the gds structure to expand
	struct gcell* top = find_struct(gds, (char*)cell);
	if (top == NULL)
	{
		snprintf(error, elen, "GDS cell %s not found", cell);
		return 0;
	}

	// Initial transformation of top level cell
	struct gds_trans trans = (struct gds_trans){ .x = 0, .y = 0, .mag = 1.0, .angle = 0.0,
		.mirror = 0 };

	// start the recursion
	collapse_cell(top, trans);

	return 1;
}

int gds_write(HGDS hGds, const char* dest, parray* pset, char* error, int elen)
{
	struct gds_db* gds = (struct gds_db*)hGds;

	if (dest == NULL)
	{
		snprintf(error, elen, "No destination file given");
		return 0;
	}

	if (pset == NULL)
	{
		snprintf(error, elen, "No output polyset given");
		return 0;
	}

	FILE* pdest = NULL;

	fopen_s(&pdest, dest, "wb");
	if (pdest == NULL)
	{
		snprintf(error, elen, "Error opening destination file");
		return 0;
	}

	char access[24] = { 0 }; // Just write zero to BGNLIB and BGNSTR
	file_append_short(pdest, GDS_HEADER, 600);
	file_append_bytes(pdest, GDS_BGNLIB, access, 24);
	file_append_string(pdest, GDS_LIBNAME, "");
	file_append_bytes(pdest, GDS_UNITS, (char*)gds->units, 16);
	file_append_bytes(pdest, GDS_BGNSTR, access, 24);
	file_append_string(pdest, GDS_STRNAME, "TOP");

	uint64_t size = parray_size(pset);
	for (int i = 0; i < size; i++) {
		struct gds_poly* p = parray_get(pset, i);

		file_append_poly(pdest, p->pairs, p->size, p->layer);
	}

	// write the tail headers to the outfile
	if (dest != NULL) {
		file_append_record(pdest, GDS_ENDSTR);
		file_append_record(pdest, GDS_ENDLIB);
	}

	if (pdest != NULL) {
		fclose(pdest);
	}

	return 1;
}

int gds_poly_contains_point(struct gds_ipair* poly, int n, struct gds_ipair p)
{
	/**
	* Evaluate if test point P is inside the polygon @poly.
	* Returns true if yes and false if no
	*
	* Caller needs to guarantee @poly != NULL
	*/

	int count, i;

	// counts the segment crossed by a vertical line downwards from test point P
	count = 0;
	for (i = 0; i < n - 1; i++) {

		// Check if segment i will cross a vertical line through the test point
		if (((poly[i].x <= p.x) && (poly[i + 1].x > p.x)) || ((poly[i].x > p.x) && (poly[i + 1].x <= p.x))) {

			// add count if it crosses
			if (p.y < poly[i].y + ((p.x - poly[i].x) * (poly[i + 1].y - poly[i].y)) / (poly[i + 1].x - poly[i].x)) {
				count++;
			}
		}
	}

	// if the coint is off the point is outside the polygon
	return (count % 2);
}

void gds_top_cells(HGDS hGds)
{
	struct gds_db* gds = (struct gds_db*)hGds;

	uint64_t i;
	uint64_t count = parray_size(gds->cells);

	if (gds == NULL)
		return;

	printf("\nTop cell in: %s\n", gds->file);

	for (i = 0; i < count; i++) {
		struct gcell* struc = parray_get(gds->cells, i);

		int is_referenced = 0;

		uint64_t j;
		for (j = 0; j < count; j++) {
			uint64_t ref_count;

			if (i == j) continue; // skip self

			struct gcell* test_struct = parray_get(gds->cells, j);

			ref_count = parray_size(test_struct->srefs);
			for (int k = 0; k < ref_count; k++) {
				if (strcmp(((struct gds_sref*)parray_get(test_struct->srefs, k))->sname, struc->strname) == 0) {
					is_referenced = 1;
					break;
				}
			}
			if (is_referenced) break;

			ref_count = parray_size(test_struct->arefs);
			for (int k = 0; k < ref_count; k++) {
				if (strcmp(((struct gds_aref*)parray_get(test_struct->arefs, k))->sname, struc->strname) == 0) {
					is_referenced = 1;
					break;
				}
			}
			if (is_referenced) break;
		}

		if (!is_referenced) {
			printf("--> %s\n", struc->strname);
		}


	}


	printf("\n");
}

void gds_all_cells(HGDS hGds)
{
	struct gds_db* gds = (struct gds_db*)hGds;

	uint64_t i;
	uint64_t count = parray_size(gds->cells);

	if (gds == NULL)
		return;

	printf("\nAll cells in: %s\n", gds->file);

	for (i = 0; i < count; i++)
	{
		struct gcell* struc = parray_get(gds->cells, i);
		printf("--> %s\n", struc->strname);
	}

	printf("\n");
}

void gds_polyset_release(parray* pset)
{
	uint64_t size = parray_size(pset);
	for (int i = 0; i < size; i++)
	{
		gds_poly* p = parray_get(pset, i);
		free(p->pairs);
		free(p);
	}
}

char* gds_getfile(HGDS hGds)
{
	struct gds_db* gds = (struct gds_db*)hGds;

	return gds->file;
}

float gds_getuu(HGDS hGds)
{
	struct gds_db* gds = (struct gds_db*)hGds;

	return (float) gds->uu_per_dbunit;
}
