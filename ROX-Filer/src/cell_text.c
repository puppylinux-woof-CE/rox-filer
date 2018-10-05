/*
 * ROX-Filer, filer for the ROX desktop project
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* cell_text.c - a GtkCellRenderer used for the text columns in details mode
 *
 * Based on gtkcellrenderertext.c.
 */

#include "config.h"

#include <gtk/gtk.h>

#include "global.h"

#include "cell_text.h"


enum {
	PROP_0,

	PROP_TEXT,
	PROP_MARKUP,
	PROP_ATTRIBUTES,
	PROP_SINGLE_PARAGRAPH_MODE,
	PROP_WIDTH_CHARS,
	PROP_WRAP_WIDTH,
	PROP_ALIGN,

	/* Style args */
	PROP_BACKGROUND,
	PROP_FOREGROUND,
	PROP_BACKGROUND_GDK,
	PROP_FOREGROUND_GDK,
	PROP_FONT,
	PROP_FONT_DESC,
	PROP_FAMILY,
	PROP_STYLE,
	PROP_VARIANT,
	PROP_WEIGHT,
	PROP_STRETCH,
	PROP_SIZE,
	PROP_SIZE_POINTS,
	PROP_SCALE,
	PROP_EDITABLE,
	PROP_STRIKETHROUGH,
	PROP_UNDERLINE,
	PROP_RISE,
	PROP_LANGUAGE,
	PROP_ELLIPSIZE,
	PROP_WRAP_MODE,

	/* Whether-a-style-arg-is-set args */
	PROP_BACKGROUND_SET,
	PROP_FOREGROUND_SET,
	PROP_FAMILY_SET,
	PROP_STYLE_SET,
	PROP_VARIANT_SET,
	PROP_WEIGHT_SET,
	PROP_STRETCH_SET,
	PROP_SIZE_SET,
	PROP_SCALE_SET,
	PROP_EDITABLE_SET,
	PROP_STRIKETHROUGH_SET,
	PROP_UNDERLINE_SET,
	PROP_RISE_SET,
	PROP_LANGUAGE_SET,
	PROP_ELLIPSIZE_SET,
	PROP_ALIGN_SET,

	PROP_SHOW_SELECTION_STATE
};

typedef struct _CellText CellText;
typedef struct _CellTextClass CellTextClass;

struct _CellText {
	GtkCellRendererText parent;

	gboolean show_selection_state;
};

struct _CellTextClass {
	GtkCellRendererTextClass parent_class;
};


#define CELL_TEXT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), cell_text_get_type (), CellTextPrivate))

typedef struct _CellTextPrivate CellTextPrivate;
struct _CellTextPrivate
{
	guint single_paragraph : 1;
	guint language_set : 1;
	guint markup_set : 1;
	guint ellipsize_set : 1;
	guint align_set : 1;

	gulong focus_out_id;
	PangoLanguage *language;
	PangoEllipsizeMode ellipsize;
	PangoWrapMode wrap_mode;
	PangoAlignment align;

	gulong populate_popup_id;
	gulong entry_menu_popdown_timeout;
	gboolean in_entry_menu;

	gint width_chars;
	gint wrap_width;

	GtkWidget *entry;
};


/* Static prototypes */
static void cell_text_finalize(GObject *object);

static void cell_text_get_property(GObject    *object,
                                   guint      param_id,
                                   GValue     *value,
                                   GParamSpec *pspec);
static void cell_text_set_property(GObject      *object,
                                   guint        param_id,
                                   const GValue *value,
                                   GParamSpec   *pspec);
static void cell_text_init(CellText *celltext);
static void cell_text_class_init(CellText *class);
static void cell_text_render (GtkCellRenderer      *cell,
                              GdkDrawable          *window,
                              GtkWidget            *widget,
                              GdkRectangle         *background_area,
                              GdkRectangle         *cell_area,
                              GdkRectangle         *expose_area,
                              GtkCellRendererState flags);
static GtkType cell_text_get_type(void);
static void get_size (GtkCellRenderer *cell,
                      GtkWidget       *widget,
                      GdkRectangle    *cell_area,
                      PangoLayout     *layout,
                      gint            *x_offset,
                      gint            *y_offset,
                      gint            *width,
                      gint            *height);
static void cell_text_get_size (GtkCellRenderer *cell,
                                GtkWidget       *widget,
                                GdkRectangle    *cell_area,
                                gint            *x_offset,
                                gint            *y_offset,
                                gint            *width,
                                gint            *height);
static PangoLayout* get_layout (GtkCellRendererText  *celltext,
                                GtkWidget            *widget,
                                gboolean             will_render,
                                GtkCellRendererState flags);
static void add_attr (PangoAttrList  *attr_list,
                      PangoAttribute *attr);

static gpointer cell_text_parent_class = NULL;

/****************************************************************
 *                      EXTERNAL INTERFACE                      *
 ****************************************************************/

