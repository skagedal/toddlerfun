/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) 2013 Simon Kågedal Reimer <simon@helgo.net>
 * 
 */

#include <config.h>
#include <math.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <librsvg/rsvg.h>
#include <pango/pangocairo.h>
#include <gst/gst.h>
#include "theme.h"

static const gint toddlerfun_effect_min = 0;
static const gint toddlerfun_effect_max = 8;
static const gdouble toddlerfun_color_cycle_distance = 2000;
static const gdouble toddlerfun_svg_size = 100;
static const gdouble toddlerfun_min_rotation = G_PI * -0.2;
static const gdouble toddlerfun_max_rotation = G_PI * 0.2;

typedef struct { 
	GtkWidget *window;
	GtkWidget *darea;
	cairo_surface_t *surface;
	gboolean has_previous;
	gint previous_x;
	gint previous_y;
	gint brighten_count;
	gint effect_num;
	gdouble traveled_distance;

	gboolean play_sound_fx;
	ToddlerFunTheme *theme;

	// Messages
	gint message_num;
	gboolean has_message;
	cairo_surface_t *message_surface;
	gdouble message_alpha;
	GTimer *message_timer;
	
	// Letters
	guint last_keyval;
	gint letter_x;
	gint letter_y;
	gdouble letter_hue;

	// These are active during a draw
	cairo_region_t *region;
	gint x;
	gint y;
	gint object_num;
	gdouble image_rotation;
	PangoLayout *layout;
} ToddlerFun;

typedef void (*ToddlerFunDrawFunc) (ToddlerFun *toddlerfun, cairo_t *cr);

typedef struct {
    GstElement *elt;
    gboolean repeat;
} ToddlerFunSound;

gchar *toddlerfun_messages [] = {
	N_("Welcome to Toddler Fun! Move the mouse around to draw. Press Escape to exit."),
	N_("Press mouse buttons to add funny images. Press keys on the keyboard to add letters."),
	N_("Press Return to save the current image to a file."),
	N_("Use the mouse scroll wheel or the Up/Down keys to change mirror effect."),
	N_("Press Space to clear drawing.")
};

#define NUM_MESSAGES (sizeof (toddlerfun_messages) / sizeof (toddlerfun_messages [0]))

//
// Sound
//

static void
eos_message_received (GstBus *bus, GstMessage *message, ToddlerFunSound *sound)
{
    if (sound->repeat == TRUE) {
        gst_element_set_state (GST_ELEMENT(sound->elt), GST_STATE_NULL);
        gst_element_set_state (GST_ELEMENT(sound->elt), GST_STATE_PLAYING);
    } else {
        gst_element_set_state (GST_ELEMENT(sound->elt), GST_STATE_NULL);
        gst_object_unref (GST_OBJECT(sound->elt));
        sound->elt = NULL;
    }
}

static void
play_sound (gchar *filesnd, gboolean repeat)
{
    gchar *filename;
    GstElement *pipeline;
	GstBus *bus;

    pipeline = gst_element_factory_make("playbin", "playbin");
    if (pipeline != NULL) {
        ToddlerFunSound *closure = g_new (ToddlerFunSound, 1);
        closure->elt = pipeline;
        closure->repeat = repeat;
        bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
        gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);
        g_signal_connect (bus, "message::eos", 
            (GCallback) eos_message_received, closure);
        gst_object_unref (bus);

        if (!g_file_test (filesnd, G_FILE_TEST_EXISTS))
            g_printerr(_("Sound file '%s' does not exist\n"), filesnd);
        else {
            filename = g_strdup_printf("file://%s", filesnd);
            g_object_set (G_OBJECT(pipeline), "uri", filename, NULL);
            gst_element_set_state (GST_ELEMENT(pipeline), GST_STATE_PLAYING);
			g_free (filename);
        }
    }
}

//
// Region handling
//

