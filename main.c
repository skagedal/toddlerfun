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

/* FIXME: this should be done the proper way */
#define ENABLE_NLS
#define GETTEXT_PACKAGE "skamine"
#define PACKAGE_LOCALE_DIR "po"

static const gint gamine_effect_min = 0;
static const gint gamine_effect_max = 8;
static const gdouble gamine_color_cycle_distance = 2000;

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

	// These are active during a draw
	cairo_region_t *region;
	gint x;
	gint y;
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
add_stroke_to_region(Gamine *gamine, cairo_t *cr)
{
	const int TOP_LEFT = 0;
	const int BOTTOM_RIGHT = 1;
	const int TOP_RIGHT = 2;
	const int BOTTOM_LEFT = 3;
	double x[4], y[4];
	cairo_rectangle_int_t rectangle;
	cairo_status_t status;
	int i;

	cairo_stroke_extents (cr, 
						  &x[TOP_LEFT], &y[TOP_LEFT], 
						  &x[BOTTOM_RIGHT], &y[BOTTOM_RIGHT]);
	/*
	g_message("Stroke extents: (%f, %f), (%f, %f)\n",
			  x[TOP_LEFT], y[TOP_LEFT], x[BOTTOM_RIGHT], y[BOTTOM_RIGHT]);
	*/

	x[TOP_RIGHT] = x[BOTTOM_RIGHT];
	y[TOP_RIGHT] = y[TOP_LEFT];
	x[BOTTOM_LEFT] = x[TOP_LEFT];
	y[BOTTOM_LEFT] = y[BOTTOM_RIGHT];

	for (i = 0; i < 4; i++)
		cairo_user_to_device (cr, &x[i], &y[i]);

	rectangle.x = min_doubles(x, 4);
	rectangle.y = min_doubles(y, 4);
	rectangle.width = max_doubles(x, 4) - rectangle.x;
	rectangle.height = max_doubles(y, 4) - rectangle.y;

	status = cairo_region_union_rectangle(gamine->region, &rectangle);
	g_assert(status == CAIRO_STATUS_SUCCESS);

	/*
	g_message("Rectangle: (%d, %d), (%d, %d)\n",
			  rectangle.x, rectangle.y, 
			  rectangle.x + rectangle.width, rectangle.y + rectangle.height);
	*/
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
	GdkWindow *window;
	gint width, height;

	gamine = (Gamine *) user_data;
	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);

	if (gamine->surface != NULL) {
		if (cairo_image_surface_get_width (gamine->surface) == width &&
			cairo_image_surface_get_height (gamine->surface) == height) {
			return TRUE;
		}

		cairo_surface_destroy (gamine->surface);
	}
	
	window = gtk_widget_get_window (widget);

// What would be the benifit of doing this instead?
//	gamine->surface = gdk_window_create_similar_surface (window, 
//														 CAIRO_CONTENT_COLOR,
//														 width, height);

	gamine->surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
	width, height);

	printf("Created surface of size %d, %d\n", width, height);

	surface_clear(gamine);
	
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

	cairo_set_source_rgba (cr, r, g, b, 0.3);
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
	draw_line (gamine, cr);

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

static gboolean
on_key_press(GtkWidget *widget,
			 GdkEventKey *event,
			 Gamine *gamine)
{
	if (event->type == GDK_KEY_PRESS) {
		switch (event->keyval) {
		case GDK_KEY_space:
			brighten_quickly(gamine);
			return TRUE;

		case GDK_KEY_q:
			gtk_main_quit();
			return TRUE;
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
/*	g_signal_connect (darea, "button_press_event", 
	G_CALLBACK (on_button_press), gamine); */
    g_signal_connect (window, "key_press_event",
					  G_CALLBACK (on_key_press), gamine);
	gtk_widget_set_events(darea,
						  gtk_widget_get_events(window) | 
						  GDK_BUTTON_PRESS_MASK |
						  GDK_POINTER_MOTION_MASK);
	g_timeout_add_seconds(10, on_brighten_timeout, gamine);
		
	return window;
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

	window = create_window (gamine, !no_fullscreen);
	gtk_widget_show_all (window);

//	gtk_widget_set_app_paintable(gamine->darea, TRUE);

	gtk_main ();

	return 0;
}
