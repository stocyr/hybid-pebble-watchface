#pragma once

#include <pebble.h>

void graphics_draw_line2(GContext *ctx, GPoint p0, GPoint p1, int8_t width);
void graphics_draw_line_polar(GContext *ctx, GPoint center, uint16_t angle, uint8_t r_start, uint8_t r_stop, int8_t width);