static double
min_doubles (double *arr, int n)
{
	double found_min;
	int i;
	g_assert (n > 0);
	found_min = arr[0];
	for (i = 1; i < n; i++) {
		if (arr[i] < found_min)
			found_min = arr[i];
	}
	return found_min;
}

static double 
max_doubles (double *arr, int n)
{
	double found_max;
	int i;
	g_assert (n > 0);
	found_max = arr[0];
	for (i = 1; i < n; i++) {
		if (arr[i] > found_max)
			found_max = arr[i];
	}
	return found_max;
}

static void
add_user_rectangle_to_region (ToddlerFun *toddlerfun, cairo_t *cr,
							  double x1, double y1, double x2, double y2)
{
	const int TOP_LEFT = 0;
	const int BOTTOM_RIGHT = 1;
	const int TOP_RIGHT = 2;
	const int BOTTOM_LEFT = 3;
	double x[4], y[4];
	cairo_rectangle_int_t rectangle;
	cairo_status_t status;
	int i;

	x[TOP_LEFT] = x[BOTTOM_LEFT] = x1;
	y[TOP_LEFT] = y[TOP_RIGHT] = y1;
	x[BOTTOM_RIGHT] = x[TOP_RIGHT] = x2;
	y[BOTTOM_RIGHT] = y[BOTTOM_LEFT] = y2;

	for (i = 0; i < 4; i++)
		cairo_user_to_device (cr, &x[i], &y[i]);

	rectangle.x = min_doubles(x, 4);
	rectangle.y = min_doubles(y, 4);
	rectangle.width = max_doubles(x, 4) - rectangle.x;
	rectangle.height = max_doubles(y, 4) - rectangle.y;

	status = cairo_region_union_rectangle(toddlerfun->region, &rectangle);
	g_assert(status == CAIRO_STATUS_SUCCESS);
}

static void 
add_stroke_to_region(ToddlerFun *toddlerfun, cairo_t *cr)
{
	double x1, y1, x2, y2;
	cairo_stroke_extents (cr, &x1, &y1, &x2, &y2);
	add_user_rectangle_to_region (toddlerfun, cr, x1, y1, x2, y2);
}

static void 
draw_line(ToddlerFun *toddlerfun, cairo_t *cr) 
{
	if (toddlerfun->has_previous)
		cairo_move_to (cr, toddlerfun->previous_x, toddlerfun->previous_y);
	else
		cairo_move_to (cr, toddlerfun->x, toddlerfun->y);
	cairo_line_to (cr, toddlerfun->x, toddlerfun->y);
	add_stroke_to_region (toddlerfun, cr);
	cairo_stroke (cr);
}

static void
draw_image(ToddlerFun *toddlerfun, cairo_t *cr)
{
	ToddlerFunThemeObject *obj;
	RsvgHandle *handle;
	RsvgDimensionData dimension;
	gdouble hypothenuse, scale;

	obj = theme_get_object(toddlerfun->theme, toddlerfun->object_num);
	if (obj == NULL)
		return;

	handle = obj->image_handle;
	if (obj == NULL)
		return;

	cairo_save (cr);
	
	rsvg_handle_get_dimensions (handle, &dimension);
	hypothenuse = sqrt(dimension.width * dimension.width + 
					   dimension.height * dimension.height);
	scale = toddlerfun_svg_size / hypothenuse;

	cairo_translate (cr, toddlerfun->x, toddlerfun->y);
	cairo_scale (cr, scale, scale);
	cairo_translate (cr, -dimension.width / 2, -dimension.height / 2);
	cairo_rotate (cr, toddlerfun->image_rotation);
	rsvg_handle_render_cairo (handle, cr);

	add_user_rectangle_to_region (toddlerfun, cr, 0, 0, 
								  dimension.width, dimension.height);

	cairo_restore (cr);
}

