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

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-operation-group.h"
#include "api/ofo-concil.h"
#include "api/ofo-entry.h"

#include "core/ofa-entry-page-delconf.h"
#include "core/ofa-iconcil.h"

/* private instance data
 */
typedef struct {
	gboolean        dispose_has_run;

	/* initialization
	 */
	ofaIGetter     *getter;
	ofoEntry       *entry;
	GList         **entries;

	/* UI
	 */
	GtkWidget      *vope_btn;
	GtkWidget      *entry1_label;
	GtkWidget      *entry2_label;

	/* runtime
	 */
	gboolean        all_entries;
	ofxCounter      ope_number;
}
	ofaEntryPageDelconfPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-entry-page-delconf.ui";

static void iwindow_iface_init( myIWindowInterface *iface );
static void iwindow_init( myIWindow *instance );
static void idialog_iface_init( myIDialogInterface *iface );
static void idialog_init( myIDialog *instance );
static void setup_ui( ofaEntryPageDelconf *self );
static void setup_data( ofaEntryPageDelconf *self );
static void on_all_toggled( GtkToggleButton *button, ofaEntryPageDelconf *self );
//static void on_single_toggled( GtkToggleButton *button, ofaEntryPageDelconf *self );
static void on_vope_clicked( GtkButton *button, ofaEntryPageDelconf *self );
static void on_ok_clicked( GtkButton *button, ofaEntryPageDelconf *self );
static void manage_settle_concil( ofaEntryPageDelconf *self );

G_DEFINE_TYPE_EXTENDED( ofaEntryPageDelconf, ofa_entry_page_delconf, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaEntryPageDelconf )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
entry_page_delconf_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_entry_page_delconf_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ENTRY_PAGE_DELCONF( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_page_delconf_parent_class )->finalize( instance );
}

static void
entry_page_delconf_dispose( GObject *instance )
{
	ofaEntryPageDelconfPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ENTRY_PAGE_DELCONF( instance ));

	priv = ofa_entry_page_delconf_get_instance_private( OFA_ENTRY_PAGE_DELCONF( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_entry_page_delconf_parent_class )->dispose( instance );
}

static void
ofa_entry_page_delconf_init( ofaEntryPageDelconf *self )
{
	static const gchar *thisfn = "ofa_entry_page_delconf_init";
	ofaEntryPageDelconfPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ENTRY_PAGE_DELCONF( self ));

	priv = ofa_entry_page_delconf_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_entry_page_delconf_class_init( ofaEntryPageDelconfClass *klass )
{
	static const gchar *thisfn = "ofa_entry_page_delconf_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = entry_page_delconf_dispose;
	G_OBJECT_CLASS( klass )->finalize = entry_page_delconf_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_entry_page_delconf_run:
 * @getter: a #ofaIGetter instance.
 * @entry: the #ofoEntry to be deleted.
 * @entries: [out]: the list of entries to be deleted;
 *  the returned list should be g_list_free() by the caller.
 *
 * Ask the user for a confirmation of the deletion
 * - of all the entries for the same operation
 * - only for this single entry.
 *
 * Returns: %TRUE if the user has confirmed the deletion, and @entries
 * if filled up with the entries to delete, or %FALSE to cancel.
 */
gboolean
ofa_entry_page_delconf_run( ofaIGetter *getter, ofoEntry *entry, GList **entries )
{
	static const gchar *thisfn = "ofa_entry_page_delconf_run";
	ofaEntryPageDelconf *self;
	ofaEntryPageDelconfPrivate *priv;
	gboolean confirmed;

	g_debug( "%s: getter=%p, entry=%p, entries=%p",
			thisfn, ( void * ) getter, ( void * ) entry, ( void * ) entries );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );
	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( entries, FALSE );

	self = g_object_new( OFA_TYPE_ENTRY_PAGE_DELCONF, NULL );

	priv = ofa_entry_page_delconf_get_instance_private( self );

	priv->getter = getter;
	priv->entry = entry;
	priv->entries = entries;

	confirmed = ( my_idialog_run( MY_IDIALOG( self )) == GTK_RESPONSE_OK );

	my_iwindow_close( MY_IWINDOW( self ));

	g_debug( "%s: confirmed=%s", thisfn, confirmed ? "True":"False" );

	return( confirmed );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_entry_page_delconf_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_entry_page_delconf_iwindow_init";
	ofaEntryPageDelconfPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_entry_page_delconf_get_instance_private( OFA_ENTRY_PAGE_DELCONF( instance ));

	my_iwindow_set_parent( instance, GTK_WINDOW( ofa_igetter_get_main_window( priv->getter )));
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_entry_page_delconf_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_entry_page_delconf_idialog_init";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	setup_ui( OFA_ENTRY_PAGE_DELCONF( instance ));
	setup_data( OFA_ENTRY_PAGE_DELCONF( instance ));
}

static void
setup_ui( ofaEntryPageDelconf *self )
{
	ofaEntryPageDelconfPrivate *priv;
	GtkWidget *btn, *label;

	priv = ofa_entry_page_delconf_get_instance_private( self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "vope-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect( btn, "clicked", G_CALLBACK( on_vope_clicked ), self );
	priv->vope_btn = btn;

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect( btn, "clicked", G_CALLBACK( on_ok_clicked ), self );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "all-btn" );
	g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( on_all_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), TRUE );
	on_all_toggled( GTK_TOGGLE_BUTTON( btn ), self );

	/*
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "single-btn" );
	g_return_if_fail( btn && GTK_IS_RADIO_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( on_single_toggled ), self );
	*/

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "entry1-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->entry1_label = label;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "entry2-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->entry2_label = label;
}

