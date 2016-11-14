/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <math.h>

#include "my/my-icollector.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-settings.h"

#include "ui/ofa-misc-audit-store.h"
#include "ui/ofa-misc-audit-treeview.h"
#include "ui/ofa-misc-audit-ui.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter           *getter;
	gchar                *settings_prefix;

	/* UI
	 */
	ofaMiscAuditStore    *audit_store;
	ofaMiscAuditTreeview *audit_tview;
	GtkWidget            *scale;

	/* runtime
	 */
	guint                 pages;
}
	ofaMiscAuditUIPrivate;

/* count of lines per page
 */
#define TICK                               1000

static const gchar  *st_resource_ui     = "/org/trychlos/openbook/ui/ofa-misc-audit-ui.ui";

static void iwindow_iface_init( myIWindowInterface *iface );
static void idialog_iface_init( myIDialogInterface *iface );
static void idialog_init( myIDialog *instance );
static void init_treeview( ofaMiscAuditUI *self );
static void init_scale( ofaMiscAuditUI *self );
static void setup_context( ofaMiscAuditUI *self );
static void scale_set_data( ofaMiscAuditUI *self );
static void scale_on_value_changed( GtkRange *range, ofaMiscAuditUI *self );

G_DEFINE_TYPE_EXTENDED( ofaMiscAuditUI, ofa_misc_audit_ui, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaMiscAuditUI )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
misc_audit_ui_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_finalize";
	ofaMiscAuditUIPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MISC_AUDIT_UI( instance ));

	/* free data members here */
	priv = ofa_misc_audit_ui_get_instance_private( OFA_MISC_AUDIT_UI( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_misc_audit_ui_parent_class )->finalize( instance );
}

static void
misc_audit_ui_dispose( GObject *instance )
{
	ofaMiscAuditUIPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MISC_AUDIT_UI( instance ));

	priv = ofa_misc_audit_ui_get_instance_private( OFA_MISC_AUDIT_UI( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_misc_audit_ui_parent_class )->dispose( instance );
}

static void
ofa_misc_audit_ui_init( ofaMiscAuditUI *self )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_init";
	ofaMiscAuditUIPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MISC_AUDIT_UI( self ));

	priv = ofa_misc_audit_ui_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_misc_audit_ui_class_init( ofaMiscAuditUIClass *klass )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = misc_audit_ui_dispose;
	G_OBJECT_CLASS( klass )->finalize = misc_audit_ui_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_misc_audit_ui_run:
 * @getter: a #ofaIGetter instance.
 *
 * Display the current content of the #myICollector interface.
 */
void
ofa_misc_audit_ui_run( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_run";
	ofaMiscAuditUI *self;
	ofaMiscAuditUIPrivate *priv;
	GtkApplicationWindow *parent;

	g_debug( "%s: getter=%p", thisfn, ( void * ) getter );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	self = g_object_new( OFA_TYPE_MISC_AUDIT_UI, NULL );

	parent = ofa_igetter_get_main_window( getter );
	my_iwindow_set_parent( MY_IWINDOW( self ), GTK_WINDOW( parent ));
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_misc_audit_ui_get_instance_private( self );

	priv->getter = getter;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_misc_audit_ui_idialog_init";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	init_treeview( OFA_MISC_AUDIT_UI( instance ));
	init_scale( OFA_MISC_AUDIT_UI( instance ));

	setup_context( OFA_MISC_AUDIT_UI( instance ));

	scale_set_data( OFA_MISC_AUDIT_UI( instance ));
}

static void
init_treeview( ofaMiscAuditUI *self )
{
	ofaMiscAuditUIPrivate *priv;
	GtkWidget *parent;

	priv = ofa_misc_audit_ui_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "audit-treeview" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->audit_tview = ofa_misc_audit_treeview_new();
	ofa_misc_audit_treeview_set_settings_key( priv->audit_tview, priv->settings_prefix );
	ofa_misc_audit_treeview_setup_columns( priv->audit_tview );
	priv->audit_store = ofa_misc_audit_treeview_setup_store( priv->audit_tview, ofa_igetter_get_hub( priv->getter ));

	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->audit_tview ));
}

/*
 * page is numbered from 1 to n
 */
static void
init_scale( ofaMiscAuditUI *self )
{
	ofaMiscAuditUIPrivate *priv;
	GtkWidget *scale, *label;

	priv = ofa_misc_audit_ui_get_instance_private( self );

	scale = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "page-scale" );
	g_return_if_fail( scale && GTK_IS_SCALE( scale ));
	priv->scale = scale;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "scale-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), scale );

	g_signal_connect( scale, "value-changed", G_CALLBACK( scale_on_value_changed ), self );
}

static void
setup_context( ofaMiscAuditUI *self )
{
	ofaMiscAuditUIPrivate *priv;
	GMenu *menu;

	priv = ofa_misc_audit_ui_get_instance_private( self );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->audit_tview ));
	ofa_icontext_append_submenu(
			OFA_ICONTEXT( priv->audit_tview ), OFA_IACTIONABLE( priv->audit_tview ),
			OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM, menu );
}

static void
scale_set_data( ofaMiscAuditUI *self )
{
	ofaMiscAuditUIPrivate *priv;
	GtkAdjustment *adjustment;

	priv = ofa_misc_audit_ui_get_instance_private( self );

	priv->pages = ofa_misc_audit_store_get_pages_count( priv->audit_store, TICK );
	ofa_misc_audit_store_load_lines( priv->audit_store, 1 );

	adjustment = gtk_adjustment_new( 1, 1, priv->pages, 1, 10, 0 );
	gtk_range_set_adjustment( GTK_RANGE( priv->scale ), adjustment );

	scale_on_value_changed( GTK_RANGE( priv->scale ), self );
}

static void
scale_on_value_changed( GtkRange *range, ofaMiscAuditUI *self )
{
	ofaMiscAuditUIPrivate *priv;
	gdouble pageno;

	priv = ofa_misc_audit_ui_get_instance_private( self );

	pageno = gtk_range_get_value( range );
	ofa_misc_audit_store_load_lines( priv->audit_store, ( guint ) pageno );
}
