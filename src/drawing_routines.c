#include <pebble.h>
// Draw line with width
// Based on http://rosettacode.org/wiki/Bitmap/Bresenham's_line_algorithm#C)

void graphics_draw_line2(GContext *ctx, GPoint p0, GPoint p1, int8_t width) {
	// Order points so that lower x is first
	int16_t x0, x1, y0, y1;
	if (p0.x <= p1.x) {
		x0 = p0.x;
		x1 = p1.x;
		y0 = p0.y;
		y1 = p1.y;
	} else {
		x0 = p1.x;
		x1 = p0.x;
		y0 = p1.y;
		y1 = p0.y;
	}

	// Init loop variables
	int16_t dx = x1 - x0;
	int16_t dy = abs(y1 - y0);
	int16_t sy = y0 < y1 ? 1 : -1;
	int16_t err = (dx > dy ? dx : -dy) / 2;
	int16_t e2;

	// Calculate whether line thickness will be added vertically or horizontally based on line angle
	int8_t xdiff, ydiff;

	if (dx > dy) {
		xdiff = 0;
		ydiff = width / 2;
	} else {
		xdiff = width / 2;
		ydiff = 0;
	}

	// Use Bresenham's integer algorithm, with slight modification for line width, to draw line at any angle
	while (true) {
		// Draw line thickness at each point by drawing another line
		// (horizontally when > +/-45 degrees, vertically when <= +/-45 degrees)
		graphics_draw_line(ctx, GPoint(x0 - xdiff, y0 - ydiff),
				GPoint(x0 + xdiff, y0 + ydiff));

		if (x0 == x1 && y0 == y1)
			break;
		e2 = err;
		if (e2 > -dx) {
			err -= dy;
			x0++;
		}
		if (e2 < dy) {
			err += dx;
			y0 += sy;
		}
	}
}

// Draws one of 12 hour lines from selectable start and stop distance to center
void graphics_draw_line_polar(GContext *ctx, GPoint center, uint16_t angle,
		uint8_t r_start, uint8_t r_stop, int8_t width) {

	GPoint A = { .x = (int16_t) (sin_lookup(angle) * (int32_t) r_start
			/ TRIG_MAX_RATIO) + center.x, .y = (int16_t) (-cos_lookup(angle)
			* (int32_t) r_start / TRIG_MAX_RATIO) + center.y, };

	GPoint B = { .x = (int16_t) (sin_lookup(angle) * (int32_t) r_stop
			/ TRIG_MAX_RATIO) + center.x, .y = (int16_t) (-cos_lookup(angle)
			* (int32_t) r_stop / TRIG_MAX_RATIO) + center.y, };

	graphics_draw_line2(ctx, A, B, width);
}
