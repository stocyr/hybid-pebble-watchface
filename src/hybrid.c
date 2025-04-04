#include <pebble.h>
#include "drawing_routines.h"
#include "paths.h"
#include "xprintf.h"

#define FONT_SIZE 58

#define DEBUG_TIME

#ifdef DEBUG_TIME
	#define SECONDS s_debug_sec
	#define MINUTES s_debug_min
	#define HOURS s_debug_hour
#else
	#define SECONDS t->tm_sec
	#define MINUTES t->tm_min
	#define HOURS t->tm_hour
#endif

static GPath *s_minute_hand_path, *s_hour_hand_path;

static Window *s_main_window;
static TextLayer *s_time_h_layer, *s_date_layer;
static Layer *s_analog_layer, *s_cosine_top_layer, *s_cosine_bottom_layer;

static BitmapLayer *s_ticks_layer;
static GBitmap *s_ticks_bitmap, *s_cosine_top_white_bitmap, *s_cosine_top_black_bitmap,
	*s_cosine_bottom_white_bitmap, *s_cosine_bottom_black_bitmap;

static GFont s_time_font;

static const GPoint s_analog_center = { 72, 84 };

static _Bool s_top;

static int8_t s_date_height, s_time_height;

static uint8_t s_debug_sec, s_debug_min, s_debug_hour;
static AppTimer *s_timer;
static uint16_t millisecs = 98;

static void update_face_orientation(_Bool top) {
	Layer *window_layer = window_get_root_layer(s_main_window);
	GRect bounds = layer_get_bounds(window_layer);
	if(top) {
		s_date_height = 88;
		s_time_height = 103;	// bounded: 110
	}
	else {
		s_date_height = 50;
		s_time_height = -10;	// bounded: -17
	}
	layer_set_hidden(s_cosine_top_layer, top);
	layer_set_hidden(s_cosine_bottom_layer, !top);
	layer_set_frame(text_layer_get_layer(s_date_layer), GRect(1, s_date_height, bounds.size.w, 30));
	layer_set_frame(text_layer_get_layer(s_time_h_layer), GRect(-13, s_time_height, bounds.size.w+26, FONT_SIZE));
}