GtkCellRenderer *cell_text_new(void)
{
	GtkCellRenderer *cell;

	cell = GTK_CELL_RENDERER(g_object_new(cell_text_get_type(), NULL));

	return cell;
}

/****************************************************************
 *                      INTERNAL FUNCTIONS                      *
 ****************************************************************/


static GtkType cell_text_get_type(void)
{
	static GtkType cell_text_type = 0;

	if (!cell_text_type)
	{
		static const GTypeInfo cell_text_info =
		{
			sizeof (CellTextClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) cell_text_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (CellText),
			0,              /* n_preallocs */
			(GInstanceInitFunc) cell_text_init,
		};

		cell_text_type = g_type_register_static(GTK_TYPE_CELL_RENDERER_TEXT,
							"CellText",
							&cell_text_info, 0);
	}

	return cell_text_type;
}

static void cell_text_init(CellText *celltext)
{
	CellTextPrivate *priv;

	priv = CELL_TEXT_GET_PRIVATE(celltext);

	celltext->show_selection_state = TRUE;

	priv->width_chars = -1;
	priv->wrap_width = -1;
	priv->wrap_mode = PANGO_WRAP_CHAR;
	priv->align = PANGO_ALIGN_LEFT;
	priv->align_set = FALSE;
}

static void cell_text_class_init(CellText *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS(class);

	cell_text_parent_class = g_type_class_peek_parent (class);

	object_class->finalize = cell_text_finalize;

	object_class->get_property = cell_text_get_property;
	object_class->set_property = cell_text_set_property;

	cell_class->get_size = cell_text_get_size;
	cell_class->render = cell_text_render;

	g_object_class_install_property(object_class,
				PROP_SHOW_SELECTION_STATE,
				g_param_spec_boolean("show_selection_state",
						"Show selection state",
						"Render cell differently when selected.",
						TRUE,
						G_PARAM_WRITABLE));

	g_type_class_add_private (class, sizeof (CellTextPrivate));
}

static void cell_text_finalize(GObject *object)
{
	CellTextPrivate *priv;

	priv = CELL_TEXT_GET_PRIVATE((CellText *) object);

	if (priv->language)
		g_object_unref (priv->language);

	G_OBJECT_CLASS (cell_text_parent_class)->finalize (object);
}

static PangoFontMask get_property_font_set_mask(guint prop_id)
{
	switch (prop_id)
	{
		case PROP_FAMILY_SET:
			return PANGO_FONT_MASK_FAMILY;
		case PROP_STYLE_SET:
			return PANGO_FONT_MASK_STYLE;
		case PROP_VARIANT_SET:
			return PANGO_FONT_MASK_VARIANT;
		case PROP_WEIGHT_SET:
			return PANGO_FONT_MASK_WEIGHT;
		case PROP_STRETCH_SET:
			return PANGO_FONT_MASK_STRETCH;
		case PROP_SIZE_SET:
			return PANGO_FONT_MASK_SIZE;
	}

	return 0;
}

