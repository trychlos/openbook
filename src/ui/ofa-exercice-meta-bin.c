/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
#include "my/my-date-editable.h"
#include "my/my-ibin.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"

#include "ui/ofa-application.h"
#include "ui/ofa-exercice-meta-bin.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* initialization
	 */
	ofaIGetter         *getter;
	gchar              *settings_prefix;
	guint               rule;

	/* UI
	 */
	GtkSizeGroup       *group0;

	/* runtime data
	 */
	ofaIDBDossierMeta  *dossier_meta;
	GDate               begin;
	GDate               end;
	gboolean            is_current;
}
	ofaExerciceMetaBinPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-exercice-meta-bin.ui";

static void          setup_bin( ofaExerciceMetaBin *self );
static void          on_begin_changed( GtkEditable *editable, ofaExerciceMetaBin *self );
static void          on_end_changed( GtkEditable *editable, ofaExerciceMetaBin *self );
static void          on_archive_toggled( GtkToggleButton *button, ofaExerciceMetaBin *self );
static void          changed_composite( ofaExerciceMetaBin *self );
static gboolean      is_valid( ofaExerciceMetaBin *self, gchar **msg );
static void          ibin_iface_init( myIBinInterface *iface );
static guint         ibin_get_interface_version( void );
static GtkSizeGroup *ibin_get_size_group( const myIBin *instance, guint column );
static gboolean      ibin_is_valid( const myIBin *instance, gchar **msgerr );

G_DEFINE_TYPE_EXTENDED( ofaExerciceMetaBin, ofa_exercice_meta_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaExerciceMetaBin )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IBIN, ibin_iface_init ))

static void
exercice_meta_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_finalize";
	ofaExerciceMetaBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXERCICE_META_BIN( instance ));

	/* free data members here */
	priv = ofa_exercice_meta_bin_get_instance_private( OFA_EXERCICE_META_BIN( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_meta_bin_parent_class )->finalize( instance );
}

static void
exercice_meta_bin_dispose( GObject *instance )
{
	ofaExerciceMetaBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXERCICE_META_BIN( instance ));

	priv = ofa_exercice_meta_bin_get_instance_private( OFA_EXERCICE_META_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_meta_bin_parent_class )->dispose( instance );
}

static void
ofa_exercice_meta_bin_init( ofaExerciceMetaBin *self )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_instance_init";
	ofaExerciceMetaBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXERCICE_META_BIN( self ));

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	my_date_clear( &priv->begin );
	my_date_clear( &priv->end );
}

static void
ofa_exercice_meta_bin_class_init( ofaExerciceMetaBinClass *klass )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exercice_meta_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = exercice_meta_bin_finalize;
}

/**
 * ofa_exercice_meta_bin_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the prefix of the key in user settings.
 * @rule: the usage of this widget.
 *
 * Returns: a newly defined composite widget which aggregates exercice
 * meta datas: name and DBMS provider.
 */
ofaExerciceMetaBin *
ofa_exercice_meta_bin_new( ofaIGetter *getter, const gchar *settings_prefix, guint rule )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_new";
	ofaExerciceMetaBin *self;
	ofaExerciceMetaBinPrivate *priv;

	g_debug( "%s: getter=%p, settings_prefix=%s, rule=%u",
			thisfn, ( void * ) getter, settings_prefix, rule );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( settings_prefix ), NULL );

	self = g_object_new( OFA_TYPE_EXERCICE_META_BIN, NULL );

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	priv->getter = getter;
	priv->rule = rule;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_bin( self );

	return( self );
}

static void
setup_bin( ofaExerciceMetaBin *self )
{
	ofaExerciceMetaBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label, *btn;

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "emb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "emb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	/* beginning date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-begin-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-begin-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_entry_format( GTK_EDITABLE( entry ), ofa_prefs_date_get_display_format( priv->getter ));
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_get_check_format( priv->getter ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_begin_changed ), self );
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->begin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-begin-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* ending date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-end-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-end-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_entry_format( GTK_EDITABLE( entry ), ofa_prefs_date_get_display_format( priv->getter ));
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_get_check_format( priv->getter ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_end_changed ), self );
	my_date_editable_set_date( GTK_EDITABLE( entry ), &priv->end );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-end-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* archive flag
	 * defaults to be initially cleared */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "emb-current-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	g_signal_connect( btn, "toggled", G_CALLBACK( on_archive_toggled ), self );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), FALSE );
	on_archive_toggled( GTK_TOGGLE_BUTTON( btn ), self );

#if 0
	switch( priv->rule ){
		/* when defining a new dossier, the new exercice is current
		 * and this is mandatory */
		case HUB_RULE_DOSSIER_NEW:
			gtk_widget_set_sensitive( btn, FALSE );
			break;
	}
#endif

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_exercice_meta_bin_set_dossier_meta:
 * @bin: this #ofaExerciceEditBin instance.
 * @dossier_meta: [allow-none]: the #ofaIDBDossierMeta to be attached to.
 *
 * Set the #ofaIDBDossierMeta dossier.
 */
void
ofa_exercice_meta_bin_set_dossier_meta( ofaExerciceMetaBin *bin, ofaIDBDossierMeta *dossier_meta )
{
	ofaExerciceMetaBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_EXERCICE_META_BIN( bin ));
	g_return_if_fail( !dossier_meta || OFA_IS_IDBDOSSIER_META( dossier_meta ));

	priv = ofa_exercice_meta_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->dossier_meta = dossier_meta;
}

static void
on_begin_changed( GtkEditable *editable, ofaExerciceMetaBin *self )
{
	ofaExerciceMetaBinPrivate *priv;

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	my_date_set_from_date( &priv->begin, my_date_editable_get_date( editable, NULL ));

	changed_composite( self );
}

