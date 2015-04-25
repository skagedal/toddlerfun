/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

typedef struct {
	gchar *sound_file;
	gchar *image_file;
	RsvgHandle *image_handle;
} ToddlerFunThemeObject;

typedef struct {
	GArray *theme_objects;
	gchar *background_sound_file;
	gboolean parsed_ok;
} ToddlerFunTheme;

ToddlerFunTheme *theme_new (void);
void theme_read (ToddlerFunTheme *theme, gchar *filename);
ToddlerFunThemeObject *theme_get_object (ToddlerFunTheme *theme, gint i);
gint theme_get_n_objects (ToddlerFunTheme *theme);
