/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
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

#include "my/my-date.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"

#include "recurrent/ofa-recurrent-generate-over.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
	GDate       last;
	GDate       begin;

	/* UI
	 */

	/* runtime
	 */
	gboolean    generate_all;
}
	ofaRecurrentGenerateOverPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/recurrent/ofa-recurrent-generate-over.ui";

static void iwindow_iface_init( myIWindowInterface *iface );
static void iwindow_init( myIWindow *instance );
static void idialog_iface_init( myIDialogInterface *iface );
static void idialog_init( myIDialog *instance );
static void setup_ui( ofaRecurrentGenerateOver *self );
static void on_new_toggled( GtkToggleButton *button, ofaRecurrentGenerateOver *self );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentGenerateOver, ofa_recurrent_generate_over, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaRecurrentGenerateOver )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
recurrent_generate_over_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_generate_over_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_GENERATE_OVER( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_generate_over_parent_class )->finalize( instance );
}

static void
recurrent_generate_over_dispose( GObject *instance )
{
	ofaRecurrentGenerateOverPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECURRENT_GENERATE_OVER( instance ));

	priv = ofa_recurrent_generate_over_get_instance_private( OFA_RECURRENT_GENERATE_OVER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_generate_over_parent_class )->dispose( instance );
}

static void
ofa_recurrent_generate_over_init( ofaRecurrentGenerateOver *self )
{
	static const gchar *thisfn = "ofa_recurrent_generate_over_init";
	ofaRecurrentGenerateOverPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_GENERATE_OVER( self ));

	priv = ofa_recurrent_generate_over_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_recurrent_generate_over_class_init( ofaRecurrentGenerateOverClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_generate_over_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_generate_over_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_generate_over_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_recurrent_generate_over_run:
 * @getter: a #ofaIGetter instance.
 * @last: [allow-none]: the previous end date used for generation.
 * @begin: the required begin date.
 *
 * When the @begin is not later than @last, then ask the user for a
 * confirmation of what he wants:
 * - cancel
 * - regenerate all opes from @begin
 * - generate only new opes from @last.
 *
 * Returns: the answer as a #ofeRecurrentGenerateOver value.
 */
ofeRecurrentGenerateOver
ofa_recurrent_generate_over_run( ofaIGetter *getter, const GDate *last, const GDate *begin )
{
	static const gchar *thisfn = "ofa_recurrent_generate_over_run";
	ofaRecurrentGenerateOver *self;
	ofaRecurrentGenerateOverPrivate *priv;
	gboolean ok;
	ofeRecurrentGenerateOver answer;

	g_debug( "%s: getter=%p, last=%p, begin=%p",
			thisfn, ( void * ) getter, ( void * ) last, ( void * ) begin );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), OFA_RECURRENT_GENERATE_CANCEL );
	g_return_val_if_fail( begin && my_date_is_valid( begin ), OFA_RECURRENT_GENERATE_CANCEL );

	/* verify that we have something to choose here
	 * as a side effect, make sure @last is a valid date
	 */
	if( !my_date_is_valid( last ) || my_date_compare( begin, last ) > 0 ){
		return( OFA_RECURRENT_GENERATE_NEW );
	}

	self = g_object_new( OFA_TYPE_RECURRENT_GENERATE_OVER, NULL );

	priv = ofa_recurrent_generate_over_get_instance_private( self );

	priv->getter = getter;
	my_date_set_from_date( &priv->last, last );
	my_date_set_from_date( &priv->begin, begin );

	ok = ( my_idialog_run( MY_IDIALOG( self )) == GTK_RESPONSE_OK );

	if( ok ){
		answer = priv->generate_all ? OFA_RECURRENT_GENERATE_OVER : OFA_RECURRENT_GENERATE_NEW;
		my_iwindow_close( MY_IWINDOW( self ));

	} else {
		answer = OFA_RECURRENT_GENERATE_CANCEL;
	}

	return( answer );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_generate_over_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_recurrent_generate_over_iwindow_init";
	ofaRecurrentGenerateOverPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_recurrent_generate_over_get_instance_private( OFA_RECURRENT_GENERATE_OVER( instance ));

	my_iwindow_set_parent( instance, GTK_WINDOW( ofa_igetter_get_main_window( priv->getter )));
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_generate_over_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_recurrent_generate_over_idialog_init";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	setup_ui( OFA_RECURRENT_GENERATE_OVER( instance ));
}

static void
setup_ui( ofaRecurrentGenerateOver *self )
{
	ofaRecurrentGenerateOverPrivate *priv;
	GtkWidget *btn, *label;
	gchar *slast, *sbegin, *str;

	priv = ofa_recurrent_generate_over_get_instance_private( self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "new-btn" );
	g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( on_new_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), TRUE );
	on_new_toggled( GTK_TOGGLE_BUTTON( btn ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "msg-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	slast = my_date_to_str( &priv->last, ofa_prefs_date_get_display_format( priv->getter ));
	sbegin = my_date_to_str( &priv->begin, ofa_prefs_date_get_display_format( priv->getter ));
	str = g_strdup_printf(
			_( "You have requested to generate recurrent operations from %s, "
				"while the last recurrent generation occured on %s.\n"
				"If you confirm to regenerate all operations, then you may have to "
				"deal with duplicates of new waiting operations.\n"
				"You have been warned."), sbegin, slast );

	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );
}

static void
on_new_toggled( GtkToggleButton *button, ofaRecurrentGenerateOver *self )
{
	static const gchar *thisfn = "ofa_recurrent_generate_over_on_new_toggled";
	ofaRecurrentGenerateOverPrivate *priv;

	priv = ofa_recurrent_generate_over_get_instance_private( self );

	priv->generate_all = !gtk_toggle_button_get_active( button );

	g_debug( "%s: setting generate_all to %s", thisfn, priv->generate_all ? "True":"False" );
}