static void
setup_data( ofaEntryPageDelconf *self )
{
	ofaEntryPageDelconfPrivate *priv;
	GString *gstr1, *gstr2;
	guint count;

	priv = ofa_entry_page_delconf_get_instance_private( self );

	priv->ope_number = ofo_entry_get_ope_number( priv->entry );
	gtk_widget_set_sensitive( priv->vope_btn, priv->ope_number > 0 );

	if( priv->ope_number > 0 ){
		*priv->entries = ofo_entry_get_by_ope_number( priv->getter, priv->ope_number );
	} else {
		*priv->entries = g_list_append( NULL, priv->entry );
	}

	gstr1 = g_string_new( "" );
	gstr2 = g_string_new( "" );
	count = g_list_length( *priv->entries );

	if( count == 1 ){
		g_string_printf( gstr1,
				_( "The selected entry does not originate from any operation,"
					" and so does not have any related entry." ));
		g_string_printf( gstr2,
				_( "Are you sure you want to remove this '%s' entry ?\n"
					"Note that this will most probably break off the balance of your books." ),
							ofo_entry_get_label( priv->entry ));
	} else {
		g_string_printf( gstr1,
				_( "%u other entries originate from the same operation %lu,"
					" and should be deleted as well." ),
							count, priv->ope_number );
		g_string_printf( gstr2,
				_( "Do you confirm you want to remove this '%s' entry "
					"and all other %u related entries ?" ),
							ofo_entry_get_label( priv->entry ), count-1 );
	}

	gtk_label_set_text( GTK_LABEL( priv->entry1_label ), gstr1->str );
	gtk_label_set_text( GTK_LABEL( priv->entry2_label ), gstr2->str );

	g_string_free( gstr1, TRUE );
	g_string_free( gstr2, TRUE );
}

static void
on_all_toggled( GtkToggleButton *button, ofaEntryPageDelconf *self )
{
	ofaEntryPageDelconfPrivate *priv;

	priv = ofa_entry_page_delconf_get_instance_private( self );

	priv->all_entries = gtk_toggle_button_get_active( button );

	g_debug( "setting all_entries to %s", priv->all_entries ? "True":"False" );
}

/*
static void
on_single_toggled( GtkToggleButton *button, ofaEntryPageDelconf *self )
{
	ofaEntryPageDelconfPrivate *priv;

	priv = ofa_entry_page_delconf_get_instance_private( self );

	priv->all_entries = !gtk_toggle_button_get_active( button );
}
*/

/*
 * Note that we cannot run a non-modal dialog from a modal one
 */
static void
on_vope_clicked( GtkButton *button, ofaEntryPageDelconf *self )
{
	ofaEntryPageDelconfPrivate *priv;

	priv = ofa_entry_page_delconf_get_instance_private( self );

	ofa_operation_group_run_modal( priv->getter, priv->ope_number );
}

static void
on_ok_clicked( GtkButton *button, ofaEntryPageDelconf *self )
{
	manage_settle_concil( self );
}

/*
 * Ask for an extra confirmation if any of the entries are settled and/
 * or conciliated.
 */
static void
manage_settle_concil( ofaEntryPageDelconf *self )
{
	ofaEntryPageDelconfPrivate *priv;
	guint count, settle_count, concil_count;
	ofoEntry *entry;
	ofoConcil *concil;
	GList *it;
	GString *gstr;
	GtkWindow *toplevel;
	gboolean ok;

	priv = ofa_entry_page_delconf_get_instance_private( self );

	count = g_list_length( *priv->entries );
	settle_count = 0;
	concil_count = 0;

	for( it=*priv->entries ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		if( ofo_entry_get_settlement_number( entry ) > 0 ){
			settle_count += 1;
		}
		concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
		if( concil ){
			concil_count += 1;
		}
	}

	if( settle_count > 0 || concil_count > 0 ){
		gstr = g_string_new( "" );

		if( settle_count > 0 ){
			if( settle_count == 1 ){
				if( count == 1 ){
					gstr = g_string_append( gstr, _( "The entry has been settled." ));
				} else {
					gstr = g_string_append( gstr, _( "One entry has been settled." ));
				}
				gstr = g_string_append( gstr,
							_( "\nDeleting it will also automatically delete all the settlement group." ));
			} else {
				gstr = g_string_append( gstr,
						_( "%u entries have been settled.\n"
							"Deleting them will also automatically delete each of these settlement groups."));
			}
		}

		if( concil_count > 0 ){
			if( gstr->len > 0 ){
				gstr = g_string_append( gstr, "\n" );
			}
			if( concil_count == 1 ){
				if( count == 1 ){
					gstr = g_string_append( gstr, _( "The entry has been reconciliated." ));
				} else {
					gstr = g_string_append( gstr, _( "One entry has been reconciliated." ));
				}
				gstr = g_string_append( gstr,
						_( "\nDeleting it will also automatically delete all the conciliation group." ));
			} else {
				gstr = g_string_append( gstr,
						_( "%u entries have been reconciliated.\n"
							"Deleting them will also automatically delete each of these conciliation groups."));
			}
		}

		gstr = g_string_append( gstr, _( "\nAre you sure ?" ));
		toplevel = GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
		ok = my_utils_dialog_question( toplevel, gstr->str, _( "_Yes, go to delete" ));

		g_string_free( gstr, TRUE );

		if( !ok ){
			g_list_free( *priv->entries );
			*priv->entries = NULL;
			g_debug( "sending cancel" );
			gtk_dialog_response( GTK_DIALOG( self ), GTK_RESPONSE_CANCEL );
		}
	}
}