static void
draw_string (ToddlerFun *toddlerfun, cairo_t *cr)
{
	gint width, height;

	cairo_save (cr);
	
	pango_layout_get_pixel_size (toddlerfun->layout, &width, &height);
	cairo_translate (cr, toddlerfun->letter_x - width / 2, 
					 toddlerfun->letter_y - height / 2);
	cairo_move_to (cr, 0, 0);
	pango_cairo_update_layout (cr, toddlerfun->layout);
	pango_cairo_show_layout (cr, toddlerfun->layout);

	add_user_rectangle_to_region (toddlerfun, cr, 0, 0, width, height);
	
	cairo_restore (cr);
}

static void 
draw_effect (ToddlerFun *toddlerfun,
			 cairo_t *cr,
			 ToddlerFunDrawFunc draw)
{
    cairo_surface_t *surface = toddlerfun->surface;
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);
    int mirror = toddlerfun->effect_num > 0;
    int rotations = toddlerfun->effect_num > 0 ? toddlerfun->effect_num : 1;
    double angle_step = G_PI * 2 / rotations;
    int center_x = width / 2;
    int center_y = height / 2;
    int rot;

    cairo_save(cr);
    for (rot = 0; rot < rotations; rot++) {
        cairo_translate (cr, center_x, center_y);
        cairo_rotate (cr, angle_step);
        cairo_translate (cr, -center_x, -center_y);
        (*draw) (toddlerfun, cr);
        if (mirror) {
            cairo_save (cr);
            cairo_translate (cr, center_x, center_y);
            cairo_scale (cr, -1, 1);
            cairo_translate (cr, -center_x, -center_y);
            (*draw) (toddlerfun, cr);
            cairo_restore (cr);
        }
    }
    cairo_restore(cr);
	
}

static void
surface_clear (ToddlerFun *toddlerfun) 
{
	cairo_t *cr;

	cr = cairo_create (toddlerfun->surface);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_paint (cr);
	
	cairo_destroy(cr);
}

static void
surface_brighten (ToddlerFun *toddlerfun)
{
	cairo_t *cr;

	cr = cairo_create (toddlerfun->surface);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_paint_with_alpha (cr, 0.1);
	
	cairo_destroy(cr);
}

static void
render_message (ToddlerFun *toddlerfun) 
{
	cairo_t *cr;
	PangoLayout *layout;
	PangoFontDescription *desc;
	gint width, height;

	// Create surface if we haven't already
	if (toddlerfun->message_surface == NULL) {
		toddlerfun->message_surface = 
			cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
										2000, 40);
	}

	cr = cairo_create (toddlerfun->message_surface);

	// Clear surface
	cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	// Layout text
	layout = pango_cairo_create_layout (cr);
	pango_layout_set_text (layout, toddlerfun_messages[toddlerfun->message_num], -1);
	desc = pango_font_description_from_string ("Sans 20px");
	pango_layout_set_font_description (layout, desc);
	pango_font_description_free (desc);
	cairo_translate (cr, 10, 10);
	pango_cairo_update_layout (cr, layout);
	pango_layout_get_pixel_size (layout, &width, &height);

	// Draw rectangle
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_rectangle (cr, 0, 0, width + 10, height + 10);
	cairo_fill (cr);

	// Draw text
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_move_to (cr, 5, 5);
	pango_cairo_show_layout (cr, layout);
	
	cairo_destroy (cr);
	g_object_unref (layout);
}

static void
update_message (ToddlerFun *toddlerfun)
{
	if (toddlerfun->message_timer == NULL)
		toddlerfun->message_timer = g_timer_new ();
	g_timer_start (toddlerfun->message_timer);

	toddlerfun->message_num = (toddlerfun->message_num + 1) % NUM_MESSAGES;

	render_message (toddlerfun);
}

