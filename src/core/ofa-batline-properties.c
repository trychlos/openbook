/*
 * Open Firm Accounting
 * A double-batline accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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
#include <stdlib.h>

#include "my/my-date-editable.h"
#include "my/my-double-editable.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-counter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"
#include "api/ofo-dossier.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"

#include "core/ofa-batline-properties.h"
#include "core/ofa-bat-properties.h"

/* private instance data
 */
typedef struct {
	gboolean        dispose_has_run;

	/* initialization
	 */
	ofaIGetter     *getter;
	GtkWindow      *parent;
	ofoBatLine     *batline;
	gboolean        editable;

	/* runtime
	 */
	gboolean        is_writable;		/* this are needed by macros, but not used in the code */
	gboolean        is_new;				/* this are needed by macros, but not used in the code */

	/* UI
	 */
	GtkWidget      *bat_line_id_entry;
	GtkWidget      *bat_id_entry;
	GtkWidget      *bat_btn;
	GtkWidget      *dope_entry;
	GtkWidget      *dope_label;
	GtkWidget      *deffect_entry;
	GtkWidget      *deffect_label;
	GtkWidget      *label_entry;
	GtkWidget      *ref_entry;
	GtkWidget      *amount_entry;
	GtkWidget      *sens_entry;
	GtkWidget      *currency_entry;
	GtkWidget      *msg_label;
	GtkWidget      *ok_btn;
}
	ofaBatlinePropertiesPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-batline-properties.ui";

static void iwindow_iface_init( myIWindowInterface *iface );
static void iwindow_init( myIWindow *instance );
static void idialog_iface_init( myIDialogInterface *iface );
static void idialog_init( myIDialog *instance );
static void setup_ui_properties( ofaBatlineProperties *self );
static void setup_data( ofaBatlineProperties *self );
static void check_for_enable_dlg( ofaBatlineProperties *self );
static void on_bat_clicked( GtkButton *button, ofaBatlineProperties *self );

G_DEFINE_TYPE_EXTENDED( ofaBatlineProperties, ofa_batline_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaBatlineProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
batline_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_batline_properties_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BATLINE_PROPERTIES( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_batline_properties_parent_class )->finalize( instance );
}

static void
batline_properties_dispose( GObject *instance )
{
	ofaBatlinePropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BATLINE_PROPERTIES( instance ));

	priv = ofa_batline_properties_get_instance_private( OFA_BATLINE_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_batline_properties_parent_class )->dispose( instance );
}

static void
ofa_batline_properties_init( ofaBatlineProperties *self )
{
	static const gchar *thisfn = "ofa_batline_properties_init";
	ofaBatlinePropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BATLINE_PROPERTIES( self ));

	priv = ofa_batline_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_batline_properties_class_init( ofaBatlinePropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_batline_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = batline_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = batline_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_batline_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @batline: the #ofoBatLine to be displayed.
 *
 * Display or update the properties of an batline.
 *
 * Note that not all properties are updatable.
 */
void
ofa_batline_properties_run( ofaIGetter *getter, GtkWindow *parent, ofoBatLine *batline )
{
	static const gchar *thisfn = "ofa_batline_properties_run";
	ofaBatlineProperties *self;
	ofaBatlinePropertiesPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, batline=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) batline );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_BATLINE_PROPERTIES, NULL );

	priv = ofa_batline_properties_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->batline = batline;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_batline_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_batline_properties_iwindow_init";
	ofaBatlinePropertiesPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_batline_properties_get_instance_private( OFA_BATLINE_PROPERTIES( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_batline_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * This dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * batline.
 *
 * As of v0.62, update of an #ofaBatLine is not handled here.
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_batline_properties_idialog_init";
	ofaBatlinePropertiesPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_batline_properties_get_instance_private( OFA_BATLINE_PROPERTIES( instance ));

	/* v 0.62 */
	/*priv->is_writable = ofa_hub_is_writable_dossier( priv->hub );*/
	priv->is_writable = FALSE;

	setup_ui_properties( OFA_BATLINE_PROPERTIES( instance ));
	setup_data( OFA_BATLINE_PROPERTIES( instance ));

	//my_utils_container_updstamp_init( instance, batline );
	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_writable );
	gtk_widget_set_sensitive( priv->bat_btn, TRUE );

	/* always closeable for now */
	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	gtk_widget_set_sensitive( priv->ok_btn, TRUE );

	check_for_enable_dlg( OFA_BATLINE_PROPERTIES( instance ));
}

