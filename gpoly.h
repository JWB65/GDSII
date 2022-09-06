#pragma once

#include <inttypes.h>

typedef struct {
	struct gds_ipair* pairs;
	uint16_t size;
	uint16_t layer;
} gds_poly;
