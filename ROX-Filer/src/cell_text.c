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

enum {
	PROP_ZERO,
	PROP_SHOW_SELECTION_STATE,
};

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
	celltext->show_selection_state = TRUE;
}

static void cell_text_class_init(CellText *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS(class);

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
}

static void cell_text_set_property(GObject      *object,
                                   guint        param_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	CellText *celltext = (CellText *) object;

	switch (param_id)
	{
		case PROP_SHOW_SELECTION_STATE:
			celltext->show_selection_state = g_value_get_boolean(value);
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

	priv = CELL_TEXT_GET_PRIVATE(cell);

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

	priv = CELL_TEXT_GET_PRIVATE(cell);

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

	priv = CELL_TEXT_GET_PRIVATE(celltext);

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