static void
save_picture (ToddlerFun *toddlerfun)
{
	GDateTime *datetime;
	gchar *dirname;
    gchar *filename;
    gchar *pathname;
 
    dirname = g_build_filename(g_get_home_dir(), "toddlerfun", NULL);

    if(!g_file_test (dirname, G_FILE_TEST_EXISTS)) {
        if (g_mkdir(dirname, 0750) < 0)
            g_printerr(_("Failed to create directory '%s'\n"),
					   dirname);
	}

	datetime = g_date_time_new_now_local ();
	filename = g_date_time_format (datetime, "%F_%H.%M.%S.png");
    pathname = g_build_filename(dirname, filename, NULL);

    if (cairo_surface_write_to_png(toddlerfun->surface, pathname) < 0)
        g_printerr(_("Failed to create file '%s'\n"), pathname);

    g_free (filename);
    g_free (dirname);
    g_free (pathname);
	g_date_time_unref (datetime);
}

static gboolean 
on_configure(GtkWidget *widget,
             GdkEventConfigure *event, 
		     gpointer user_data)
{
	ToddlerFun *toddlerfun;
	gint width, height, old_width, old_height;
	cairo_surface_t *old_surface = NULL;

	toddlerfun = (ToddlerFun *) user_data;
	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);

	old_surface = toddlerfun->surface;

	if (old_surface != NULL) {
		old_width = cairo_image_surface_get_width (old_surface);
		old_height = cairo_image_surface_get_height (old_surface);

		if (old_width == width && old_height == height) 
			return TRUE;
	}
	
	toddlerfun->surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
												  width, height);

	if (old_surface != NULL) {
		cairo_t *cr = cairo_create (toddlerfun->surface);
		cairo_scale (cr, 
					 (gdouble) width / (gdouble) old_width,
					 (gdouble) height / (gdouble) old_height);
		cairo_set_source_surface (cr, old_surface, 0, 0);
		cairo_paint (cr);
		cairo_destroy (cr);
		cairo_surface_destroy (old_surface);
	} else {
		surface_clear(toddlerfun);
	}

	toddlerfun->has_previous = FALSE;
	
	return TRUE;
}

static gboolean 
on_draw(GtkWidget *window, 
		cairo_t *cr,
        gpointer user_data)
{
	ToddlerFun *toddlerfun;
	gint height;

	toddlerfun = (ToddlerFun *) user_data;

	if (toddlerfun == NULL || toddlerfun->surface == NULL)
		return TRUE;
	
	cairo_set_source_surface (cr, toddlerfun->surface, 0, 0);
	cairo_paint (cr);

	if (toddlerfun->has_message) {
		height = cairo_image_surface_get_height (toddlerfun->surface);
		cairo_set_source_surface (cr, toddlerfun->message_surface,
								  10, height - 50);
		cairo_paint_with_alpha (cr, toddlerfun->message_alpha);
	}

	return FALSE;
}

static void
update_color (ToddlerFun *toddlerfun,
			  cairo_t *cr)
{
	gdouble hue, r, g, b;
	if (toddlerfun->has_previous) {
		gdouble new_distance;
		gint xdiff, ydiff;

		xdiff = toddlerfun->x - toddlerfun->previous_x;
		ydiff = toddlerfun->y - toddlerfun->previous_y;
		new_distance = sqrt(xdiff * xdiff + ydiff * ydiff);

		toddlerfun->traveled_distance += new_distance;
		while (toddlerfun->traveled_distance > toddlerfun_color_cycle_distance)
			toddlerfun->traveled_distance -= toddlerfun_color_cycle_distance;
	}

	hue = toddlerfun->traveled_distance / toddlerfun_color_cycle_distance;
	gtk_hsv_to_rgb (hue, 1.0, 1.0, &r, &g, &b);

	cairo_set_source_rgba (cr, r, g, b, 0.7);
}

