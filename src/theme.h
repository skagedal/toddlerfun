/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

typedef struct {
	gchar *sound_file;
	gchar *image_file;
	RsvgHandle *image_handle;
} GamineThemeObject;

typedef struct {
	GArray *theme_objects;
	gchar *background_sound_file;
	gboolean parsed_ok;
} GamineTheme;

GamineTheme *theme_new (void);
void theme_read (GamineTheme *theme, gchar *filename);
GamineThemeObject *theme_get_object (GamineTheme *theme, gint i);
gint theme_get_n_objects (GamineTheme *theme);
