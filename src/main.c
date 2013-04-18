/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) 2013 Simon Kågedal Reimer <simon@helgo.net>
 * 
 */

#include <config.h>
#include <math.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <librsvg/rsvg.h>
#include <pango/pangocairo.h>
#include <gst/gst.h>
#include "theme.h"

/* FIXME: this should be done the proper way */
#define GETTEXT_PACKAGE "skamine"
#define PACKAGE_LOCALE_DIR "po"

static const gint gamine_effect_min = 0;
static const gint gamine_effect_max = 8;
static const gdouble gamine_color_cycle_distance = 2000;
static const gdouble gamine_svg_size = 100;
static const gdouble gamine_min_rotation = G_PI * -0.2;
static const gdouble gamine_max_rotation = G_PI * 0.2;

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

	// Messages
	gint message_num;
	gboolean has_message;
	cairo_surface_t *message_surface;
	gdouble message_alpha;
	GTimer *message_timer;

	gboolean play_sound_fx;
	GamineTheme *theme;

	// These are active during a draw
	cairo_region_t *region;
	gint x;
	gint y;
	gint object_num;
	gdouble image_rotation;
	gchar *str;
} Gamine;

typedef void (*GamineDrawFunc) (Gamine *gamine, cairo_t *cr);

typedef struct {
    GstElement *elt;
    gboolean repeat;
} GamineSound;

gchar *gamine_messages [] = {
	N_("Welcome to Toddler Fun! Move the mouse around to draw. Press Escape to exit."),
	N_("Press mouse buttons to add funny images. Press keys on the keyboard to add letters."),
	N_("Press Return to save the current image to a file."),
	N_("Use the mouse scroll wheel or the Up/Down keys to change mirror effect."),
	N_("Press Space to clear drawing.")
};

#define NUM_MESSAGES (sizeof (gamine_messages) / sizeof (gamine_messages [0]))

//
// Sound
//

static void
eos_message_received (GstBus *bus, GstMessage *message, GamineSound *sound)
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
    gchar *filename, *cwd;
    GstElement *pipeline;
	GstBus *bus;

    pipeline = gst_element_factory_make("playbin", "playbin");
    if (pipeline != NULL) {
        GamineSound *closure = g_new (GamineSound, 1);
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
			cwd = g_get_current_dir ();
			filesnd = g_build_filename(cwd, filesnd, NULL);
            filename = g_strdup_printf("file://%s", filesnd);
            g_object_set (G_OBJECT(pipeline), "uri", filename, NULL);
            gst_element_set_state (GST_ELEMENT(pipeline), GST_STATE_PLAYING);
			g_free (filename);
			g_free (filesnd);
			g_free (cwd);
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
add_user_rectangle_to_region (Gamine *gamine, cairo_t *cr,
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

	status = cairo_region_union_rectangle(gamine->region, &rectangle);
	g_assert(status == CAIRO_STATUS_SUCCESS);
}

static void 
add_stroke_to_region(Gamine *gamine, cairo_t *cr)
{
	double x1, y1, x2, y2;
	cairo_stroke_extents (cr, &x1, &y1, &x2, &y2);
	add_user_rectangle_to_region (gamine, cr, x1, y1, x2, y2);
}

static void 
draw_line(Gamine *gamine, cairo_t *cr) 
{
	if (gamine->has_previous)
		cairo_move_to (cr, gamine->previous_x, gamine->previous_y);
	else
		cairo_move_to (cr, gamine->x, gamine->y);
	cairo_line_to (cr, gamine->x, gamine->y);
	add_stroke_to_region (gamine, cr);
	cairo_stroke (cr);
}

static void
draw_image(Gamine *gamine, cairo_t *cr)
{
	GamineThemeObject *obj;
	RsvgHandle *handle;
	RsvgDimensionData dimension;
	gdouble hypothenuse, scale;

	obj = theme_get_object(gamine->theme, gamine->object_num);
	if (obj == NULL)
		return;

	handle = obj->image_handle;
	if (obj == NULL)
		return;

	cairo_save (cr);
	
	rsvg_handle_get_dimensions (handle, &dimension);
	hypothenuse = sqrt(dimension.width * dimension.width + 
					   dimension.height * dimension.height);
	scale = gamine_svg_size / hypothenuse;

	cairo_translate (cr, gamine->x, gamine->y);
	cairo_scale (cr, scale, scale);
	cairo_translate (cr, -dimension.width / 2, -dimension.height / 2);
	cairo_rotate (cr, gamine->image_rotation);
	rsvg_handle_render_cairo (handle, cr);

	add_user_rectangle_to_region (gamine, cr, 0, 0, 
								  dimension.width, dimension.height);

	cairo_restore (cr);
}

static void
draw_string (Gamine *gamine, cairo_t *cr)
{
// FIXME
}

static void 
draw_effect (Gamine *gamine,
			 cairo_t *cr,
			 GamineDrawFunc draw)
{
    cairo_surface_t *surface = gamine->surface;
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);
    int mirror = gamine->effect_num > 0;
    int rotations = gamine->effect_num > 0 ? gamine->effect_num : 1;
    double angle_step = G_PI * 2 / rotations;
    int center_x = width / 2;
    int center_y = height / 2;
    int rot;

    cairo_save(cr);
    for (rot = 0; rot < rotations; rot++) {
        cairo_translate (cr, center_x, center_y);
        cairo_rotate (cr, angle_step);
        cairo_translate (cr, -center_x, -center_y);
        (*draw) (gamine, cr);
        if (mirror) {
            cairo_save (cr);
            cairo_translate (cr, center_x, center_y);
            cairo_scale (cr, -1, 1);
            cairo_translate (cr, -center_x, -center_y);
            (*draw) (gamine, cr);
            cairo_restore (cr);
        }
    }
    cairo_restore(cr);
	
}

