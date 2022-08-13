/*
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
*/

#include "gds.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>
#include <string.h>

//#define GDS_TRANSLATE
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

// Data to pass during recursion
struct rinfo {
	FILE* out_file;
	vec* out_pset;
	struct gds_db* gds;
	struct gds_ipair* bb;
	uint64_t scount, pcount, max_polys;
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
static void buf_write_int(char* p, int offset, int n) {
	p[offset + 0] = (n >> 24) & 0xFF;
	p[offset + 1] = (n >> 16) & 0xFF;
	p[offset + 2] = (n >> 8) & 0xFF;
	p[offset + 3] = n & 0xFF;
}

// Find cell with name @name in @gds
static struct gds_cell* find_struct(struct gds_db* gds, char* name)
{
	uint64_t i;

	for (i = 0; i < gds->cells->size; i++) {
		struct gds_cell* s;

		s = vec_get(gds->cells, i);
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


// Add polygon to polygon set
static void polyset_add_poly(vec* pset, struct gds_ipair* in, int size, uint16_t layer)
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

	vec_add(pset, poly);
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

	if (norm != 0.0) {
		out.x = tail.x + (int)((length / norm) * segx);
		out.y = tail.y + (int)((length / norm) * segy);
	}

	return out;
}

static struct gds_ipair vec_sum(struct gds_ipair one, struct gds_ipair two)
{
	return (struct gds_ipair) { one.x + two.x, one.y + two.y };
}


static void expand_path(struct gds_ipair* pout, struct gds_ipair* pin, int size, int width, int pathtype)
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
static bool poly_overlap_test(struct gds_ipair* p, int size)
{
	// Returns false if certainly no overlap

	// The final (closing) point of poly is not needed for this evaluation

	int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;

	for (int i = 0; i < size - 1; i++) {
		if (p[i].x > maxx)  maxx = p[i].x;
	}
	if (maxx < g_info.bb[0].x) return false;

	for (int i = 0; i < size - 1; i++) {
		if (p[i].y > maxy)  maxy = p[i].y;
	}
	if (maxy < g_info.bb[0].y) return false;

	for (int i = 0; i < size - 1; i++) {
		if (p[i].x < minx)  minx = p[i].x;
	}
	if (minx > g_info.bb[2].x) return false;

	for (int i = 0; i < size - 1; i++) {
		if (p[i].y < miny)  miny = p[i].y;
	}
	if (miny > g_info.bb[2].y) return false;

	return true;
}

// Add polygon to the file and/or polygon set
static void add_poly(struct gds_ipair* pairs, int size, uint16_t layer)
{
	g_info.scount++;

	if (!g_info.bb || poly_overlap_test(pairs, size)) {

#ifdef GDS_TRANSLATE
		// Define the location of the bounding box as the origin of the collapsed cell
		for (int i = 0; i < size; i++) {
			pairs[i].x = pairs[i].x - g_info.bb[0].x;
			pairs[i].y = pairs[i].y - g_info.bb[0].y;
		}
#endif

		if (g_info.out_file) {
			file_append_poly(g_info.out_file, pairs, size, layer);
		}

		if (g_info.out_pset) {
			polyset_add_poly(g_info.out_pset, pairs, size, layer);
		}

		// pcount is the polygon counter
		g_info.pcount++;
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
static void collapse_cell(struct gds_cell* top, struct gds_trans tra)
{
	struct gds_ipair out[400];

	if (g_info.pcount >= g_info.max_polys)
		return;

	// BOUNDARY elements
	for (int i = 0; i < top->boundaries->size; i++) {
		struct gds_bndry* b = vec_get(top->boundaries, i);

		transform_poly(out, b->pairs, b->size, tra);
		add_poly(out, b->size, b->layer);

		if (g_info.pcount >= g_info.max_polys)
			return;
	}

	// PATH elements
	for (int i = 0; i < top->paths->size; i++) {

		struct gds_path* p = vec_get(top->paths, i);

		// Expand and transform
		transform_poly(out, p->epairs, p->esize, tra);
		add_poly(out, p->esize, p->layer);

		if (g_info.pcount >= g_info.max_polys)
			return;
	}

	// SREF elements
	for (int i = 0; i < top->srefs->size; i++) {
		struct gds_sref* p = ((struct gds_sref*)vec_get(top->srefs, i));
		//struct gds_cell* str = find_struct(g_info.gds, p->sname);

		if (!p->cell) {
			p->cell = find_struct(g_info.gds, p->sname);
			if (!p->cell) continue;
		}

		struct gds_cell* str = p->cell;

		// Calculate the origin of the sub cell with the accumulated transformation
		struct gds_ipair ori;
		ori = transform_point ((struct gds_ipair) { p->x , p->y }, tra);

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

		if (g_info.pcount >= g_info.max_polys)
			return;
	}

	// Expand AREF elements
	for (int i = 0; i < top->arefs->size; i++) {
		struct gds_aref* p;

		p = ((struct gds_aref*)vec_get(top->arefs, i));

		if (!p->cell) {
			p->cell = find_struct(g_info.gds, p->sname);
			if (!p->cell) continue;
		}

		struct gds_cell* str = p->cell;

		// (v_col_x, v_col_y) vector in column direction
		double v_col_x = ((double)(p->x2 - p->x1)) / p->col;
		double v_col_y = ((double)(p->y2 - p->y1)) / p->col;

		// (v_row_x, v_row_y) vector in row direction
		double v_row_x = ((double)(p->x3 - p->x1)) / p->row;
		double v_row_y = ((double)(p->y3 - p->y1)) / p->row;

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

				if (g_info.pcount >= g_info.max_polys)
					return;
			}
		}
	}
}

// Free memory allocated in GDS structure @s

GDS_ERROR gds_db_new(struct gds_db** gds, const char* file)
{
	// current GDS structure being read
	struct gds_cell* cur_str = NULL;

	// the different elements in a GDS structure
	struct gds_bndry* cur_boundary = NULL;
	struct gds_path* cur_path = NULL;
	struct gds_sref* cur_sref = NULL;
	struct gds_aref* cur_aref = NULL;

	FILE* p_file = NULL;
	fopen_s(&p_file, file, "rb");
	if (!p_file)
		return FILE_ERROR;

	*gds = calloc(1, sizeof(struct gds_db));
	(*gds)->cells = vec_new();
	(*gds)->file = file;

	uint64_t read = 0; // number of bytes read
	uint64_t scount = 0; // number of structures read

	int endlib = 0;
	uint8_t header[4];
	while (!endlib && fread(header, 1, 4, p_file) == 4) {
		uint16_t buf_size, record_len, record_type;

		//fread(header, 1, 4, p_file);
		read += 4;

		// first 2 bytes: record length; second 2 bytes record type and data type
		record_len = (header[0] << 8) | header[1];
		record_type = header[2] << 8 | header[3];

		// the size of the additional data in bytes
		buf_size = record_len - 4;

		// read the data
		uint8_t* buf = NULL;
		if (buf_size > 0) {
			buf = malloc(buf_size * sizeof(uint8_t));
			read += fread(buf, 1, buf_size, p_file);
		}

		// handle the GDS records
		switch (record_type) {
		case GDS_HEADER:
			(*gds)->version = buf[0] << 8 | buf[1];
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

			cur_str = calloc(1, sizeof(struct gds_cell));
			cur_str->boundaries = vec_new();
			cur_str->paths = vec_new();
			cur_str->srefs = vec_new();
			cur_str->arefs = vec_new();

			break;
		case GDS_ENDSTR:

			vec_add((*gds)->cells, cur_str);
			cur_str = NULL;

			break;
		case GDS_UNITS:
			{
				(*gds)->uu_per_dbunit = buf_read_float(buf);
				(*gds)->meter_per_dbunit = buf_read_float(buf + 8);

				memcpy((*gds)->units, buf, 16); // also store the buffer raw data

				break;
			}
		case GDS_STRNAME:
			{
				memcpy_s(cur_str->strname, MAX_STR_NAME, buf, buf_size);
				cur_str->strname[buf_size] = 0;

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

			if (cur_boundary) {
				vec_add(cur_str->boundaries, cur_boundary);
				cur_boundary = NULL;
			} else if (cur_path) {
				vec_add(cur_str->paths, cur_path);
				cur_path = NULL;
			} else if (cur_sref) {
				vec_add(cur_str->srefs, cur_sref);
				cur_sref = NULL;
			} else if (cur_aref) {
				vec_add(cur_str->arefs, cur_aref);
				cur_aref = NULL;
			}

			break;
		case GDS_SNAME: // SREF, AREF

			if (cur_sref) {
				memcpy_s(cur_sref->sname, MAX_STR_NAME, buf, buf_size);
				cur_sref->sname[buf_size] = 0;
			} else if (cur_aref) {
				memcpy_s(cur_aref->sname, MAX_STR_NAME, buf, buf_size);
				cur_aref->sname[buf_size] = 0;
			}

			break;
		case GDS_COLROW: // AREF

			if (cur_aref) {
				cur_aref->col = buf[0] << 8 | buf[1];
				cur_aref->row = buf[2] << 8 | buf[3];
			}

			break;
		case GDS_PATHTYPE:

			if (cur_path) {
				cur_path->pathtype = buf[0] << 8 | buf[1];
			}

			break;
		case GDS_STRANS: // SREF, AREF, TEXT

			if (cur_sref) {
				cur_sref->strans = buf[0] << 8 | buf[1];
			} else if (cur_aref) {
				cur_aref->strans = buf[0] << 8 | buf[1];
			}

			break;
		case GDS_ANGLE: // SREF, AREF, TEXT

			if (cur_sref) {
				cur_sref->angle = (float) (M_PI * buf_read_float(buf) / 180.0);
			} else if (cur_aref) {
				cur_aref->angle = (float) (M_PI * buf_read_float(buf) / 180.0);
			}

			break;
		case GDS_MAG: // SREF, AREF, TEXT
			//printf("--> MAG read\n");

			if (cur_sref) {
				cur_sref->mag = (float) buf_read_float(buf);
			} else if (cur_aref) {
				cur_aref->mag = (float) buf_read_float(buf);
			}

			break;
		case GDS_XY:
			{

				if (cur_boundary) {
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

			if (cur_boundary) {
				cur_boundary->layer = buf[0] << 8 | buf[1];
			} else if (cur_path) {
				cur_path->layer = buf[0] << 8 | buf[1];
			}

			break;
		case GDS_WIDTH: // PATH, TEXT

			if (cur_path) {
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

	// Expand all path elements into polygons
	uint64_t cells_size = (*gds)->cells->size;
	for (int i = 0; i < cells_size; i++) {

		struct gds_cell* cell = vec_get((*gds)->cells, i);

		for (int j = 0; j < cell->paths->size; j++)
		{
			struct gds_path* p = vec_get(cell->paths, j);

			p->esize = 2 * p->size + 1;
			p->epairs = malloc(p->esize * sizeof(struct gds_ipair));

			expand_path(p->epairs, p->pairs, p->size, p->width, p->pathtype);
		}
	}

	return SUCCESS;
}

static void gds_cell_delete(struct gds_cell* s)
{
	uint64_t i;

	for (i = 0; i < s->srefs->size; i++) {
		free(((struct gds_sref*)vec_get(s->srefs, i)));
	}
	vec_free(s->srefs);
	free(s->srefs);

	for (i = 0; i < s->arefs->size; i++) {
		free(((struct gds_aref*)vec_get(s->arefs, i)));
	}
	vec_free(s->arefs);
	free(s->arefs);

	for (i = 0; i < s->boundaries->size; i++) {
		free(((struct gds_bndry*)vec_get(s->boundaries, i))->pairs);
		free(((struct gds_bndry*)vec_get(s->boundaries, i)));
	}
	vec_free(s->boundaries);
	free(s->boundaries);

	for (i = 0; i < s->paths->size; i++) {
		free(((struct gds_path*)vec_get(s->paths, i))->pairs);
		free(((struct gds_path*)vec_get(s->paths, i))->epairs);
		free(((struct gds_path*)vec_get(s->paths, i)));
	}
	vec_free(s->paths);
	free(s->paths);

	free(s);
}

void gds_db_delete(struct gds_db* gds)
{
	int i;

	if (gds == NULL) return;

	for (i = 0; i < gds->cells->size; i++) {
		gds_cell_delete(((struct gds_cell*)vec_get(gds->cells, i)));
	}

	vec_free(gds->cells);
	free(gds->cells);
	free(gds);
}

GDS_ERROR gds_collapse(struct gds_db* gds, const char* cell, const double* bounds,
	uint64_t max_polys, const char* dest, vec* pvec)
{
	if (cell == NULL) {
		return INVALID_ARGUMENT;
	}

	if (gds == NULL) {
		return INVALID_ARGUMENT;
	}

	// Write header records to file

	FILE* pdest = NULL;
	if (dest != NULL) {
		fopen_s(&pdest, dest, "wb");
		if (pdest == NULL)
			return FILE_ERROR;
	}

	if (dest != NULL) {
		char access[24] = { 0 }; // Just write zero to BGNLIB and BGNSTR
		file_append_short(pdest, GDS_HEADER, 600);
		file_append_bytes(pdest, GDS_BGNLIB, access, 24);
		file_append_string(pdest, GDS_LIBNAME, "");
		file_append_bytes(pdest, GDS_UNITS, (char*)gds->units, 16);
		file_append_bytes(pdest, GDS_BGNSTR, access, 24);
		file_append_string(pdest, GDS_STRNAME, "TOP");
	}

	// Create the bounding box
	struct gds_ipair* bbox = NULL;
	if (bounds != NULL) {

		if (bounds[2] <= bounds[0] || bounds[3] <= bounds[1])
			return INVALID_ARGUMENT;

		bbox = malloc(5 * sizeof(struct gds_ipair));

		bbox[0] = (struct gds_ipair){ (int)(bounds[0] / gds->uu_per_dbunit), (int)(bounds[1] / gds->uu_per_dbunit) };
		bbox[1] = (struct gds_ipair){ (int)(bounds[0] / gds->uu_per_dbunit), (int)(bounds[3] / gds->uu_per_dbunit) };
		bbox[2] = (struct gds_ipair){ (int)(bounds[2] / gds->uu_per_dbunit), (int)(bounds[3] / gds->uu_per_dbunit) };
		bbox[3] = (struct gds_ipair){ (int)(bounds[2] / gds->uu_per_dbunit), (int)(bounds[1] / gds->uu_per_dbunit) };
		bbox[4] = bbox[0];
	}

	// Find the gds structure to expand
	struct gds_cell* top = find_struct(gds, cell);
	if (top == NULL)
		return CELL_NOT_FOUND;

	// Initial transformation of top level cell
	struct gds_trans trans = (struct gds_trans){ .x = 0, .y = 0, .mag = 1.0, .angle = 0.0, .mirror = 0 };

	// The rinfo structure is used for common data during recursion
	
	g_info = (struct rinfo){ .gds = gds, .bb = bbox, .max_polys = max_polys, .out_file = pdest, .out_pset = pvec,
						   .scount = 0, .pcount = 0 };

	// start the recursion
	collapse_cell(top, trans);

	// write the tail headers to the outfile
	if (dest != NULL) {
		file_append_record(pdest, GDS_ENDSTR);
		file_append_record(pdest, GDS_ENDLIB);
	}

	if (pdest != NULL) {
		fclose(pdest);
	}

	free(bbox);

	return 0;
}

bool gds_poly_contains_point(struct gds_ipair* poly, int n, struct gds_ipair p)
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

void gds_top_cells(struct gds_db* gds)
{
	uint64_t i;
	uint64_t count = gds->cells->size;

	if (gds == NULL)
		return;

	printf("Top structures:\n");

	for (i = 0; i < count; i++) {
		struct gds_cell* struc = vec_get(gds->cells, i);

		int is_referenced = 0;

		uint64_t j;
		for (j = 0; j < count; j++) {
			uint64_t ref_count;

			if (i == j) continue; // skip self

			struct gds_cell* test_struct = vec_get(gds->cells, j);

			ref_count = test_struct->srefs->size;
			for (int k = 0; k < ref_count; k++) {
				if (strcmp(((struct gds_sref*)vec_get(test_struct->srefs, k))->sname, struc->strname) == 0) {
					is_referenced = 1;
					break;
				}
			}
			if (is_referenced) break;

			ref_count = test_struct->arefs->size;
			for (int k = 0; k < ref_count; k++) {
				if (strcmp(((struct gds_aref*)vec_get(test_struct->arefs, k))->sname, struc->strname) == 0) {
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
}

/*
vector* gds_polyset_new()
{
	vector* pset;

	pset = malloc(sizeof(vector));
	vec_init(pset);

	return pset;
}

void gds_polyset_delete(vector* pset)
{
	if (pset == NULL) return;

	for (int i = 0; i < pset->size; i++) {
		struct gds_poly* p = vec_get(pset, i);
		free(p->pairs);
		free(p);
	}
	vec_free(pset);
	free(pset);
}
*/
