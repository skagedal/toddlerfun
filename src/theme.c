/*
 * theme.c
 * Reads theme files
 * Copyright (C) 2013 Simon Kågedal Reimer <simon@helgo.net>
 * 
 */

#include <config.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <librsvg/rsvg.h>
#include "theme.h"

typedef struct {
	ToddlerFunTheme *theme;
	gchar *dirname;
} ToddlerFunThemeParser;

static const gchar *
get_attribute (const char *name,
			   const gchar **attribute_names,
			   const gchar **attribute_values)
{
	while (*attribute_names != NULL) {
		if (strcmp (*attribute_names, name) == 0) 
			return *attribute_values;

		attribute_names++;
		attribute_values++;
	}
	return NULL;
}

static void parser_start_element (GMarkupParseContext *context,
								  const gchar         *element_name,
								  const gchar        **attribute_names,
								  const gchar        **attribute_values,
								  gpointer             user_data,
								  GError             **error)
{
	ToddlerFunThemeParser *parser = (ToddlerFunThemeParser *) user_data;

	if (strcmp (element_name, "objects") == 0) {
		guint size = sizeof (ToddlerFunThemeObject);
		if (parser->theme->theme_objects == NULL) 
			parser->theme->theme_objects = g_array_new (FALSE, TRUE, size);

	} else if (strcmp (element_name, "object") == 0) {
		ToddlerFunThemeObject obj;
		memset (&obj, 0, sizeof (ToddlerFunThemeObject));

		while (*attribute_names != NULL) {
			if (strcmp (*attribute_names, "sound") == 0) {
				const gchar *basename = *attribute_values;
				obj.sound_file = g_build_filename (parser->dirname,
												   basename,
												   NULL);
			}
			if (strcmp (*attribute_names, "image") == 0) {
				const gchar *basename = *attribute_values;
				obj.image_file = g_build_filename (parser->dirname,
												   basename,
												   NULL);
			}

			attribute_names++;
			attribute_values++;
		}

		g_array_append_val (parser->theme->theme_objects, obj);

	} else if (strcmp (element_name, "background") == 0) {
		const gchar *basename = get_attribute ("sound", 
											   attribute_names,
											   attribute_values);
		gchar *path = g_build_filename (parser->dirname,
										basename, NULL);
		parser->theme->background_sound_file = path;
	}
}

static void parser_end_element (GMarkupParseContext *context,
								const gchar         *element_name,
								gpointer             user_data,
								GError             **error)
{
	
}

static void parser_error (GMarkupParseContext *context,
                          GError              *error,
                          gpointer             user_data)
{
	g_printerr (_("Theme parser error\n"));
}

static GMarkupParser parser = {
	parser_start_element,
	parser_end_element,
	NULL, /* text */
	NULL, /* passthrough */
	parser_error
};
	
ToddlerFunTheme *theme_new (void) 
{
	return g_new0 (ToddlerFunTheme, 1);
}

void
theme_read (ToddlerFunTheme *theme, gchar *filename)
{
	ToddlerFunThemeParser parseinfo;
	GMarkupParseContext *context;
	gchar *xml;
	gsize length;
	GError *error = NULL;

	parseinfo.theme = theme;
	parseinfo.dirname = g_path_get_dirname (filename);

	if (!g_file_get_contents (filename, &xml, &length, &error)) {
		g_printerr ("%s\n", error->message);
		g_clear_error(&error);
		return;
	}

	context = g_markup_parse_context_new (&parser, 0, &parseinfo,  NULL);
	
	if (!g_markup_parse_context_parse (context, xml, length, &error)) {
		g_printerr (_("Parse error on theme file %s: %s\n"),
					filename, error->message);
		g_clear_error (&error);
		goto theme_read_cleanup;
	} 

	if (!g_markup_parse_context_end_parse (context, &error)) {
		g_printerr (_("Parse error on theme file %s: %s\n"),
					filename, error->message);
		g_clear_error (&error);
		goto theme_read_cleanup;
	}

	theme->parsed_ok = TRUE;

theme_read_cleanup:
	g_markup_parse_context_free (context);
	g_free (parseinfo.dirname);
}

ToddlerFunThemeObject *
theme_get_object (ToddlerFunTheme *theme, gint i)
{
	if (theme->theme_objects != NULL) {
		GArray *arr = theme->theme_objects;
		return &g_array_index (arr, ToddlerFunThemeObject, i);
	}
	return NULL;
}

gint
theme_get_n_objects (ToddlerFunTheme *theme)
{
	if (theme->theme_objects != NULL)
		return theme->theme_objects->len;
	return 0;
}