static void
surface_clear (Gamine *gamine) 
{
	cairo_t *cr;

	cr = cairo_create (gamine->surface);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_paint (cr);
	
	cairo_destroy(cr);
}

static void
surface_brighten (Gamine *gamine)
{
	cairo_t *cr;

	cr = cairo_create (gamine->surface);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_paint_with_alpha (cr, 0.1);
	
	cairo_destroy(cr);
}

static void
render_message (Gamine *gamine) 
{
	cairo_t *cr;
	PangoLayout *layout;
	PangoFontDescription *desc;
	gint width, height;

	// Create surface if we haven't already
	if (gamine->message_surface == NULL) {
		gamine->message_surface = 
			cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
										2000, 40);
	}

	cr = cairo_create (gamine->message_surface);

	// Clear surface
	cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	// Layout text
	layout = pango_cairo_create_layout (cr);
	pango_layout_set_text (layout, gamine_messages[gamine->message_num], -1);
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
update_message (Gamine *gamine)
{
	if (gamine->message_timer == NULL)
		gamine->message_timer = g_timer_new ();
	g_timer_start (gamine->message_timer);

	gamine->message_num = (gamine->message_num + 1) % NUM_MESSAGES;

	render_message (gamine);
}

static void
save_picture (Gamine *gamine)
{
	GDateTime *datetime;
	gchar *dirname;
 
	
    struct stat st;
    gchar *filename;
    gchar *pathname;
    time_t timestamp;
    struct tm * t;
    cairo_surface_t *surface;
    timestamp = time(NULL);
    t = localtime(&timestamp);
 
    dirname = g_build_filename(g_get_home_dir(), "gamine", NULL);
    //if dirname not exists

    if(!g_file_test (dirname, G_FILE_TEST_EXISTS)) {
        if (g_mkdir(dirname, 0750) < 0)
            g_printerr(_("Failed to create directory '%s'\n"),
					   dirname);
	}

	datetime = g_date_time_new_now_local ();
	filename = g_date_time_format (datetime, "%F_%R-%S.png");
    pathname = g_build_filename(dirname, filename, NULL);

    if (cairo_surface_write_to_png(gamine->surface, pathname) < 0)
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
	Gamine *gamine;
	gint width, height, old_width, old_height;
	cairo_surface_t *old_surface = NULL;

	gamine = (Gamine *) user_data;
	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);

	old_surface = gamine->surface;

	if (old_surface != NULL) {
		old_width = cairo_image_surface_get_width (old_surface);
		old_height = cairo_image_surface_get_height (old_surface);

		if (old_width == width && old_height == height) 
			return TRUE;
	}
	
	gamine->surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
												  width, height);

	if (old_surface != NULL) {
		cairo_t *cr = cairo_create (gamine->surface);
		cairo_scale (cr, 
					 (gdouble) width / (gdouble) old_width,
					 (gdouble) height / (gdouble) old_height);
		cairo_set_source_surface (cr, old_surface, 0, 0);
		cairo_paint (cr);
		cairo_destroy (cr);
		cairo_surface_destroy (old_surface);
	} else {
		surface_clear(gamine);
	}

	gamine->has_previous = FALSE;
	
	return TRUE;
}