static gboolean
on_motion_notify (GtkWidget *widget,
				  GdkEventButton *event,
				  ToddlerFun *toddlerfun)
{
	cairo_t *cr = cairo_create (toddlerfun->surface);
	cairo_set_source_rgba(cr, 1, 0, 0, 0.3);
	cairo_set_line_width(cr, 5);

	toddlerfun->region = cairo_region_create ();
	toddlerfun->x = event->x;
	toddlerfun->y = event->y;

	update_color (toddlerfun, cr);

//	draw_line (toddlerfun, cr);
	draw_effect (toddlerfun, cr, &draw_line);

	cairo_destroy(cr);

	gtk_widget_queue_draw_region (widget, toddlerfun->region);

	cairo_region_destroy(toddlerfun->region);
	toddlerfun->region = NULL;

	toddlerfun->previous_x = event->x;
	toddlerfun->previous_y = event->y;
	toddlerfun->has_previous = TRUE;

	return TRUE;
}				 

static gboolean
on_button_press (GtkWidget *widget,
				 GdkEventButton *event,
				 ToddlerFun *toddlerfun)
{
	gint num_objects;
	cairo_t *cr = cairo_create (toddlerfun->surface);

	toddlerfun->region = cairo_region_create ();
	toddlerfun->x = event->x;
	toddlerfun->y = event->y;
	num_objects = theme_get_n_objects (toddlerfun->theme);
	if (num_objects < 1)
		return TRUE;

	toddlerfun->object_num = g_random_int_range (0, num_objects);
	toddlerfun->image_rotation = g_random_double_range (toddlerfun_min_rotation,
													toddlerfun_max_rotation);

	if (toddlerfun->play_sound_fx) {
		ToddlerFunThemeObject *obj;
		obj = theme_get_object (toddlerfun->theme, toddlerfun->object_num);
		play_sound (obj->sound_file, FALSE);
	}

	draw_effect (toddlerfun, cr, &draw_image);
	
	cairo_destroy(cr);

	gtk_widget_queue_draw_region (widget, toddlerfun->region);
	cairo_region_destroy(toddlerfun->region);
	toddlerfun->region = NULL;

	return TRUE;
}

static void
print_string (gchar *s, ToddlerFun *toddlerfun)
{
	gdouble r, g, b;
	PangoFontDescription *desc;
	cairo_t *cr = cairo_create (toddlerfun->surface);

	toddlerfun->region = cairo_region_create ();

	toddlerfun->layout = pango_cairo_create_layout (cr);
	pango_layout_set_text (toddlerfun->layout, s, -1);
	desc = pango_font_description_from_string ("Sans Bold 60px");
	pango_layout_set_font_description (toddlerfun->layout, desc);
	pango_font_description_free (desc);

	gtk_hsv_to_rgb (toddlerfun->letter_hue, 1.0, 0.8, &r, &g, &b);
	cairo_set_source_rgb (cr, r, g, b);

	draw_effect (toddlerfun, cr, &draw_string);
	
	cairo_destroy (cr);

	gtk_widget_queue_draw_region (toddlerfun->darea, toddlerfun->region);

	g_object_unref (toddlerfun->layout);
	cairo_region_destroy (toddlerfun->region);
	toddlerfun->region = NULL;
}

static gboolean
on_tick (gpointer user_data)
{
	ToddlerFun *toddlerfun = (ToddlerFun *) user_data;

	surface_brighten(toddlerfun);

	if (g_timer_elapsed (toddlerfun->message_timer, NULL) >= 5)
		update_message (toddlerfun);

	gtk_widget_queue_draw (toddlerfun->darea);
	return TRUE;
}

static gboolean
on_brighten_quickly_timeout (gpointer user_data)
{
	ToddlerFun *toddlerfun = (ToddlerFun *) user_data;
	surface_brighten(toddlerfun);
	gtk_widget_queue_draw (toddlerfun->darea);
	return (--toddlerfun->brighten_count > 0);
}

static void 
brighten_quickly(ToddlerFun *toddlerfun) 
{
	toddlerfun->brighten_count = 40;
	g_timeout_add (1000 / 30, on_brighten_quickly_timeout, toddlerfun);
}

