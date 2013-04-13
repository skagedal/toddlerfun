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

/* FIXME: this should be done the proper way */
#define ENABLE_NLS
#define GETTEXT_PACKAGE "skamine"
#define PACKAGE_LOCALE_DIR "po"

static const gint gamine_effect_min = 0;
static const gint gamine_effect_max = 8;
static const gdouble gamine_color_cycle_distance = 2000;
static const gdouble gamine_svg_size = 100;
static const gdouble gamine_min_rotation = G_PI * -0.2;
static const gdouble gamine_max_rotation = G_PI * 0.2;

static const gchar *gamine_image_file_names [] = 
{ "images/baby.svg", "images/cat.svg", "images/sheep.svg" };
#define NUM_IMAGES (sizeof(gamine_image_file_names)/sizeof (gchar *))

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
	
	RsvgHandle *images[NUM_IMAGES];

	// These are active during a draw
	cairo_region_t *region;
	gint x;
	gint y;
	gint image_num;
	gdouble image_rotation;
} Gamine;

typedef void (*GamineDrawFunc) (Gamine *gamine, cairo_t *cr);

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
	RsvgHandle *handle = gamine->images[gamine->image_num];
	RsvgDimensionData dimension;
	gdouble hypothenuse, scale;

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
	double width, height;

	gamine = (Gamine *) user_data;

	if (gamine == NULL || gamine->surface == NULL)
		return TRUE;
	
	cairo_set_source_surface (cr, gamine->surface, 0, 0);
	cairo_paint (cr);

	cairo_set_source_rgba(cr, 0, 1, 0, 0.1);
	cairo_rectangle(cr, 5, 5, 50, 50);
	cairo_fill(cr);
	
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
	cairo_t *cr = cairo_create (gamine->surface);
	
	gamine->region = cairo_region_create ();
	gamine->x = event->x;
	gamine->y = event->y;
	gamine->image_num = g_random_int_range (0, NUM_IMAGES);
	gamine->image_rotation = g_random_double_range (gamine_min_rotation,
													gamine_max_rotation);

//	draw_image (gamine, cr);
	draw_effect (gamine, cr, &draw_image);
	
	cairo_destroy(cr);

	gtk_widget_queue_draw_region (widget, gamine->region);
	cairo_region_destroy(gamine->region);
	gamine->region = NULL;

	return TRUE;
}

static gboolean
on_brighten_timeout(gpointer user_data)
{
	Gamine *gamine = (Gamine *) user_data;
	surface_brighten(gamine);
	gtk_widget_queue_draw (gamine->darea);
	return TRUE;
}

static gboolean
on_brighten_quickly_timeout(gpointer user_data)
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
	if (event->type == GDK_KEY_PRESS) {
		switch (event->keyval) {
		case GDK_KEY_space:
			brighten_quickly (gamine);
			return TRUE;

		case GDK_KEY_q:
			gtk_main_quit();
			return TRUE;

		case GDK_KEY_Up:
			effect_up (gamine);
            break;

		case GDK_KEY_Down:
			effect_down (gamine);
            break;

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
		gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);

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
	g_timeout_add_seconds(2, on_brighten_timeout, gamine);
		
	return window;
}

static void
load_images (Gamine *gamine) 
{
	int i;
	GError *error = NULL;
	for (i = 0; i < NUM_IMAGES; i++) {
		const gchar *file_name = gamine_image_file_names[i];
		gamine->images[i] = rsvg_handle_new_from_file (file_name, &error);
		if (error != NULL) {
			fprintf (stderr, "When loading %s:\n", file_name);
			fprintf (stderr, "Error: %s\n", error->message);
			g_clear_error (&error);
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

	g_set_prgname("skamine");
	g_set_application_name(_("Toddler Fun"));
	option_context = g_option_context_new (NULL);
	g_option_context_set_translation_domain(option_context, GETTEXT_PACKAGE);
	g_option_context_set_summary(option_context, "A drawing toy for toddlers.");
	g_option_context_add_main_entries(option_context, options, 
                                      GETTEXT_PACKAGE);
	g_option_context_add_group (option_context, gtk_get_option_group (TRUE));

	if (!g_option_context_parse (option_context, &argc, &argv, &error)) {
			g_print ("option parsing failed: %s\n", error->message);
			return (1);
    }

	gtk_init (&argc, &argv);

	load_images (gamine);

	window = create_window (gamine, !no_fullscreen);
	gtk_widget_show_all (window);

//	gtk_widget_set_app_paintable(gamine->darea, TRUE);

	gtk_main ();

	return 0;
}