static gboolean 
on_draw(GtkWidget *window, 
		cairo_t *cr,
        gpointer user_data)
{
	Gamine *gamine;
	gint height;

	gamine = (Gamine *) user_data;

	if (gamine == NULL || gamine->surface == NULL)
		return TRUE;
	
	cairo_set_source_surface (cr, gamine->surface, 0, 0);
	cairo_paint (cr);

	if (gamine->has_message) {
		height = cairo_image_surface_get_height (gamine->surface);
		cairo_set_source_surface (cr, gamine->message_surface,
								  10, height - 50);
		cairo_paint_with_alpha (cr, gamine->message_alpha);
	}

	return FALSE;
}

static void
update_color (Gamine *gamine,
			  cairo_t *cr)
{
	gdouble hue, r, g, b;
	if (gamine->has_previous) {
		gdouble new_distance;
		gint xdiff, ydiff;

		xdiff = gamine->x - gamine->previous_x;
		ydiff = gamine->y - gamine->previous_y;
		new_distance = sqrt(xdiff * xdiff + ydiff * ydiff);

		gamine->traveled_distance += new_distance;
		while (gamine->traveled_distance > gamine_color_cycle_distance)
			gamine->traveled_distance -= gamine_color_cycle_distance;
	}

	hue = gamine->traveled_distance / gamine_color_cycle_distance;
	gtk_hsv_to_rgb (hue, 1.0, 1.0, &r, &g, &b);

	cairo_set_source_rgba (cr, r, g, b, 0.7);
}

static gboolean
on_motion_notify (GtkWidget *widget,
				  GdkEventButton *event,
				  Gamine *gamine)
{
	cairo_t *cr = cairo_create (gamine->surface);
	cairo_set_source_rgba(cr, 1, 0, 0, 0.3);
	cairo_set_line_width(cr, 5);

	gamine->region = cairo_region_create ();
	gamine->x = event->x;
	gamine->y = event->y;

	update_color (gamine, cr);

//	draw_line (gamine, cr);
	draw_effect (gamine, cr, &draw_line);

	cairo_destroy(cr);

	gtk_widget_queue_draw_region (widget, gamine->region);

	cairo_region_destroy(gamine->region);
	gamine->region = NULL;

	gamine->previous_x = event->x;
	gamine->previous_y = event->y;
	gamine->has_previous = TRUE;

	return TRUE;
}				 

static gboolean
on_button_press (GtkWidget *widget,
				 GdkEventButton *event,
				 Gamine *gamine)
{
	GamineThemeObject *obj;
	gint num_objects;
	cairo_t *cr = cairo_create (gamine->surface);

	gamine->region = cairo_region_create ();
	gamine->x = event->x;
	gamine->y = event->y;
	num_objects = gamine->theme->theme_objects->len;
	gamine->object_num = g_random_int_range (0, num_objects);
	gamine->image_rotation = g_random_double_range (gamine_min_rotation,
													gamine_max_rotation);

	obj = theme_get_object (gamine->theme, gamine->object_num);
	play_sound (obj->sound_file, FALSE);

	draw_effect (gamine, cr, &draw_image);
	
	cairo_destroy(cr);

	gtk_widget_queue_draw_region (widget, gamine->region);
	cairo_region_destroy(gamine->region);
	gamine->region = NULL;

	return TRUE;
}

static void
print_string (gchar *s, Gamine *gamine)
{
	cairo_t *cr = cairo_create (gamine->surface);
	GdkDeviceManager *device_manager;
	GdkDevice *pointer;

	gamine->region = cairo_region_create ();

	gamine->x = gamine->previous_x;
	gamine->y = gamine->previous_y;

	draw_effect (gamine, cr, &draw_string);
	
	cairo_destroy (cr);

	gtk_widget_queue_draw_region (gamine->darea, gamine->region);
	cairo_region_destroy (gamine->region);
	gamine->region = NULL;
}

static gboolean
on_tick (gpointer user_data)
{
	Gamine *gamine = (Gamine *) user_data;

	surface_brighten(gamine);

	if (g_timer_elapsed (gamine->message_timer, NULL) >= 5)
		update_message (gamine);

	gtk_widget_queue_draw (gamine->darea);
	return TRUE;
}

static gboolean
on_brighten_quickly_timeout (gpointer user_data)
{
	Gamine *gamine = (Gamine *) user_data;
	surface_brighten(gamine);
	gtk_widget_queue_draw (gamine->darea);
	return (--gamine->brighten_count > 0);
}