static void
on_end_changed( GtkEditable *editable, ofaExerciceMetaBin *self )
{
	ofaExerciceMetaBinPrivate *priv;

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	my_date_set_from_date( &priv->end, my_date_editable_get_date( editable, NULL ));

	changed_composite( self );
}

static void
on_archive_toggled( GtkToggleButton *button, ofaExerciceMetaBin *self )
{
	ofaExerciceMetaBinPrivate *priv;

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	priv->is_current = !gtk_toggle_button_get_active( button );

	changed_composite( self );
}

static void
changed_composite( ofaExerciceMetaBin *self )
{
	g_signal_emit_by_name( self, "my-ibin-changed" );
}

/*
 * if date are set, beginning must be less or equal than ending
 * both dates are mandatory when defining an archive
 */
static gboolean
is_valid( ofaExerciceMetaBin *self, gchar **msg )
{
	ofaExerciceMetaBinPrivate *priv;
	gboolean valid, begin_set, end_set;

	priv = ofa_exercice_meta_bin_get_instance_private( self );

	valid = TRUE;
	begin_set = my_date_is_valid( &priv->begin );
	end_set = my_date_is_valid( &priv->end );

	if( begin_set && end_set ){
		if( my_date_compare( &priv->begin, &priv->end ) > 0 ){
			valid = FALSE;
			*msg = g_strdup( _( "Beginning date is greater than ending date" ));
		}
	}

	if( valid && !priv->is_current ){
		if( !begin_set ){
			valid = FALSE;
			*msg = g_strdup( _( "Beginning date must be set when defining an archive" ));
		}
		else if( !end_set ){
			valid = FALSE;
			*msg = g_strdup( _( "Ending date must be set when defining an archive" ));
		}
	}

	if( valid && priv->dossier_meta ){
		if( priv->is_current ){
			if( ofa_idbdossier_meta_get_current_period( priv->dossier_meta ) != NULL ){
				valid = FALSE;
				*msg = g_strdup( _( "A current exercice is already defined, refusing to define another" ));
			}
		} else {
			if( ofa_idbdossier_meta_get_archived_period( priv->dossier_meta, &priv->begin ) != NULL ||
					ofa_idbdossier_meta_get_archived_period( priv->dossier_meta, &priv->end ) != NULL ){
				valid = FALSE;
				*msg = g_strdup( _( "An archived exercice is already defined on these dates, refusing to define another" ));
			}
		}
	}

	return( valid );
}

/**
 * ofa_exercice_meta_bin_apply:
 * @bin: this #ofaExerciceMetaBin instance.
 *
 * Returns: a newly created #ofaIDBExerciceMeta attached to the @dossier_meta.
 *
 * Note that this widget cannot be eventually valid while a dossier has
 * not been set. This is a programming error to not have set a dossier
 * at validation time.
 */
ofaIDBExerciceMeta *
ofa_exercice_meta_bin_apply( ofaExerciceMetaBin *bin )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_apply";
	ofaExerciceMetaBinPrivate *priv;
	ofaIDBExerciceMeta *exercice_meta;

	g_debug( "%s: bin=%p", thisfn, ( void * ) bin );

	g_return_val_if_fail( bin && OFA_IS_EXERCICE_META_BIN( bin ), NULL );
	g_return_val_if_fail( my_ibin_is_valid( MY_IBIN( bin ), NULL ), NULL );

	priv = ofa_exercice_meta_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->dossier_meta && OFA_IS_IDBDOSSIER_META( priv->dossier_meta ), NULL );

	exercice_meta = NULL;

	switch( priv->rule ){
		case HUB_RULE_DOSSIER_NEW:
		case HUB_RULE_EXERCICE_NEW:
			exercice_meta = ofa_idbdossier_meta_new_period( priv->dossier_meta, TRUE );
			ofa_idbexercice_meta_set_begin_date( exercice_meta, &priv->begin );
			ofa_idbexercice_meta_set_end_date( exercice_meta, &priv->end );
			ofa_idbexercice_meta_set_current( exercice_meta, priv->is_current );
			break;
		default:
			g_warning( "%s: unmanaged rule=%u", thisfn, priv->rule );
	}

	return( exercice_meta );
}

/*
 * myIBin interface management
 */
static void
ibin_iface_init( myIBinInterface *iface )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_ibin_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ibin_get_interface_version;
	iface->get_size_group = ibin_get_size_group;
	iface->is_valid = ibin_is_valid;
}

static guint
ibin_get_interface_version( void )
{
	return( 1 );
}

static GtkSizeGroup *
ibin_get_size_group( const myIBin *instance, guint column )
{
	static const gchar *thisfn = "ofa_exercice_meta_bin_ibin_get_size_group";
	ofaExerciceMetaBinPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_EXERCICE_META_BIN( instance ), NULL );

	priv = ofa_exercice_meta_bin_get_instance_private( OFA_EXERCICE_META_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: invalid column=%u", thisfn, column );

	return( NULL );
}

/*
 * Both beginning and ending dates must be set when defining an archive.
 */
gboolean
ibin_is_valid( const myIBin *instance, gchar **msgerr )
{
	ofaExerciceMetaBinPrivate *priv;
	gboolean valid;
	gchar *msg;

	g_return_val_if_fail( instance && OFA_IS_EXERCICE_META_BIN( instance ), FALSE );

	priv = ofa_exercice_meta_bin_get_instance_private( OFA_EXERCICE_META_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	msg = NULL;
	if( msgerr ){
		*msgerr = NULL;
	}

	valid = is_valid( OFA_EXERCICE_META_BIN( instance ), &msg );
	if( !valid && msgerr ){
		*msgerr = g_strdup( msg );
	}

	g_free( msg );

	return( valid );
}