static void cell_text_get_property(GObject    *object,
                                   guint      param_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
	GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT(object);
	CellText *mycelltext = (CellText *) object;
	CellTextPrivate *priv;

	priv = CELL_TEXT_GET_PRIVATE((CellText *) object);

	switch (param_id)
	{
		case PROP_TEXT:
			g_value_set_string (value, celltext->text);
			break;

		case PROP_ATTRIBUTES:
			g_value_set_boxed (value, celltext->extra_attrs);
			break;

		case PROP_SINGLE_PARAGRAPH_MODE:
			g_value_set_boolean (value, priv->single_paragraph);
			break;

		case PROP_BACKGROUND_GDK:
			{
				GdkColor color;

				color.red = celltext->background.red;
				color.green = celltext->background.green;
				color.blue = celltext->background.blue;

				g_value_set_boxed (value, &color);
			}
			break;

		case PROP_FOREGROUND_GDK:
			{
				GdkColor color;

				color.red = celltext->foreground.red;
				color.green = celltext->foreground.green;
				color.blue = celltext->foreground.blue;

				g_value_set_boxed (value, &color);
			}
			break;

		case PROP_FONT:
			g_value_take_string (value, pango_font_description_to_string (celltext->font));
			break;

		case PROP_FONT_DESC:
			g_value_set_boxed (value, celltext->font);
			break;

		case PROP_FAMILY:
			g_value_set_string (value, pango_font_description_get_family (celltext->font));
			break;

		case PROP_STYLE:
			g_value_set_enum (value, pango_font_description_get_style (celltext->font));
			break;

		case PROP_VARIANT:
			g_value_set_enum (value, pango_font_description_get_variant (celltext->font));
			break;

		case PROP_WEIGHT:
			g_value_set_int (value, pango_font_description_get_weight (celltext->font));
			break;

		case PROP_STRETCH:
			g_value_set_enum (value, pango_font_description_get_stretch (celltext->font));
			break;

		case PROP_SIZE:
			g_value_set_int (value, pango_font_description_get_size (celltext->font));
			break;

		case PROP_SIZE_POINTS:
			g_value_set_double (value, ((double)pango_font_description_get_size (celltext->font)) / (double)PANGO_SCALE);
			break;

		case PROP_SCALE:
			g_value_set_double (value, celltext->font_scale);
			break;

		case PROP_EDITABLE:
			g_value_set_boolean (value, celltext->editable);
			break;

		case PROP_STRIKETHROUGH:
			g_value_set_boolean (value, celltext->strikethrough);
			break;

		case PROP_UNDERLINE:
			g_value_set_enum (value, celltext->underline_style);
			break;

		case PROP_RISE:
			g_value_set_int (value, celltext->rise);
			break;

		case PROP_LANGUAGE:
			g_value_set_static_string (value, pango_language_to_string (priv->language));
			break;

		case PROP_ELLIPSIZE:
			g_value_set_enum (value, priv->ellipsize);
			break;

		case PROP_WRAP_MODE:
			g_value_set_enum (value, priv->wrap_mode);
			break;

		case PROP_WRAP_WIDTH:
			g_value_set_int (value, priv->wrap_width);
			break;

		case PROP_ALIGN:
			g_value_set_enum (value, priv->align);
			break;

		case PROP_BACKGROUND_SET:
			g_value_set_boolean (value, celltext->background_set);
			break;

		case PROP_FOREGROUND_SET:
			g_value_set_boolean (value, celltext->foreground_set);
			break;

		case PROP_FAMILY_SET:
		case PROP_STYLE_SET:
		case PROP_VARIANT_SET:
		case PROP_WEIGHT_SET:
		case PROP_STRETCH_SET:
		case PROP_SIZE_SET:
			{
				PangoFontMask mask = get_property_font_set_mask (param_id);
				g_value_set_boolean (value, (pango_font_description_get_set_fields (celltext->font) & mask) != 0);

				break;
			}

		case PROP_SCALE_SET:
			g_value_set_boolean (value, celltext->scale_set);
			break;

		case PROP_EDITABLE_SET:
			g_value_set_boolean (value, celltext->editable_set);
			break;

		case PROP_STRIKETHROUGH_SET:
			g_value_set_boolean (value, celltext->strikethrough_set);
			break;

		case PROP_UNDERLINE_SET:
			g_value_set_boolean (value, celltext->underline_set);
			break;

		case  PROP_RISE_SET:
			g_value_set_boolean (value, celltext->rise_set);
			break;

		case PROP_LANGUAGE_SET:
			g_value_set_boolean (value, priv->language_set);
			break;

		case PROP_ELLIPSIZE_SET:
			g_value_set_boolean (value, priv->ellipsize_set);
			break;

		case PROP_ALIGN_SET:
			g_value_set_boolean (value, priv->align_set);
			break;

		case PROP_WIDTH_CHARS:
			g_value_set_int (value, priv->width_chars);
			break;

		case PROP_SHOW_SELECTION_STATE:
			g_value_set_boolean (value, mycelltext->show_selection_state);
			break;

		case PROP_BACKGROUND:
		case PROP_FOREGROUND:
		case PROP_MARKUP:
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void set_bg_color(GtkCellRendererText *celltext,
                         GdkColor            *color)
{
	if (color)
	{
		if (!celltext->background_set)
		{
			celltext->background_set = TRUE;
			g_object_notify (G_OBJECT (celltext), "background-set");
		}

		celltext->background.red = color->red;
		celltext->background.green = color->green;
		celltext->background.blue = color->blue;
	}
	else
	{
		if (celltext->background_set)
		{
			celltext->background_set = FALSE;
			g_object_notify (G_OBJECT (celltext), "background-set");
		}
	}
}

static void set_fg_color(GtkCellRendererText *celltext,
                         GdkColor            *color)
{
	if (color)
	{
		if (!celltext->foreground_set)
		{
			celltext->foreground_set = TRUE;
			g_object_notify (G_OBJECT (celltext), "foreground-set");
		}

		celltext->foreground.red = color->red;
		celltext->foreground.green = color->green;
		celltext->foreground.blue = color->blue;
	}
	else
	{
		if (celltext->foreground_set)
		{
			celltext->foreground_set = FALSE;
			g_object_notify (G_OBJECT (celltext), "foreground-set");
		}
	}
}

static PangoFontMask set_font_desc_fields(PangoFontDescription *desc,
                                          PangoFontMask        to_set)
{
	PangoFontMask changed_mask = 0;

	if (to_set & PANGO_FONT_MASK_FAMILY)
	{
		const char *family = pango_font_description_get_family (desc);
		if (!family)
		{
			family = "sans";
			changed_mask |= PANGO_FONT_MASK_FAMILY;
		}

		pango_font_description_set_family (desc, family);
	}
	if (to_set & PANGO_FONT_MASK_STYLE)
		pango_font_description_set_style (desc, pango_font_description_get_style (desc));
	if (to_set & PANGO_FONT_MASK_VARIANT)
		pango_font_description_set_variant (desc, pango_font_description_get_variant (desc));
	if (to_set & PANGO_FONT_MASK_WEIGHT)
		pango_font_description_set_weight (desc, pango_font_description_get_weight (desc));
	if (to_set & PANGO_FONT_MASK_STRETCH)
		pango_font_description_set_stretch (desc, pango_font_description_get_stretch (desc));
	if (to_set & PANGO_FONT_MASK_SIZE)
	{
		gint size = pango_font_description_get_size (desc);
		if (size <= 0)
		{
			size = 10 * PANGO_SCALE;
			changed_mask |= PANGO_FONT_MASK_SIZE;
		}

		pango_font_description_set_size (desc, size);
	}

	return changed_mask;
}

static void notify_set_changed(GObject       *object,
                               PangoFontMask changed_mask)
{
	if (changed_mask & PANGO_FONT_MASK_FAMILY)
		g_object_notify (object, "family-set");
	if (changed_mask & PANGO_FONT_MASK_STYLE)
		g_object_notify (object, "style-set");
	if (changed_mask & PANGO_FONT_MASK_VARIANT)
		g_object_notify (object, "variant-set");
	if (changed_mask & PANGO_FONT_MASK_WEIGHT)
		g_object_notify (object, "weight-set");
	if (changed_mask & PANGO_FONT_MASK_STRETCH)
		g_object_notify (object, "stretch-set");
	if (changed_mask & PANGO_FONT_MASK_SIZE)
		g_object_notify (object, "size-set");
}

static void set_font_description(GtkCellRendererText  *celltext,
                                 PangoFontDescription *font_desc)
{
	GObject *object = G_OBJECT (celltext);
	PangoFontDescription *new_font_desc;
	PangoFontMask old_mask, new_mask, changed_mask, set_changed_mask;

	if (font_desc)
		new_font_desc = pango_font_description_copy (font_desc);
	else
		new_font_desc = pango_font_description_new ();

	old_mask = pango_font_description_get_set_fields (celltext->font);
	new_mask = pango_font_description_get_set_fields (new_font_desc);

	changed_mask = old_mask | new_mask;
	set_changed_mask = old_mask ^ new_mask;

	pango_font_description_free (celltext->font);
	celltext->font = new_font_desc;

	g_object_freeze_notify (object);

	g_object_notify (object, "font-desc");
	g_object_notify (object, "font");

	if (changed_mask & PANGO_FONT_MASK_FAMILY)
		g_object_notify (object, "family");
	if (changed_mask & PANGO_FONT_MASK_STYLE)
		g_object_notify (object, "style");
	if (changed_mask & PANGO_FONT_MASK_VARIANT)
		g_object_notify (object, "variant");
	if (changed_mask & PANGO_FONT_MASK_WEIGHT)
		g_object_notify (object, "weight");
	if (changed_mask & PANGO_FONT_MASK_STRETCH)
		g_object_notify (object, "stretch");
	if (changed_mask & PANGO_FONT_MASK_SIZE)
	{
		g_object_notify (object, "size");
		g_object_notify (object, "size-points");
	}

	notify_set_changed (object, set_changed_mask);

	g_object_thaw_notify (object);
}

static void notify_fields_changed(GObject       *object,
                                  PangoFontMask  changed_mask)
{
	if (changed_mask & PANGO_FONT_MASK_FAMILY)
		g_object_notify (object, "family");
	if (changed_mask & PANGO_FONT_MASK_STYLE)
		g_object_notify (object, "style");
	if (changed_mask & PANGO_FONT_MASK_VARIANT)
		g_object_notify (object, "variant");
	if (changed_mask & PANGO_FONT_MASK_WEIGHT)
		g_object_notify (object, "weight");
	if (changed_mask & PANGO_FONT_MASK_STRETCH)
		g_object_notify (object, "stretch");
	if (changed_mask & PANGO_FONT_MASK_SIZE)
		g_object_notify (object, "size");
}

static void cell_text_set_property(GObject      *object,
                                   guint        param_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT(object);
	CellText *mycelltext = (CellText *) object;
	CellTextPrivate *priv;

	priv = CELL_TEXT_GET_PRIVATE(mycelltext);

	switch (param_id)
	{
		case PROP_TEXT:
			g_free (celltext->text);

			if (priv->markup_set)
			{
				if (celltext->extra_attrs)
					pango_attr_list_unref (celltext->extra_attrs);
				celltext->extra_attrs = NULL;
				priv->markup_set = FALSE;
			}

			celltext->text = g_value_dup_string (value);
			g_object_notify (object, "text");
			break;

		case PROP_ATTRIBUTES:
			if (celltext->extra_attrs)
				pango_attr_list_unref (celltext->extra_attrs);

			celltext->extra_attrs = g_value_get_boxed (value);
			if (celltext->extra_attrs)
				pango_attr_list_ref (celltext->extra_attrs);
			break;

		case PROP_MARKUP:
			{
				const gchar *str;
				gchar *text = NULL;
				GError *error = NULL;
				PangoAttrList *attrs = NULL;

				str = g_value_get_string (value);
				if (str && !pango_parse_markup (str,
					-1,
					0,
					&attrs,
					&text,
					NULL,
					&error))
				{
					g_warning ("Failed to set text from markup due to error parsing markup: %s",
								error->message);
					g_error_free (error);
					return;
				}

				g_free (celltext->text);

				if (celltext->extra_attrs)
					pango_attr_list_unref (celltext->extra_attrs);

				celltext->text = text;
				celltext->extra_attrs = attrs;
				priv->markup_set = TRUE;
			}
			break;

		case PROP_SINGLE_PARAGRAPH_MODE:
			priv->single_paragraph = g_value_get_boolean (value);
			break;

		case PROP_BACKGROUND:
			{
				GdkColor color;

				if (!g_value_get_string (value))
					set_bg_color (celltext, NULL);       /* reset to background_set to FALSE */
				else if (gdk_color_parse (g_value_get_string (value), &color))
					set_bg_color (celltext, &color);
				else
					g_warning ("Don't know color `%s'", g_value_get_string (value));

				g_object_notify (object, "background-gdk");
			}
			break;

		case PROP_FOREGROUND:
			{
				GdkColor color;

				if (!g_value_get_string (value))
					set_fg_color (celltext, NULL);       /* reset to foreground_set to FALSE */
				else if (gdk_color_parse (g_value_get_string (value), &color))
					set_fg_color (celltext, &color);
				else
					g_warning ("Don't know color `%s'", g_value_get_string (value));

				g_object_notify (object, "foreground-gdk");
			}
			break;

		case PROP_BACKGROUND_GDK:
			/* This notifies the GObject itself. */
			set_bg_color (celltext, g_value_get_boxed (value));
			break;

		case PROP_FOREGROUND_GDK:
			/* This notifies the GObject itself. */
			set_fg_color (celltext, g_value_get_boxed (value));
			break;

		case PROP_FONT:
			{
				PangoFontDescription *font_desc = NULL;
				const gchar *name;

				name = g_value_get_string (value);

				if (name)
					font_desc = pango_font_description_from_string (name);

				set_font_description (celltext, font_desc);

				pango_font_description_free (font_desc);

				if (celltext->fixed_height_rows != -1)
					celltext->calc_fixed_height = TRUE;
			}
			break;

		case PROP_FONT_DESC:
			set_font_description (celltext, g_value_get_boxed (value));

			if (celltext->fixed_height_rows != -1)
				celltext->calc_fixed_height = TRUE;
			break;

		case PROP_FAMILY:
		case PROP_STYLE:
		case PROP_VARIANT:
		case PROP_WEIGHT:
		case PROP_STRETCH:
		case PROP_SIZE:
		case PROP_SIZE_POINTS:
			{
				PangoFontMask old_set_mask = pango_font_description_get_set_fields (celltext->font);

				switch (param_id)
				{
					case PROP_FAMILY:
						pango_font_description_set_family (celltext->font,
							g_value_get_string (value));
						break;
					case PROP_STYLE:
						pango_font_description_set_style (celltext->font,
							g_value_get_enum (value));
						break;
					case PROP_VARIANT:
						pango_font_description_set_variant (celltext->font,
							g_value_get_enum (value));
						break;
					case PROP_WEIGHT:
						pango_font_description_set_weight (celltext->font,
							g_value_get_int (value));
						break;
					case PROP_STRETCH:
						pango_font_description_set_stretch (celltext->font,
							g_value_get_enum (value));
						break;
					case PROP_SIZE:
						pango_font_description_set_size (celltext->font,
							g_value_get_int (value));
						g_object_notify (object, "size-points");
						break;
					case PROP_SIZE_POINTS:
						pango_font_description_set_size (celltext->font,
							g_value_get_double (value) * PANGO_SCALE);
						g_object_notify (object, "size");
						break;
				}

				if (celltext->fixed_height_rows != -1)
					celltext->calc_fixed_height = TRUE;

				notify_set_changed (object, old_set_mask & pango_font_description_get_set_fields (celltext->font));
				g_object_notify (object, "font-desc");
				g_object_notify (object, "font");

				break;
			}

		case PROP_SCALE:
			celltext->font_scale = g_value_get_double (value);
			celltext->scale_set = TRUE;
			if (celltext->fixed_height_rows != -1)
				celltext->calc_fixed_height = TRUE;
			g_object_notify (object, "scale-set");
			break;

		case PROP_EDITABLE:
			celltext->editable = g_value_get_boolean (value);
			celltext->editable_set = TRUE;
			if (celltext->editable)
				GTK_CELL_RENDERER (celltext)->mode = GTK_CELL_RENDERER_MODE_EDITABLE;
			else
				GTK_CELL_RENDERER (celltext)->mode = GTK_CELL_RENDERER_MODE_INERT;
			g_object_notify (object, "editable-set");
			break;

		case PROP_STRIKETHROUGH:
			celltext->strikethrough = g_value_get_boolean (value);
			celltext->strikethrough_set = TRUE;
			g_object_notify (object, "strikethrough-set");
			break;

		case PROP_UNDERLINE:
			celltext->underline_style = g_value_get_enum (value);
			celltext->underline_set = TRUE;
			g_object_notify (object, "underline-set");
			break;

		case PROP_RISE:
			celltext->rise = g_value_get_int (value);
			celltext->rise_set = TRUE;
			g_object_notify (object, "rise-set");
			if (celltext->fixed_height_rows != -1)
				celltext->calc_fixed_height = TRUE;
			break;

		case PROP_LANGUAGE:
			priv->language_set = TRUE;
			if (priv->language)
				g_object_unref (priv->language);
			priv->language = pango_language_from_string (g_value_get_string (value));
			g_object_notify (object, "language-set");
			break;

		case PROP_ELLIPSIZE:
			priv->ellipsize = g_value_get_enum (value);
			priv->ellipsize_set = TRUE;
			g_object_notify (object, "ellipsize-set");
			break;

		case PROP_WRAP_MODE:
			priv->wrap_mode = g_value_get_enum (value);
			break;

		case PROP_WRAP_WIDTH:
			priv->wrap_width = g_value_get_int (value);
			break;

		case PROP_WIDTH_CHARS:
			priv->width_chars = g_value_get_int (value);
			break;

		case PROP_ALIGN:
			priv->align = g_value_get_enum (value);
			priv->align_set = TRUE;
			g_object_notify (object, "align-set");
			break;

		case PROP_BACKGROUND_SET:
			celltext->background_set = g_value_get_boolean (value);
			break;

		case PROP_FOREGROUND_SET:
			celltext->foreground_set = g_value_get_boolean (value);
			break;

		case PROP_FAMILY_SET:
		case PROP_STYLE_SET:
		case PROP_VARIANT_SET:
		case PROP_WEIGHT_SET:
		case PROP_STRETCH_SET:
		case PROP_SIZE_SET:
			if (!g_value_get_boolean (value))
			{
				pango_font_description_unset_fields (celltext->font,
					get_property_font_set_mask (param_id));
			}
			else
			{
				PangoFontMask changed_mask;

				changed_mask = set_font_desc_fields (celltext->font,
					get_property_font_set_mask (param_id));
				notify_fields_changed (G_OBJECT (celltext), changed_mask);
			}
			break;

		case PROP_SCALE_SET:
			celltext->scale_set = g_value_get_boolean (value);
			break;

		case PROP_EDITABLE_SET:
			celltext->editable_set = g_value_get_boolean (value);
			break;

		case PROP_STRIKETHROUGH_SET:
			celltext->strikethrough_set = g_value_get_boolean (value);
			break;

		case PROP_UNDERLINE_SET:
			celltext->underline_set = g_value_get_boolean (value);
			break;

		case PROP_RISE_SET:
			celltext->rise_set = g_value_get_boolean (value);
			break;

		case PROP_LANGUAGE_SET:
			priv->language_set = g_value_get_boolean (value);
			break;

		case PROP_ELLIPSIZE_SET:
			priv->ellipsize_set = g_value_get_boolean (value);
			break;

		case PROP_ALIGN_SET:
			priv->align_set = g_value_get_boolean (value);
			break;

		case PROP_SHOW_SELECTION_STATE:
			mycelltext->show_selection_state = g_value_get_boolean(value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object,
							param_id, pspec);
			break;
	}
}


static void
cell_text_render (GtkCellRenderer      *cell,
                  GdkDrawable          *window,
                  GtkWidget            *widget,
                  GdkRectangle         *background_area,
                  GdkRectangle         *cell_area,
                  GdkRectangle         *expose_area,
                  GtkCellRendererState flags)

{
	GtkCellRendererText *gtkcelltext = (GtkCellRendererText *) cell;
	CellText *mycelltext = (CellText *) cell;
	PangoLayout *layout;
	GtkStateType state;
	gint x_offset;
	gint y_offset;
	CellTextPrivate *priv;

	priv = CELL_TEXT_GET_PRIVATE(mycelltext);

	layout = get_layout (gtkcelltext, widget, TRUE, flags);
	get_size (cell, widget, cell_area, layout, &x_offset, &y_offset, NULL, NULL);

	if (!cell->sensitive)
	{
		state = GTK_STATE_INSENSITIVE;
	}
/* my edits */
	else if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED &&
			mycelltext->show_selection_state == TRUE)
	{
		if (gtk_widget_has_focus (widget))
			state = GTK_STATE_SELECTED;
		else
			state = GTK_STATE_ACTIVE;
	}
	else if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT &&
			gtk_widget_get_state (widget) == GTK_STATE_PRELIGHT)
	{
		state = GTK_STATE_PRELIGHT;
	}
	else
	{
		if (gtk_widget_get_state (widget) == GTK_STATE_INSENSITIVE)
			state = GTK_STATE_INSENSITIVE;
		else
			state = GTK_STATE_NORMAL;
	}

/* my edits */
	if ((flags & GTK_CELL_RENDERER_SELECTED) == 0 ||
		(mycelltext->show_selection_state == FALSE &&
		(flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED))
	{
		if (gtkcelltext->background_set)
		{

			cairo_t *cr = gdk_cairo_create (window);

			if (expose_area && mycelltext->show_selection_state == TRUE)
			{
				gdk_cairo_rectangle (cr, expose_area);
				cairo_clip (cr);
			}

			gdk_cairo_rectangle (cr, background_area);

			cairo_set_source_rgb (cr,
				gtkcelltext->background.red / 65535.,
				gtkcelltext->background.green / 65535.,
				gtkcelltext->background.blue / 65535.);

			cairo_fill (cr);

			cairo_destroy (cr);
		}
		else
		{
			/* For trees: even rows are base color, odd rows are a shade of
			 * the base color, the sort column is a shade of the original color
			 * for that row.
			 *
			 * For ROX-Filer, alternating colors between even and odd rows
			 * is not supported here.
			 */
			const gchar *detail = NULL;
			if ((flags & GTK_CELL_RENDERER_SORTED) == GTK_CELL_RENDERER_SORTED)
				detail = "cell_even_sorted";
			else
				detail = "cell_even";

			gtk_paint_flat_box(widget->style,
						window,
						state,
						GTK_SHADOW_NONE,
						background_area,
						widget,
						detail,
						background_area->x,
						background_area->y,
						background_area->width,
						background_area->height);
		}
	}

	if (priv->ellipsize_set && priv->ellipsize != PANGO_ELLIPSIZE_NONE)
		pango_layout_set_width (layout,
			(cell_area->width - x_offset - 2 * cell->xpad) * PANGO_SCALE);
	else if (priv->wrap_width == -1)
		pango_layout_set_width (layout, -1);

	gtk_paint_layout (widget->style,
				window,
				state,
				TRUE,
				expose_area,
				widget,
				"cellrenderertext",
				cell_area->x + x_offset + cell->xpad,
				cell_area->y + y_offset + cell->ypad,
				layout);

	g_object_unref (layout);
}

static void
get_size (GtkCellRenderer *cell,
          GtkWidget       *widget,
          GdkRectangle    *cell_area,
          PangoLayout     *layout,
          gint            *x_offset,
          gint            *y_offset,
          gint            *width,
          gint            *height)
{
	GtkCellRendererText *celltext = (GtkCellRendererText *) cell;
	PangoRectangle rect;
	CellTextPrivate *priv;

	priv = CELL_TEXT_GET_PRIVATE((CellText *) cell);

	if (celltext->calc_fixed_height)
	{
		PangoContext *context;
		PangoFontMetrics *metrics;
		PangoFontDescription *font_desc;
		gint row_height;

		font_desc = pango_font_description_copy_static (widget->style->font_desc);
		pango_font_description_merge_static (font_desc, celltext->font, TRUE);

		if (celltext->scale_set)
			pango_font_description_set_size (font_desc,
				 celltext->font_scale * pango_font_description_get_size (font_desc));

		context = gtk_widget_get_pango_context (widget);

		metrics = pango_context_get_metrics (context,
						font_desc,
						pango_context_get_language (context));
		row_height = (pango_font_metrics_get_ascent (metrics) +
			pango_font_metrics_get_descent (metrics));
		pango_font_metrics_unref (metrics);

		pango_font_description_free (font_desc);

		gtk_cell_renderer_set_fixed_size (cell,
					cell->width, 2*cell->ypad +
					celltext->fixed_height_rows * PANGO_PIXELS (row_height));

		if (height)
		{
			*height = cell->height;
			height = NULL;
		}
		celltext->calc_fixed_height = FALSE;
		if (width == NULL)
			return;
	}

	if (layout)
		g_object_ref (layout);
	else
		layout = get_layout (celltext, widget, FALSE, 0);

	pango_layout_get_pixel_extents (layout, NULL, &rect);

	if (height)
		*height = cell->ypad * 2 + rect.height;

	/* The minimum size for ellipsized labels is ~ 3 chars */
	if (width)
	{
		if (priv->ellipsize || priv->width_chars > 0)
		{
			PangoContext *context;
			PangoFontMetrics *metrics;
			gint char_width;

			context = pango_layout_get_context (layout);
			metrics = pango_context_get_metrics (context, widget->style->font_desc, pango_context_get_language (context));

			char_width = pango_font_metrics_get_approximate_char_width (metrics);
			pango_font_metrics_unref (metrics);

			*width = cell->xpad * 2 + (PANGO_PIXELS (char_width) * MAX (priv->width_chars, 3));
		}
		else
		{
			*width = cell->xpad * 2 + rect.x + rect.width;
		}
	}

	if (cell_area)
	{
		if (x_offset)
		{
		if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
			*x_offset = (1.0 - cell->xalign) * (cell_area->width - (rect.x + rect.width + (2 * cell->xpad)));
		else
			*x_offset = cell->xalign * (cell_area->width - (rect.x + rect.width + (2 * cell->xpad)));

		if ((priv->ellipsize_set && priv->ellipsize != PANGO_ELLIPSIZE_NONE) || priv->wrap_width != -1)
			*x_offset = MAX(*x_offset, 0);
		}
		if (y_offset)
		{
			*y_offset = cell->yalign * (cell_area->height - (rect.height + (2 * cell->ypad)));
			*y_offset = MAX (*y_offset, 0);
		}
	}
	else
	{
		if (x_offset) *x_offset = 0;
		if (y_offset) *y_offset = 0;
	}

	g_object_unref (layout);
}

static void
cell_text_get_size (GtkCellRenderer *cell,
                    GtkWidget       *widget,
                    GdkRectangle    *cell_area,
                    gint            *x_offset,
                    gint            *y_offset,
                    gint            *width,
                    gint            *height)
{
	get_size (cell, widget, cell_area, NULL,
		x_offset, y_offset, width, height);
}

static PangoLayout*
get_layout (GtkCellRendererText  *celltext,
            GtkWidget            *widget,
            gboolean             will_render,
            GtkCellRendererState flags)
{
	PangoAttrList *attr_list;
	PangoLayout *layout;
	PangoUnderline uline;
	CellTextPrivate *priv;
	CellText *mycelltext = (CellText *) celltext;

	priv = CELL_TEXT_GET_PRIVATE(mycelltext);

	layout = gtk_widget_create_pango_layout (widget, celltext->text);

	if (celltext->extra_attrs)
		attr_list = pango_attr_list_copy (celltext->extra_attrs);
	else
		attr_list = pango_attr_list_new ();

	pango_layout_set_single_paragraph_mode (layout, priv->single_paragraph);

	if (will_render)
	{
		/* Add options that affect appearance but not size */

		/* note that background doesn't go here, since it affects
		 * background_area not the PangoLayout area
		 */

		if (celltext->foreground_set &&
			((flags & GTK_CELL_RENDERER_SELECTED) == 0 ||
			mycelltext->show_selection_state == FALSE))
		{
			PangoColor color;

			color = celltext->foreground;

			add_attr (attr_list,
				pango_attr_foreground_new (color.red, color.green, color.blue));
		}

		if (celltext->strikethrough_set)
			add_attr (attr_list,
				pango_attr_strikethrough_new (celltext->strikethrough));
	}

	add_attr (attr_list, pango_attr_font_desc_new (celltext->font));

	if (celltext->scale_set && celltext->font_scale != 1.0)
		add_attr (attr_list, pango_attr_scale_new (celltext->font_scale));

	if (celltext->underline_set)
		uline = celltext->underline_style;
	else
		uline = PANGO_UNDERLINE_NONE;

	if (priv->language_set)
		add_attr (attr_list, pango_attr_language_new (priv->language));

	if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT)
	{
		switch (uline)
		{
			case PANGO_UNDERLINE_NONE:
				uline = PANGO_UNDERLINE_SINGLE;
				break;

			case PANGO_UNDERLINE_SINGLE:
				uline = PANGO_UNDERLINE_DOUBLE;
				break;

			default:
				break;
		}
	}

	if (uline != PANGO_UNDERLINE_NONE)
		add_attr (attr_list, pango_attr_underline_new (celltext->underline_style));

	if (celltext->rise_set)
		add_attr (attr_list, pango_attr_rise_new (celltext->rise));

	if (priv->ellipsize_set)
		pango_layout_set_ellipsize (layout, priv->ellipsize);
	else
		pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_NONE);

	if (priv->wrap_width != -1)
	{
		pango_layout_set_width (layout, priv->wrap_width * PANGO_SCALE);
		pango_layout_set_wrap (layout, priv->wrap_mode);
	}
	else
	{
		pango_layout_set_width (layout, -1);
		pango_layout_set_wrap (layout, PANGO_WRAP_CHAR);
	}

	if (priv->align_set)
		pango_layout_set_alignment (layout, priv->align);
	else
	{
		PangoAlignment align;

		if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
			align = PANGO_ALIGN_RIGHT;
		else
			align = PANGO_ALIGN_LEFT;

		pango_layout_set_alignment (layout, align);
	}

	pango_layout_set_attributes (layout, attr_list);

	pango_attr_list_unref (attr_list);

	return layout;
}

static void
add_attr (PangoAttrList  *attr_list,
          PangoAttribute *attr)
{
	attr->start_index = 0;
	attr->end_index = G_MAXINT;

	pango_attr_list_insert (attr_list, attr);
}