static void 
brighten_quickly(Gamine *gamine) 
{
	gamine->brighten_count = 40;
	g_timeout_add (1000 / 30, on_brighten_quickly_timeout, gamine);
}

static void
effect_up (Gamine *gamine)
{
	if (gamine->effect_num < gamine_effect_max) {
		brighten_quickly (gamine);
		gamine->effect_num++;
	}
}

static void
effect_down (Gamine *gamine)
{
	if (gamine->effect_num > gamine_effect_min) {
		brighten_quickly (gamine);
		gamine->effect_num--;
	}
}

static gboolean
on_scroll (GtkWidget *widget,
		   GdkEventScroll *event,
		   Gamine *gamine)
{
	if (event->direction == GDK_SCROLL_UP) {
		effect_up (gamine);
		return TRUE;
	}
	if (event->direction == GDK_SCROLL_DOWN) {
		effect_down (gamine);
		return TRUE;
	}
	return FALSE;
}

static gboolean
on_key_press(GtkWidget *widget,
			 GdkEventKey *event,
			 Gamine *gamine)
{
	gunichar c;
	if (event->type == GDK_KEY_PRESS) {
		switch (event->keyval) {
		case GDK_KEY_space:
			brighten_quickly (gamine);
			return TRUE;

		case GDK_KEY_Escape:
			gtk_main_quit();
			return TRUE;

		case GDK_KEY_Up:
			effect_up (gamine);
            break;

		case GDK_KEY_Down:
			effect_down (gamine);
            break;

		case GDK_KEY_Return:
			save_picture (gamine);
			break;

		default:
			c = g_utf8_get_char_validated (event->string, -1);
			if (c >= 0 && g_unichar_isgraph (c) && gamine->has_previous) 
				print_string (event->string, gamine);
		}
	}

	return FALSE;
}

static GtkWidget*
create_window (Gamine *gamine, gboolean fullscreen)
{
	GtkWidget *window, *darea;

	window = gamine->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	darea = gamine->darea = gtk_drawing_area_new ();
	gtk_container_add (GTK_CONTAINER (window), darea);

	gtk_window_set_title (GTK_WINDOW (window), "Gamine");
	if (fullscreen)
		gtk_window_fullscreen (GTK_WINDOW (window));
	else 
		gtk_window_set_default_size (GTK_WINDOW (window), 1000, 800);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
	g_signal_connect (darea, "draw", G_CALLBACK (on_draw), gamine);
	g_signal_connect (darea, "configure_event", 
					  G_CALLBACK (on_configure), gamine); 
    g_signal_connect (darea, "motion_notify_event",
					  G_CALLBACK (on_motion_notify), gamine);
	g_signal_connect (darea, "button_press_event", 
					  G_CALLBACK (on_button_press), gamine); 
	g_signal_connect (darea, "scroll_event",
					  G_CALLBACK (on_scroll), gamine);
    g_signal_connect (window, "key_press_event",
					  G_CALLBACK (on_key_press), gamine);
	gtk_widget_set_events(darea,
						  gtk_widget_get_events(window) | 
						  GDK_BUTTON_PRESS_MASK |
						  GDK_POINTER_MOTION_MASK |
						  GDK_SCROLL_MASK);

	g_timeout_add_seconds(2, on_tick, gamine);
		
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
load_theme (Gamine *gamine)
{
	gamine->theme = theme_new ();
	theme_read (gamine->theme, "defaulttheme/theme.xml");
	if (gamine->theme->parsed_ok) {
		gint i;
		gint len = theme_get_n_objects (gamine->theme);
		for (i = 0; i < len; i++) {
			GamineThemeObject *obj = theme_get_object (gamine->theme, i);
			if (obj->image_file != NULL) {
				obj->image_handle = load_image (obj->image_file);
			}
		}
	}
}

int
main (int argc, char *argv[])
{
	Gamine *gamine;
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
		  N_("Don't play sound effects"), NULL }
	};

	setlocale(LC_ALL, "");

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	gamine = g_new0(Gamine, 1);

	gamine->play_sound_fx = !no_sound_fx;

	g_set_prgname("skamine");
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

	load_theme (gamine);

	if (!no_music && gamine->theme->background_sound_file)
		play_sound (gamine->theme->background_sound_file, TRUE);

	gamine->message_num = -1;
	update_message (gamine);
	gamine->message_alpha = 0.8;
	gamine->has_message = TRUE;

	window = create_window (gamine, !no_fullscreen);
	gtk_widget_show_all (window);

//	gtk_widget_set_app_paintable(gamine->darea, TRUE);

	gtk_main ();

	return 0;
}