static void
setup_ui_properties( ofaBatlineProperties *self )
{
	ofaBatlinePropertiesPrivate *priv;
	GtkWidget *entry, *label, *btn;

	priv = ofa_batline_properties_get_instance_private( self );

	/* batline id */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-batline-id" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->bat_line_id_entry = entry;

	/* bat id */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-bat-id" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->bat_id_entry = entry;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-bat-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect( btn, "clicked", G_CALLBACK( on_bat_clicked ), self );
	priv->bat_btn = btn;

	/* operation date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dope-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-dope-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_get_check_format( priv->getter ));
	priv->dope_entry = entry;

	/* effect date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-deffect-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-deffect-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_get_check_format( priv->getter ));
	priv->deffect_entry = entry;

	/* label */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->label_entry = entry;

	/* reference */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ref-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->ref_entry = entry;

	/* debit/credit amount and sens */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-amount-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	my_double_editable_init_ex( GTK_EDITABLE( entry ),
			g_utf8_get_char( ofa_prefs_amount_get_thousand_sep( priv->getter )),
			g_utf8_get_char( ofa_prefs_amount_get_decimal_sep( priv->getter )),
			ofa_prefs_amount_get_accept_dot( priv->getter ),
			ofa_prefs_amount_get_accept_comma( priv->getter ),
			HUB_DEFAULT_DECIMALS_AMOUNT );
	priv->amount_entry = entry;

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-sens-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->sens_entry = entry;

	/* currency */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-currency-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->currency_entry = entry;
}

static void
setup_data( ofaBatlineProperties *self )
{
	ofaBatlinePropertiesPrivate *priv;
	ofxCounter counter;
	gchar *str;
	const GDate *cdate;
	const gchar *cstr;
	ofxAmount amount;

	priv = ofa_batline_properties_get_instance_private( self );

	/* batline id */
	counter = ofo_bat_line_get_line_id( priv->batline );
	str = g_strdup_printf( "%lu", counter );
	gtk_entry_set_text( GTK_ENTRY( priv->bat_line_id_entry ), str );
	g_free( str );

	/* bat id */
	counter = ofo_bat_line_get_bat_id( priv->batline );
	str = g_strdup_printf( "%lu", counter );
	gtk_entry_set_text( GTK_ENTRY( priv->bat_id_entry ), str );
	g_free( str );

	/* operation date */
	cdate = ofo_bat_line_get_dope( priv->batline );
	if( my_date_is_valid( cdate )){
		my_date_editable_set_date( GTK_EDITABLE( priv->dope_entry ), cdate );
	}

	/* effect date */
	cdate = ofo_bat_line_get_deffect( priv->batline );
	if( my_date_is_valid( cdate )){
		my_date_editable_set_date( GTK_EDITABLE( priv->deffect_entry ), cdate );
	}

	/* label */
	cstr = ofo_bat_line_get_label( priv->batline );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->label_entry ), cstr );
	}

	/* reference */
	cstr = ofo_bat_line_get_ref( priv->batline );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->ref_entry ), cstr );
	}

	/* amount / sens */
	amount = ofo_bat_line_get_amount( priv->batline );
	if( amount < 0 ){
		my_double_editable_set_amount( GTK_EDITABLE( priv->amount_entry ), -amount );
		gtk_entry_set_text( GTK_ENTRY( priv->sens_entry ), _( "DB" ));
	} else {
		my_double_editable_set_amount( GTK_EDITABLE( priv->amount_entry ), amount );
		gtk_entry_set_text( GTK_ENTRY( priv->sens_entry ), _( "CR" ));
	}

	/* currency */
	cstr = ofo_bat_line_get_currency( priv->batline );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->currency_entry ), cstr );
	}
}

static void
check_for_enable_dlg( ofaBatlineProperties *self )
{
}

static void
on_bat_clicked( GtkButton *button, ofaBatlineProperties *self )
{
	ofaBatlinePropertiesPrivate *priv;
	ofxCounter bat_id;
	ofoBat *bat;

	priv = ofa_batline_properties_get_instance_private( self );

	bat_id = ofo_bat_line_get_bat_id( priv->batline );
	bat = ofo_bat_get_by_id( priv->getter, bat_id );
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	ofa_bat_properties_run( priv->getter, priv->parent, bat );
}