static void
effect_up (ToddlerFun *toddlerfun)
{
	if (toddlerfun->effect_num < toddlerfun_effect_max) {
		brighten_quickly (toddlerfun);
		toddlerfun->effect_num++;
	}
}

static void
effect_down (ToddlerFun *toddlerfun)
{
	if (toddlerfun->effect_num > toddlerfun_effect_min) {
		brighten_quickly (toddlerfun);
		toddlerfun->effect_num--;
	}
}

static gboolean
on_scroll (GtkWidget *widget,
		   GdkEventScroll *event,
		   ToddlerFun *toddlerfun)
{
	if (event->direction == GDK_SCROLL_UP) {
		effect_up (toddlerfun);
		return TRUE;
	}
	if (event->direction == GDK_SCROLL_DOWN) {
		effect_down (toddlerfun);
		return TRUE;
	}
	return FALSE;
}

static gboolean
on_key_press(GtkWidget *widget,
			 GdkEventKey *event,
			 ToddlerFun *toddlerfun)
{
	gunichar c;
	gboolean is_key_repeat;

	is_key_repeat = event->keyval == toddlerfun->last_keyval;
	toddlerfun->last_keyval = event->keyval;

	switch (event->keyval) {
	case GDK_KEY_space:
		brighten_quickly (toddlerfun);
		return TRUE;

	case GDK_KEY_Escape:
		gtk_main_quit();
		return TRUE;

	case GDK_KEY_Up:
		effect_up (toddlerfun);
        break;

	case GDK_KEY_Down:
		effect_down (toddlerfun);
        break;

	case GDK_KEY_Return:
		save_picture (toddlerfun);
		break;

	default:
		c = gdk_keyval_to_unicode (event->keyval);
		if (c >= 0 && g_unichar_isgraph (c) && toddlerfun->has_previous) {
			gchar outbuf[7];
			gint bytes_written;
			bytes_written = g_unichar_to_utf8 (c, outbuf);
			outbuf[bytes_written] = '\0';

			if (is_key_repeat) {
				toddlerfun->letter_hue += 0.01;
				if (toddlerfun->letter_hue >= 1.0) 
					toddlerfun->letter_hue -= 1.0;
			} else {
				toddlerfun->letter_x = toddlerfun->previous_x;
				toddlerfun->letter_y = toddlerfun->previous_y;
				toddlerfun->letter_hue = g_random_double ();
			}

			print_string (outbuf, toddlerfun);
		}	
	}

	return FALSE;
}

static gboolean
on_key_release(GtkWidget *widget,
			   GdkEventKey *event,
			   ToddlerFun *toddlerfun)
{
	toddlerfun->last_keyval = GDK_KEY_VoidSymbol;

	return FALSE;
}

static GtkWidget*
create_window (ToddlerFun *toddlerfun, gboolean fullscreen)
{
	GtkWidget *window, *darea;

	window = toddlerfun->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	darea = toddlerfun->darea = gtk_drawing_area_new ();
	gtk_container_add (GTK_CONTAINER (window), darea);

	gtk_window_set_title (GTK_WINDOW (window), "ToddlerFun");
	if (fullscreen)
		gtk_window_fullscreen (GTK_WINDOW (window));
	else 
		gtk_window_set_default_size (GTK_WINDOW (window), 1000, 800);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
	g_signal_connect (darea, "draw", G_CALLBACK (on_draw), toddlerfun);
	g_signal_connect (darea, "configure-event", 
					  G_CALLBACK (on_configure), toddlerfun); 
    g_signal_connect (darea, "motion-notify-event",
					  G_CALLBACK (on_motion_notify), toddlerfun);
	g_signal_connect (darea, "button-press-event", 
					  G_CALLBACK (on_button_press), toddlerfun); 
	g_signal_connect (darea, "scroll-event",
					  G_CALLBACK (on_scroll), toddlerfun);
    g_signal_connect (window, "key-press-event",
					  G_CALLBACK (on_key_press), toddlerfun);
	g_signal_connect (window, "key-release-event",
					  G_CALLBACK (on_key_release), toddlerfun);
	gtk_widget_set_events(darea,
						  gtk_widget_get_events(window) | 
						  GDK_BUTTON_PRESS_MASK |
						  GDK_POINTER_MOTION_MASK |
						  GDK_SCROLL_MASK);

	g_timeout_add_seconds(2, on_tick, toddlerfun);
		
	return window;
}

