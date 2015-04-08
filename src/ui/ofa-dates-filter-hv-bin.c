/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "ui/ofa-dates-filter-hv-bin.h"
#include "ui/ofa-idates-filter.h"

/* private instance data
 */
struct _ofaDatesFilterHVBinPrivate {
	gboolean dispose_has_run;
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-dates-filter-hv-bin.ui";

static void  idates_filter_iface_init( ofaIDatesFilterInterface *iface );
static guint idates_filter_get_interface_version( const ofaIDatesFilter *instance );

G_DEFINE_TYPE_EXTENDED( ofaDatesFilterHVBin, ofa_dates_filter_hv_bin, GTK_TYPE_BIN, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDATES_FILTER, idates_filter_iface_init ))

static void
dates_filter_hv_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dates_filter_hv_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DATES_FILTER_HV_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dates_filter_hv_bin_parent_class )->finalize( instance );
}

static void
dates_filter_hv_bin_dispose( GObject *instance )
{
	ofaDatesFilterHVBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DATES_FILTER_HV_BIN( instance ));

	priv = OFA_DATES_FILTER_HV_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dates_filter_hv_bin_parent_class )->dispose( instance );
}

static void
ofa_dates_filter_hv_bin_init( ofaDatesFilterHVBin *self )
{
	static const gchar *thisfn = "ofa_dates_filter_hv_bin_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DATES_FILTER_HV_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_DATES_FILTER_HV_BIN, ofaDatesFilterHVBinPrivate );
}

static void
ofa_dates_filter_hv_bin_class_init( ofaDatesFilterHVBinClass *klass )
{
	static const gchar *thisfn = "ofa_dates_filter_hv_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dates_filter_hv_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = dates_filter_hv_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaDatesFilterHVBinPrivate ));
}

/**
 * ofa_dates_filter_hv_bin_new:
 * @xml_name: the radical of the XML file which defines the UI.
 *
 * Returns: a newly allocated #ofaDatesFilterHVBin object.
 */
ofaDatesFilterHVBin *
ofa_dates_filter_hv_bin_new( void )
{
	ofaDatesFilterHVBin *bin;

	bin = g_object_new( OFA_TYPE_DATES_FILTER_HV_BIN, NULL );

	ofa_idates_filter_setup_bin( OFA_IDATES_FILTER( bin ), st_ui_xml );

	return( bin );
}

static void
idates_filter_iface_init( ofaIDatesFilterInterface *iface )
{
	static const gchar *thisfn = "ofa_dates_filter_idates_filter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idates_filter_get_interface_version;
}

static guint
idates_filter_get_interface_version( const ofaIDatesFilter *instance )
{
	return( 1 );
}

#if 0
/**
 * ofa_dates_filter_hv_bin_is_date_valid:
 * @who: whether we are addressing the "From" date or the "To" one.
 *
 * Returns: %TRUE if the addressed date is valid, ie:
 * - set and intrinsically valid,
 * - or unset and not mandatory.
 */
gboolean
ofa_dates_filter_hv_bin_is_date_valid( const ofaDatesFilterHVBin *bin, gint who )
{
	static const gchar *thisfn = "ofa_dates_filter_hv_bin_is_date_valid";
	ofaDatesFilterHVBinPrivate *priv;
	GtkWidget *entry;
	gboolean empty;
	gboolean valid;
	gboolean mandatory;
	GDate date;

	g_return_val_if_fail( bin && OFA_IS_DATES_FILTER_HV_BIN( bin ), FALSE );

	priv = bin->priv;
	valid = FALSE;

	if( !priv->dispose_has_run ){

		entry = NULL;
		switch( who ){
			case OFA_DATES_FILTER_FROM:
				entry = priv->from_entry;
				mandatory = priv->from_mandatory;
				break;
			case OFA_DATES_FILTER_TO:
				entry = priv->from_entry;
				mandatory = priv->to_mandatory;
				break;
			default:
				g_warning( "%s: invalid date identifier: %d", thisfn, who );
		}
		if( entry ){
			empty = my_editable_date_is_empty( GTK_EDITABLE( entry ));
			my_editable_date_get_date( GTK_EDITABLE( entry ), &valid );
			if( !valid && empty && !mandatory ){
				valid = TRUE;
			}
		}
	}

	return( valid );
}

/**
 * ofa_dates_filter_hv_bin_get_from_prompt:
 * @bin:
 *
 * Returns: a pointer to the GtkWidget which holds the 'From :' prompt.
 */
GtkWidget *
ofa_dates_filter_hv_bin_get_from_prompt( const ofaDatesFilterHVBin *bin )
{
	ofaDatesFilterHVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_DATES_FILTER_HV_BIN( bin ), NULL );

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		return( my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "from-prompt" ));
	}

	return( NULL );
}
#endif