static void update_time() {
	// Get a tm structure
	time_t temp = time(NULL);
	struct tm *t = localtime(&temp);

	// update TOP / BOTTOM analog watchface if neccessary
	if (MINUTES == 15) {
		s_top = false;
		update_face_orientation(s_top);
	}
	else if (MINUTES == 45) {
		s_top = true;
		update_face_orientation(s_top);
	}

	// Write the current hours and minutes into a buffer
	static char s_time_h_buffer[6], s_date_buffer[6];
	strftime(s_date_buffer, sizeof(s_date_buffer), "%d.%m", t);
	strftime(s_time_h_buffer, sizeof(s_time_h_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
#ifdef DEBUG_TIME
	xsprintf(s_time_h_buffer, "%02d:%02d", s_debug_hour, s_debug_min);
#endif

	// Display time on the TextLayer
	text_layer_set_text(s_time_h_layer, s_time_h_buffer);
	// Display date on the TextLayer
	text_layer_set_text(s_date_layer, s_date_buffer);

	// Update hand layer
	layer_mark_dirty(s_analog_layer);
}

static void analog_layer_update_callback(Layer *layer, GContext *ctx) {
	time_t temp = time(NULL);
	struct tm *t = localtime(&temp);

	//###  MINUTE HAND  ###
	uint16_t angle_m = TRIG_MAX_ANGLE * MINUTES / 60;
	gpath_move_to(s_minute_hand_path, s_analog_center);
	if(MINUTES != 0)
		gpath_rotate_to(s_minute_hand_path, angle_m);
	else
		gpath_rotate_to(s_minute_hand_path, 0);

	graphics_context_set_stroke_color(ctx, GColorBlack);
	gpath_draw_outline(ctx, s_minute_hand_path);
	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_draw_filled(ctx, s_minute_hand_path);
	//graphics_context_set_stroke_color(ctx, GColorWhite);
	//graphics_draw_line_polar(ctx, s_analog_center, angle_m, 0, 60, 3);

	//###  HOUR HAND  ###
	uint16_t angle_h = (TRIG_MAX_ANGLE * (((HOURS % 12) * 6) + (MINUTES / 10))) / (12 * 6);
	gpath_move_to(s_hour_hand_path, s_analog_center);
	if((HOURS == 0 || HOURS == 12) && MINUTES == 0)
		gpath_rotate_to(s_hour_hand_path, 0);
	else
		gpath_rotate_to(s_hour_hand_path, angle_h);


	graphics_context_set_stroke_color(ctx, GColorBlack);
	gpath_draw_outline(ctx, s_hour_hand_path);
	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_draw_filled(ctx, s_hour_hand_path);
	//graphics_context_set_stroke_color(ctx, GColorWhite);
	//graphics_draw_line_polar(ctx, s_analog_center, angle_h, 0, 45, 6);
}

static void cosine_top_layer_update_callback(Layer *layer, GContext *ctx) {
	GRect cosine_rect = gbitmap_get_bounds(s_cosine_top_white_bitmap);
	cosine_rect.origin = GPoint(0, 0);

	// Draw white
	graphics_context_set_compositing_mode(ctx, GCompOpOr);
	graphics_draw_bitmap_in_rect(ctx, s_cosine_top_white_bitmap, cosine_rect);

	cosine_rect = gbitmap_get_bounds(s_cosine_top_black_bitmap);
	cosine_rect.origin = GPoint(0, 0);

	// Draw black
	graphics_context_set_compositing_mode(ctx, GCompOpClear);
	graphics_draw_bitmap_in_rect(ctx, s_cosine_top_black_bitmap, cosine_rect);
}

static void cosine_bottom_layer_update_callback(Layer *layer, GContext *ctx) {
	GRect cosine_rect = gbitmap_get_bounds(s_cosine_bottom_white_bitmap);
	cosine_rect.origin = GPoint(0, 79);

	// Draw white
	graphics_context_set_compositing_mode(ctx, GCompOpOr);
	graphics_draw_bitmap_in_rect(ctx, s_cosine_bottom_white_bitmap, cosine_rect);

	cosine_rect = gbitmap_get_bounds(s_cosine_bottom_black_bitmap);
	cosine_rect.origin = GPoint(0, 79);

	// Draw black
	graphics_context_set_compositing_mode(ctx, GCompOpClear);
	graphics_draw_bitmap_in_rect(ctx, s_cosine_bottom_black_bitmap, cosine_rect);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	update_time();
}

static void timer_callback(void *data) {
	//if(++s_debug_sec == 60) {
		if(++s_debug_min == 60) {
			if(++s_debug_hour == 24)
				s_debug_hour = 0;
			s_debug_min = 0;
		}
		s_debug_sec = 0;
	//}
	update_time();
	s_timer = app_timer_register(millisecs, timer_callback, NULL);
}

static void main_window_load(Window *window) {
	// Get information about the Window
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	// Create GBitmap
	s_ticks_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TICKS);
	s_cosine_top_white_bitmap = gbitmap_create_with_resource(
			RESOURCE_ID_IMAGE_COSINE_TOP_WHITE);
	s_cosine_top_black_bitmap = gbitmap_create_with_resource(
			RESOURCE_ID_IMAGE_COSINE_TOP_BLACK);
	s_ticks_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TICKS);
		s_cosine_bottom_white_bitmap = gbitmap_create_with_resource(
				RESOURCE_ID_IMAGE_COSINE_BOTTOM_WHITE);
		s_cosine_bottom_black_bitmap = gbitmap_create_with_resource(
				RESOURCE_ID_IMAGE_COSINE_BOTTOM_BLACK);

	// Create BitmapLayer to display the GBitmap
	s_ticks_layer = bitmap_layer_create(bounds);
	s_cosine_top_layer = layer_create(layer_get_bounds(window_layer));
	s_cosine_bottom_layer = layer_create(layer_get_bounds(window_layer));

	// Set the bitmap onto the layer and add to the window
	bitmap_layer_set_bitmap(s_ticks_layer, s_ticks_bitmap);
	layer_add_child(window_layer, bitmap_layer_get_layer(s_ticks_layer));

	// Create the TextLayer with specific bounds
	s_time_h_layer = text_layer_create(GRect(-13, s_time_height, bounds.size.w+26, FONT_SIZE));
	s_date_layer = text_layer_create(GRect(1, s_date_height, bounds.size.w, 30));

	// Create the analog clock layer
	s_analog_layer = layer_create(bounds);
	layer_set_update_proc(s_analog_layer, analog_layer_update_callback);
	layer_set_update_proc(s_cosine_top_layer, cosine_top_layer_update_callback);
	layer_set_update_proc(s_cosine_bottom_layer, cosine_bottom_layer_update_callback);
	layer_add_child(window_layer, s_analog_layer);
	layer_add_child(window_layer, s_cosine_bottom_layer);
	layer_add_child(window_layer, s_cosine_top_layer);

	// Improve the layout to be more like a watchface
	text_layer_set_background_color(s_time_h_layer, GColorClear);
	text_layer_set_text_color(s_time_h_layer, GColorWhite);
	text_layer_set_text(s_time_h_layer, "00:00");
	text_layer_set_text_alignment(s_time_h_layer, GTextAlignmentCenter);

	// Set font
	text_layer_set_font(s_time_h_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_square_58)));

	// Improve the layout to be more like a watchface
	text_layer_set_background_color(s_date_layer, GColorClear);
	text_layer_set_text_color(s_date_layer, GColorWhite);
	text_layer_set_text(s_date_layer, "00.00");
	text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
	text_layer_set_font(s_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_bitdust_22)));

	// Add it as a child layer to the Window's root layer
	layer_add_child(window_layer, text_layer_get_layer(s_time_h_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

	layer_mark_dirty(s_cosine_top_layer);
	layer_mark_dirty(s_cosine_bottom_layer);
}

static void main_window_unload(Window *window) {
	// Destroy TextLayer
	text_layer_destroy(s_time_h_layer);

	// Destroy analogLayer
	layer_destroy(s_analog_layer);
	layer_destroy(s_cosine_top_layer);
	layer_destroy(s_cosine_bottom_layer);

	// Unload GFont
	fonts_unload_custom_font(s_time_font);

	// Destroy GBitmap
	gbitmap_destroy(s_ticks_bitmap);
	gbitmap_destroy(s_cosine_top_white_bitmap);
	gbitmap_destroy(s_cosine_top_black_bitmap);
	gbitmap_destroy(s_cosine_bottom_white_bitmap);
	gbitmap_destroy(s_cosine_bottom_black_bitmap);

	// Destroy BitmapLayer
	bitmap_layer_destroy(s_ticks_layer);
}

static void init() {
	time_t temp = time(NULL);
	struct tm *t = localtime(&temp);
	s_top = t->tm_min < 15 || t->tm_min >= 45;
	s_debug_sec = t->tm_sec;
	s_debug_min = t->tm_min;
	s_debug_hour = t->tm_hour + 2;

	// Create main Window element and assign to pointer
	s_main_window = window_create();

	// Set the background color
	window_set_background_color(s_main_window, GColorBlack);

	// Set handlers to manage the elements inside the Window
	window_set_window_handlers(s_main_window, (WindowHandlers ) { .load =
					main_window_load, .unload = main_window_unload });

	// Show the Window on the watch, with animated=false
	window_stack_push(s_main_window, false);

	// Make sure the time is displayed from the start
	// update TOP / BOTTOM analog watchface
	update_face_orientation(s_top);
	update_time();

	// Register with TickTimerService
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

	// Initialize GPaths
	s_minute_hand_path = gpath_create(&MINUTE_HAND_POINTS);
	s_hour_hand_path = gpath_create(&HOUR_HAND_POINTS);

#ifdef DEBUG_TIME
	s_timer = app_timer_register(millisecs, timer_callback, NULL);
#endif
}

static void deinit() {
#ifdef DEBUG_TIME
	app_timer_cancel(s_timer);
#endif

	// Destroy Window
	window_destroy(s_main_window);

	// Destroy GPaths
	gpath_destroy(s_minute_hand_path);
	gpath_destroy(s_hour_hand_path);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