static RsvgHandle *
load_image (const gchar *file_name) 
{
	GError *error = NULL;
	RsvgHandle *handle;

	handle = rsvg_handle_new_from_file (file_name, &error);
	if (error != NULL) {
		g_printerr (_("Can't load %s: %s\n"), file_name, error->message);
		g_clear_error (&error);
		return NULL;
	}

	return handle;
}

static void
load_theme (ToddlerFun *toddlerfun)
{
	toddlerfun->theme = theme_new ();
	theme_read (toddlerfun->theme, DATADIR "/defaulttheme/theme.xml");
	if (toddlerfun->theme->parsed_ok) {
		gint i;
		gint len = theme_get_n_objects (toddlerfun->theme);
		for (i = 0; i < len; i++) {
			ToddlerFunThemeObject *obj = theme_get_object (toddlerfun->theme, i);
			if (obj->image_file != NULL) {
				obj->image_handle = load_image (obj->image_file);
			}
		}
	}
}

#ifdef ENABLE_NLS
static void
translate_messages (void) 
{
	gint i;
	for (i = 0; i < NUM_MESSAGES; i++) {
		toddlerfun_messages[i] = gettext (toddlerfun_messages[i]);
	}
}
#endif

int
main (int argc, char *argv[])
{
	ToddlerFun *toddlerfun;
 	GtkWidget *window;
	GOptionContext *option_context;
	GError *error = NULL;
	gboolean no_fullscreen = FALSE;
	gboolean no_music = FALSE;
	gboolean no_sound_fx = FALSE;

	GOptionEntry options [] =
	{
		{ "no-fullscreen", 'F', 0, G_OPTION_ARG_NONE, &no_fullscreen,
		  N_("Don't run in fullscreen mode"), NULL },
		{ "no-music", 'M', 0, G_OPTION_ARG_NONE, &no_music,
		  N_("Don't play music"), NULL },
		{ "no-sound-fx", 'S', 0, G_OPTION_ARG_NONE, &no_sound_fx,
		  N_("Don't play sound effects"), NULL },
		{ NULL }
	};

	setlocale(LC_ALL, "");

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	translate_messages ();
#endif

	toddlerfun = g_new0(ToddlerFun, 1);

	g_set_prgname("toddlerfun");
	g_set_application_name(_("Toddler Fun"));
	option_context = g_option_context_new (NULL);
	g_option_context_set_translation_domain(option_context, GETTEXT_PACKAGE);
	g_option_context_set_summary(option_context, 
								 _("A drawing toy for toddlers."));
	g_option_context_add_main_entries(option_context, options, 
                                      GETTEXT_PACKAGE);
	g_option_context_add_group (option_context, gtk_get_option_group (TRUE));

	if (!g_option_context_parse (option_context, &argc, &argv, &error)) {
		g_print (_("option parsing failed: %s\n"), error->message);
		return (1);
    }

	gtk_init (&argc, &argv);
	gst_init (&argc, &argv);

	load_theme (toddlerfun);

	if (!no_music && toddlerfun->theme->background_sound_file != NULL)
		play_sound (toddlerfun->theme->background_sound_file, TRUE);

	toddlerfun->play_sound_fx = !no_sound_fx;

	toddlerfun->message_num = -1;
	update_message (toddlerfun);
	toddlerfun->message_alpha = 0.8;
	toddlerfun->has_message = TRUE;

	window = create_window (toddlerfun, !no_fullscreen);
	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
