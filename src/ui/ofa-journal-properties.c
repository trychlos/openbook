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

#include <glib/gi18n.h>

#include "ui/my-utils.h"
#include "ui/ofa-base-dialog-prot.h"
#include "ui/ofa-journal-properties.h"
#include "ui/ofa-main-window.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-journal.h"

/* private instance data
 */
struct _ofaJournalPropertiesPrivate {

	/* internals
	 */
	ofoJournal *journal;
	gboolean    is_new;
	gboolean    updated;

	/* data
	 */
	gchar      *mnemo;
	gchar      *label;
	gchar      *maj_user;
	GTimeVal    maj_stamp;
	GDate       cloture;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-journal-properties.ui";
static const gchar  *st_ui_id  = "JournalPropertiesDlg";

G_DEFINE_TYPE( ofaJournalProperties, ofa_journal_properties, OFA_TYPE_BASE_DIALOG )

static void      v_init_dialog( ofaBaseDialog *dialog );
static void      on_mnemo_changed( GtkEntry *entry, ofaJournalProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaJournalProperties *self );
static void      check_for_enable_dlg( ofaJournalProperties *self );
static gboolean  is_dialog_validable( ofaJournalProperties *self );
static gboolean  v_quit_on_ok( ofaBaseDialog *dialog );
static gboolean  do_update( ofaJournalProperties *self );

static void
journal_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_journal_properties_finalize";
	ofaJournalPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_JOURNAL_PROPERTIES( instance ));

	priv = OFA_JOURNAL_PROPERTIES( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->maj_user );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_journal_properties_parent_class )->finalize( instance );
}

static void
journal_properties_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_JOURNAL_PROPERTIES( instance ));

	if( !OFA_BASE_DIALOG( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_journal_properties_parent_class )->dispose( instance );
}

static void
ofa_journal_properties_init( ofaJournalProperties *self )
{
	static const gchar *thisfn = "ofa_journal_properties_instance_init";

	g_return_if_fail( OFA_IS_JOURNAL_PROPERTIES( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaJournalPropertiesPrivate, 1 );

	self->private->is_new = FALSE;
	self->private->updated = FALSE;
}

static void
ofa_journal_properties_class_init( ofaJournalPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_journal_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = journal_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = journal_properties_finalize;

	OFA_BASE_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	OFA_BASE_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * ofa_journal_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an journal
 */
gboolean
ofa_journal_properties_run( ofaMainWindow *main_window, ofoJournal *journal )
{
	static const gchar *thisfn = "ofa_journal_properties_run";
	ofaJournalProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, journal=%p",
				thisfn, ( void * ) main_window, ( void * ) journal );

	self = g_object_new(
					OFA_TYPE_JOURNAL_PROPERTIES,
					OFA_PROP_MAIN_WINDOW, main_window,
					OFA_PROP_DIALOG_XML,  st_ui_xml,
					OFA_PROP_DIALOG_NAME, st_ui_id,
					NULL );

	self->private->journal = journal;

	ofa_base_dialog_run_dialog( OFA_BASE_DIALOG( self ));

	updated = self->private->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( ofaBaseDialog *dialog )
{
	ofaJournalPropertiesPrivate *priv;
	gchar *title;
	const gchar *jou_mnemo;
	GtkEntry *entry;

	priv = OFA_JOURNAL_PROPERTIES( dialog )->private;

	jou_mnemo = ofo_journal_get_mnemo( priv->journal );
	if( !jou_mnemo ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new journal" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » journal" ), jou_mnemo );
	}
	gtk_window_set_title( GTK_WINDOW( dialog->prot->dialog ), title );

	priv->mnemo = g_strdup( jou_mnemo );
	entry = GTK_ENTRY( my_utils_container_get_child_by_name(
					GTK_CONTAINER( dialog->prot->dialog ), "p1-mnemo" ));
	if( priv->mnemo ){
		gtk_entry_set_text( entry, priv->mnemo );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mnemo_changed ), dialog );

	priv->label = g_strdup( ofo_journal_get_label( priv->journal ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name(
					GTK_CONTAINER( dialog->prot->dialog ), "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), dialog );

	my_utils_init_notes_ex( dialog->prot->dialog, journal );
	my_utils_init_maj_user_stamp_ex( dialog->prot->dialog, journal );

	check_for_enable_dlg( OFA_JOURNAL_PROPERTIES( dialog ));
}

static void
on_mnemo_changed( GtkEntry *entry, ofaJournalProperties *self )
{
	g_free( self->private->mnemo );
	self->private->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaJournalProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaJournalProperties *self )
{
	GtkWidget *button;

	button = my_utils_container_get_child_by_name(
						GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "btn-ok" );

	gtk_widget_set_sensitive( button, is_dialog_validable( self ));
}

static gboolean
is_dialog_validable( ofaJournalProperties *self )
{
	gboolean ok;
	ofaJournalPropertiesPrivate *priv;
	ofoJournal *exists;

	priv = self->private;

	ok = ofo_journal_is_valid( priv->mnemo, priv->label );

	if( ok ){
		exists = ofo_journal_get_by_mnemo(
				ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self )), priv->mnemo );
		ok &= !exists ||
				( !priv->is_new && !g_utf8_collate( priv->mnemo, ofo_journal_get_mnemo( priv->journal )));
	}

	return( ok );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( ofaBaseDialog *dialog )
{
	return( do_update( OFA_JOURNAL_PROPERTIES( dialog )));
}

/*
 * either creating a new journal (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 */
static gboolean
do_update( ofaJournalProperties *self )
{
	ofaJournalPropertiesPrivate *priv;
	ofoDossier *dossier;
	gchar *prev_mnemo;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->private;
	dossier = ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self ));
	prev_mnemo = g_strdup( ofo_journal_get_mnemo( priv->journal ));

	/* le nouveau mnemo n'est pas encore utilisé,
	 * ou bien il est déjà utilisé par ce même journal (n'a pas été modifié)
	 */
	ofo_journal_set_mnemo( priv->journal, priv->mnemo );
	ofo_journal_set_label( priv->journal, priv->label );
	my_utils_getback_notes_ex( OFA_BASE_DIALOG( self )->prot->dialog, journal );

	if( priv->is_new ){
		priv->updated =
				ofo_journal_insert( priv->journal, dossier );
	} else {
		priv->updated =
				ofo_journal_update( priv->journal, dossier, prev_mnemo );
	}

	g_free( prev_mnemo );

	return( priv->updated );
}
