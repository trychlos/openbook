/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-utils.h"
#include "api/ofo-bat.h"

#include "core/my-window-prot.h"

#include "ui/ofa-bat-common.h"
#include "ui/ofa-bat-select.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaBatSelectPrivate {

	/* runtime
	 */
	ofaBatCommon *bat_common;

	/* returned value
	 */
	gint          bat_id;
};

static const gchar *st_ui_xml = PKGUIDIR "/ofa-bat-select.ui";
static const gchar *st_ui_id  = "BatSelectDlg";

G_DEFINE_TYPE( ofaBatSelect, ofa_bat_select, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_row_activated( const ofoBat *bat, ofaBatSelect *self );
static void      check_for_enable_dlg( ofaBatSelect *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaBatSelect *self );

static void
bat_select_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_select_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_SELECT( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_select_parent_class )->finalize( instance );
}

static void
bat_select_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_BAT_SELECT( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_select_parent_class )->dispose( instance );
}

static void
ofa_bat_select_init( ofaBatSelect *self )
{
	static const gchar *thisfn = "ofa_bat_select_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BAT_SELECT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BAT_SELECT, ofaBatSelectPrivate );
}

static void
ofa_bat_select_class_init( ofaBatSelectClass *klass )
{
	static const gchar *thisfn = "ofa_bat_select_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_select_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_select_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaBatSelectPrivate ));
}

/**
 * ofa_bat_select_run:
 *
 * Returns the selected Bank Account Transaction list (BAT) identifier,
 * or -1.
 */
gint
ofa_bat_select_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_bat_select_run";
	ofaBatSelect *self;
	gint bat_id;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
			OFA_TYPE_BAT_SELECT,
			MY_PROP_MAIN_WINDOW, main_window,
			MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
			MY_PROP_WINDOW_XML,  st_ui_xml,
			MY_PROP_WINDOW_NAME, st_ui_id,
			NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	bat_id = self->priv->bat_id;

	g_object_unref( self );

	return( bat_id );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaBatSelectPrivate *priv;
	ofsBatCommonParms parms;
	GtkWidget *container;

	priv = OFA_BAT_SELECT( dialog )->priv;

	container = my_utils_container_get_child_by_name(
						GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog ))),
						"containing-frame" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	parms.container = GTK_CONTAINER( container );
	parms.dossier = MY_WINDOW( dialog )->protected->dossier;
	parms.with_tree_view = TRUE;
	parms.editable = FALSE;
	parms.pfnSelection = NULL;
	parms.pfnActivation = ( ofaBatCommonCb ) on_row_activated;
	parms.user_data = dialog;

	priv->bat_common = ofa_bat_common_init_dialog( &parms );

	check_for_enable_dlg( OFA_BAT_SELECT( dialog ));
}

/*
 * ofoBatCommon cb
 */
static void
on_row_activated( const ofoBat *bat, ofaBatSelect *self )
{
	gtk_dialog_response(
			GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
			GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaBatSelect *self )
{
	const ofoBat *bat;
	GtkWidget *btn;

	bat = ofa_bat_common_get_selection( self->priv->bat_common );

	btn = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );

	gtk_widget_set_sensitive( btn, bat && OFO_IS_BAT( bat ));
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_BAT_SELECT( dialog )));
}

static gboolean
do_update( ofaBatSelect *self )
{
	const ofoBat *bat;

	bat = ofa_bat_common_get_selection( self->priv->bat_common );
	if( bat ){
		self->priv->bat_id = ofo_bat_get_id( bat );
	}

	return( TRUE );
}
