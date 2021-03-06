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
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-prefs.h"

/* some data attached to each IDBExerciceMeta instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {

	/* initialization
	 */
	ofaIDBDossierMeta *dossier_meta;

	/* second init stage
	 */
	gchar             *settings_key;
	gchar             *settings_id;

	/* runtime
	 */
	GDate              begin;
	GDate              end;
	gboolean           current;
}
	sIDBMeta;

#define IDBEXERCICE_META_LAST_VERSION     1
#define IDBEXERCICE_META_DATA            "idbexercice-meta-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType     register_type( void );
static void      interface_base_init( ofaIDBExerciceMetaInterface *klass );
static void      interface_base_finalize( ofaIDBExerciceMetaInterface *klass );
static void      read_settings( ofaIDBExerciceMeta *self );
static void      write_settings( const ofaIDBExerciceMeta *self );
static sIDBMeta *get_instance_data( const ofaIDBExerciceMeta *self );
static void      on_instance_finalized( sIDBMeta *sdata, GObject *finalized_instance );

/**
 * ofa_idbexercice_meta_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbexercice_meta_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbexercice_meta_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbexercice_meta_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBExerciceMetaInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBExerciceMeta", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBExerciceMetaInterface *klass )
{
	static const gchar *thisfn = "ofa_idbexercice_meta_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDBExerciceMetaInterface *klass )
{
	static const gchar *thisfn = "ofa_idbexercice_meta_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbexercice_meta_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbexercice_meta_get_interface_last_version( void )
{
	return( IDBEXERCICE_META_LAST_VERSION );
}

/**
 * ofa_idbexercice_meta_get_interface_version:
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_idbexercice_meta_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IDBEXERCICE_META );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIDBExerciceMetaInterface * ) iface )->get_interface_version ){
		version = (( ofaIDBExerciceMetaInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIDBExerciceMeta::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_idbexercice_meta_unref:
 * @exercice_meta: this #ofaIDBExerciceMeta instance.
 *
 * Unref the GObject.
 */
void
ofa_idbexercice_meta_unref( ofaIDBExerciceMeta *exercice_meta )
{
	static const gchar *thisfn = "ofa_idbexercice_meta_unref";

	g_debug( "%s: meta=%p (%s), ref_count=%d",
			thisfn, ( void * ) exercice_meta, G_OBJECT_TYPE_NAME( exercice_meta ),
			G_OBJECT( exercice_meta )->ref_count );

	g_return_if_fail( exercice_meta && OFA_IS_IDBEXERCICE_META( exercice_meta ));

	g_object_unref( exercice_meta );
}

/**
 * ofa_idbexercice_meta_get_dossier_meta:
 * @exercice_meta: this #ofaIDBExerciceMeta instance.
 *
 * Returns: the attached #ofaIDBDossierMeta dossier.
 *
 * The returned reference is owned by @exercice_meta, and should not be
 * released by the caller.
 */
ofaIDBDossierMeta *
ofa_idbexercice_meta_get_dossier_meta( const ofaIDBExerciceMeta *exercice_meta )
{
	sIDBMeta *sdata;

	g_return_val_if_fail( exercice_meta && OFA_IS_IDBEXERCICE_META( exercice_meta ), NULL );

	sdata = get_instance_data( exercice_meta );

	return( sdata->dossier_meta );
}

/**
 * ofa_idbexercice_meta_set_dossier_meta:
 * @exercice_meta: this #ofaIDBExerciceMeta instance.
 * @dossier_meta: the #ofaIDBDossierMeta dossier.
 *
 * Attach the @dossier_meta to the @exercice_meta.
 */
void
ofa_idbexercice_meta_set_dossier_meta( ofaIDBExerciceMeta *exercice_meta, ofaIDBDossierMeta *dossier_meta )
{
	sIDBMeta *sdata;

	g_return_if_fail( exercice_meta && OFA_IS_IDBEXERCICE_META( exercice_meta ));
	g_return_if_fail( dossier_meta && OFA_IS_IDBDOSSIER_META( dossier_meta ));

	sdata = get_instance_data( exercice_meta );

	sdata->dossier_meta = dossier_meta;
}

/**
 * ofa_idbexercice_meta_get_settings_key:
 * @exercice_meta: this #ofaIDBExerciceMeta instance.
 *
 * Returns: the settings key.
 */
const gchar *
ofa_idbexercice_meta_get_settings_key( const ofaIDBExerciceMeta *exercice_meta )
{
	sIDBMeta *sdata;

	g_return_val_if_fail( exercice_meta && OFA_IS_IDBEXERCICE_META( exercice_meta ), NULL );

	sdata = get_instance_data( exercice_meta );

	return( sdata->settings_key );
}

/**
 * ofa_idbexercice_meta_set_settings_key:
 * @exercice_meta: this #ofaIDBExerciceMeta instance.
 * @settings_key: [allow-none]: the exercice key in the settings.
 *
 * Set the settings key.
 */
void
ofa_idbexercice_meta_set_settings_key( ofaIDBExerciceMeta *exercice_meta, const gchar *settings_key )
{
	sIDBMeta *sdata;

	g_return_if_fail( exercice_meta && OFA_IS_IDBEXERCICE_META( exercice_meta ));

	sdata = get_instance_data( exercice_meta );

	g_free( sdata->settings_key );
	sdata->settings_key = g_strdup( settings_key );
}

/**
 * ofa_idbexercice_meta_get_settings_id:
 * @exercice_meta: this #ofaIDBExerciceMeta instance.
 *
 * Returns: the identifier of the settings key.
 */
const gchar *
ofa_idbexercice_meta_get_settings_id( const ofaIDBExerciceMeta *exercice_meta )
{
	sIDBMeta *sdata;

	g_return_val_if_fail( exercice_meta && OFA_IS_IDBEXERCICE_META( exercice_meta ), NULL );

	sdata = get_instance_data( exercice_meta );

	return( sdata->settings_id );
}

/**
 * ofa_idbexercice_meta_set_settings_id:
 * @exercice_meta: this #ofaIDBExerciceMeta instance.
 * @settings_id: [allow-none]: the identifier of the exercice in the settings.
 *
 * Set the identifier of the settings key.
 */
void
ofa_idbexercice_meta_set_settings_id( ofaIDBExerciceMeta *exercice_meta, const gchar *settings_id )
{
	sIDBMeta *sdata;

	g_return_if_fail( exercice_meta && OFA_IS_IDBEXERCICE_META( exercice_meta ));

	sdata = get_instance_data( exercice_meta );

	g_free( sdata->settings_id );
	sdata->settings_id = g_strdup( settings_id );
}

/**
 * ofa_idbexercice_meta_set_from_settings:
 * @exercice_meta: this #ofaIDBExerciceMeta instance.
 *
 * Reads from dossier settings informations relative to the @exercice_meta
 * exercice.
 */
void
ofa_idbexercice_meta_set_from_settings( ofaIDBExerciceMeta *exercice_meta )
{
	static const gchar *thisfn = "ofa_idbexercice_meta_set_from_settings";
	sIDBMeta *sdata;

	g_debug( "%s: exercice_meta=%p", thisfn, ( void * ) exercice_meta );

	g_return_if_fail( exercice_meta && OFA_IS_IDBEXERCICE_META( exercice_meta ));

	read_settings( exercice_meta );

	if( OFA_IDBEXERCICE_META_GET_INTERFACE( exercice_meta )->set_from_settings ){
		sdata = get_instance_data( exercice_meta );
		OFA_IDBEXERCICE_META_GET_INTERFACE( exercice_meta )->set_from_settings( exercice_meta, sdata->settings_id );
		return;
	}

	g_info( "%s: ofaIDBExerciceMeta's %s implementation does not provide 'set_from_settings()' method",
			thisfn, G_OBJECT_TYPE_NAME( exercice_meta ));
}

/**
 * ofa_idbexercice_meta_set_from_editor:
 * @exercice_meta: this #ofaIDBExerciceMeta instance.
 * @editor: the #ofaIDBExerciceEditor widget.
 *
 * Setup the @exercice_meta exercice with informations from @editor.
 */
void
ofa_idbexercice_meta_set_from_editor( ofaIDBExerciceMeta *exercice_meta, ofaIDBExerciceEditor *editor )
{
	static const gchar *thisfn = "ofa_idbexercice_meta_set_from_editor";
	sIDBMeta *sdata;

	g_debug( "%s: exercice_meta=%p, editor=%p",
			thisfn, ( void * ) exercice_meta, editor );

	g_return_if_fail( exercice_meta && OFA_IS_IDBEXERCICE_META( exercice_meta ));

	write_settings( exercice_meta );

	if( OFA_IDBEXERCICE_META_GET_INTERFACE( exercice_meta )->set_from_editor ){
		sdata = get_instance_data( exercice_meta );
		OFA_IDBEXERCICE_META_GET_INTERFACE( exercice_meta )->set_from_editor( exercice_meta, editor, sdata->settings_id );
		return;
	}

	g_info( "%s: ofaIDBExerciceMeta's %s implementation does not provide 'set_from_editor()' method",
			thisfn, G_OBJECT_TYPE_NAME( exercice_meta ));
}

/**
 * ofa_idbexercice_meta_get_begin_date:
 * @period: this #ofaIDBExerciceMeta instance.
 *
 * Returns: the beginning date of the @period.
 */
const GDate *
ofa_idbexercice_meta_get_begin_date( const ofaIDBExerciceMeta *period )
{
	sIDBMeta *sdata;

	g_return_val_if_fail( period && OFA_IS_IDBEXERCICE_META( period ), NULL );

	sdata = get_instance_data( period );

	return(( const GDate * ) &sdata->begin );
}

/**
 * ofa_idbexercice_meta_set_begin_date:
 * @period: this #ofaIDBExerciceMeta instance.
 * @date: the beginning date to be set.
 *
 * Set the beginning date of the @period.
 */
void
ofa_idbexercice_meta_set_begin_date( ofaIDBExerciceMeta *period, const GDate *date )
{
	sIDBMeta *sdata;

	g_return_if_fail( period && OFA_IS_IDBEXERCICE_META( period ));

	sdata = get_instance_data( period );

	my_date_set_from_date( &sdata->begin, date );
}

/**
 * ofa_idbexercice_meta_get_end_date:
 * @period: this #ofaIDBExerciceMeta instance.
 *
 * Returns: the ending date of the @period.
 */
const GDate *
ofa_idbexercice_meta_get_end_date( const ofaIDBExerciceMeta *period )
{
	sIDBMeta *sdata;

	g_return_val_if_fail( period && OFA_IS_IDBEXERCICE_META( period ), NULL );

	sdata = get_instance_data( period );

	return(( const GDate * ) &sdata->end );
}

/**
 * ofa_idbexercice_meta_set_end_date:
 * @period: this #ofaIDBExerciceMeta instance.
 * @date: the endning date to be set.
 *
 * Set the ending date of the @period.
 */
void
ofa_idbexercice_meta_set_end_date( ofaIDBExerciceMeta *period, const GDate *date )
{
	sIDBMeta *sdata;

	g_return_if_fail( period && OFA_IS_IDBEXERCICE_META( period ));

	sdata = get_instance_data( period );

	my_date_set_from_date( &sdata->end, date );
}

/**
 * ofa_idbexercice_meta_get_current:
 * @period: this #ofaIDBExerciceMeta instance.
 *
 * Returns: %TRUE if the financial period is current, i.e. may be
 * modified, %FALSE else.
 */
gboolean
ofa_idbexercice_meta_get_current( const ofaIDBExerciceMeta *period )
{
	sIDBMeta *sdata;

	g_return_val_if_fail( period && OFA_IS_IDBEXERCICE_META( period ), FALSE );

	sdata = get_instance_data( period );

	return( sdata->current );
}

/**
 * ofa_idbexercice_meta_set_current:
 * @period: this #ofaIDBExerciceMeta instance.
 * @current: whether this @period is current.
 *
 * Set the @current flag.
 */
void
ofa_idbexercice_meta_set_current( ofaIDBExerciceMeta *period, gboolean current )
{
	sIDBMeta *sdata;

	g_return_if_fail( period && OFA_IS_IDBEXERCICE_META( period ));

	sdata = get_instance_data( period );

	sdata->current = current;
}

/**
 * ofa_idbexercice_meta_get_status:
 * @period: this #ofaIDBExerciceMeta instance.
 *
 * Returns: the localized status string, as a newly allocated string
 * which should be g_free() by the caller.
 *
 * English example:
 * - 'Current' for the currently opened period
 * - 'Archived' for any closed period.
 */
gchar *
ofa_idbexercice_meta_get_status( const ofaIDBExerciceMeta *period )
{
	g_return_val_if_fail( period && OFA_IS_IDBEXERCICE_META( period ), NULL );

	return( g_strdup( ofa_idbexercice_meta_get_current( period ) ? _( "Current") : _( "Archived" )));
}

/**
 * ofa_idbexercice_meta_get_label:
 * @period: this #ofaIDBExerciceMeta instance.
 *
 * Returns: a localized string which describes and qualifies the @period,
 *  as a newly allocated string which should be g_free() by the caller.
 *
 * English example:
 * - 'Current exercice to 31/12/2013' for the currently opened period
 * - 'Archived exercice from 01/01/2012 to 31/12/2012'.
 */
gchar *
ofa_idbexercice_meta_get_label( const ofaIDBExerciceMeta *period )
{
	sIDBMeta *sdata;
	GString *svalue;
	gchar *sdate;
	const GDate *begin, *end;
	ofaIDBProvider *provider;
	ofaIGetter *getter;

	g_return_val_if_fail( period && OFA_IS_IDBEXERCICE_META( period ), NULL );

	sdata = get_instance_data( period );
	provider = ofa_idbdossier_meta_get_provider( sdata->dossier_meta );
	getter = ofa_idbprovider_get_getter( provider );

	svalue = g_string_new( ofa_idbexercice_meta_get_current( period )
					? _( "Current exercice" )
					: _( "Archived exercice" ));

	begin = ofa_idbexercice_meta_get_begin_date( period );
	if( my_date_is_valid( begin )){
		sdate = my_date_to_str( begin, ofa_prefs_date_get_display_format( getter ));
		g_string_append_printf( svalue, _( " from %s" ), sdate );
		g_free( sdate );
	}

	end = ofa_idbexercice_meta_get_end_date( period );
	if( my_date_is_valid( end )){
		sdate = my_date_to_str( end, ofa_prefs_date_get_display_format( getter ));
		g_string_append_printf( svalue, _( " to %s" ), sdate );
		g_free( sdate );
	}

	return( g_string_free( svalue, FALSE ));
}

/**
 * ofa_idbexercice_meta_get_name:
 * @period: this #ofaIDBExerciceMeta instance.
 *
 * Returns: a plugin-specific which qualifies the @period,
 *  as a newly allocated string which should be g_free() by the caller.
 *
 * Example:
 * - MySQL DBMS provider uses this method to return the database name.
 */
gchar *
ofa_idbexercice_meta_get_name( const ofaIDBExerciceMeta *period )
{
	static const gchar *thisfn = "ofa_idbexercice_meta_get_name";

	g_return_val_if_fail( period && OFA_IS_IDBEXERCICE_META( period ), NULL );

	if( OFA_IDBEXERCICE_META_GET_INTERFACE( period )->get_name ){
		return( OFA_IDBEXERCICE_META_GET_INTERFACE( period )->get_name( period ));
	}

	g_info( "%s: ofaIDBExerciceMeta's %s implementation does not provide 'get_name() method",
			thisfn, G_OBJECT_TYPE_NAME( period ));
	return( NULL );
}

/**
 * ofa_idbexercice_meta_compare:
 * @a: a #ofaIDBExerciceMeta instance.
 * @b: another #ofaIDBExerciceMeta instance.
 *
 * Compare the two periods by their dates.
 *
 * Returns: -1 if @a < @b, +1 if @a > @b, 0 if they are equal.
 */
gint
ofa_idbexercice_meta_compare( const ofaIDBExerciceMeta *a, const ofaIDBExerciceMeta *b )
{
	static const gchar *thisfn = "ofa_idbexercice_meta_compare";
	gint cmp;
	const GDate *a_begin, *a_end, *b_begin, *b_end;
	gboolean a_status, b_status;

	cmp = 0;

	if( a ){
		a_begin = ofa_idbexercice_meta_get_begin_date( a );
		a_end = ofa_idbexercice_meta_get_end_date( a );
		a_status = ofa_idbexercice_meta_get_current( a );

		if( b ){
			b_begin = ofa_idbexercice_meta_get_begin_date( b );
			b_end = ofa_idbexercice_meta_get_end_date( b );
			b_status = ofa_idbexercice_meta_get_current( b );

			cmp = my_date_compare_ex( a_begin, b_begin, TRUE );
			if( cmp == 0 ){
				cmp = my_date_compare_ex( a_end, b_end, FALSE );
			}
			if( cmp == 0 ){
				cmp = ( a_status == b_status ? 0 : ( a_status ? +1 : -1 ));
			}
			if( cmp == 0 ){
				if( OFA_IDBEXERCICE_META_GET_INTERFACE( a )->compare ){
					cmp = OFA_IDBEXERCICE_META_GET_INTERFACE( a )->compare( a, b );
				} else {
					g_info( "%s: ofaIDBExerciceMeta's %s implementation does not provide 'compare() method",
							thisfn, G_OBJECT_TYPE_NAME( a ));
				}
			}
			return( cmp );
		}
		/* if a is set, and b is not set, then a > b */
		return( 1 );

	} else if( b ) {
		/* if a is not set and b is set, then a < b */
		return( -1 );

	} else {
		/* both a and b are unset */
		return( 0 );
	}
}

/**
 * ofa_idbexercice_meta_is_suitable:
 * @period: a #ofaIDBExerciceMeta instance.
 * @begin: [allow-none]: the beginning date of a period.
 * @end: [allow-none]: the ending date of a period.
 *
 * Returns: %TRUE if @period is compatible with @begin and @end.
 */
gboolean
ofa_idbexercice_meta_is_suitable( const ofaIDBExerciceMeta *period, const GDate *begin, const GDate *end )
{
	sIDBMeta *sdata;
	gboolean begin_ok, end_ok;

	begin_ok = FALSE;
	end_ok = FALSE;
	sdata = get_instance_data( period );

	begin_ok = begin ? my_date_compare_ex( begin, &sdata->begin, TRUE ) : TRUE;
	end_ok = end ? my_date_compare_ex( end, &sdata->end, FALSE ) : TRUE;

	return( begin_ok && end_ok );
}

/**
 * ofa_idbexercice_meta_is_restorable:
 * @period: a #ofaIDBExerciceMeta instance.
 * @uri: a file containing an archive.
 *
 * Returns:
 * - %OFA_RESTORABLE_OK if @uri actually contains an archived exercice
 *    restorable on @period;
 *
 * - %OFA_RESTORABLE_RPID if the target @period is an archive and the
 *   @uri archive file does not contain an archive of this same dossier;
 *
 * - %OFA_RESTORABLE_DATE if the target @period exercice dates are not
 *   compatible with those contained in @uri archive;
 *
 * - %OFA_RESTORABLE_HEADER if the @uri does not contain a suitable header.
 */
guint
ofa_idbexercice_meta_is_restorable( const ofaIDBExerciceMeta *period, const gchar *uri )
{
	return( OFA_RESTORABLE_HEADER );
}

/**
 * ofa_idbexercice_meta_update_settings:
 * @period: this #ofaIDBExerciceMeta instance.
 *
 * Write meta datas to dossier settings.
 */
void
ofa_idbexercice_meta_update_settings( const ofaIDBExerciceMeta *period )
{
	static const gchar *thisfn = "ofa_idbexercice_meta_update_settings";

	g_return_if_fail( period && OFA_IS_IDBEXERCICE_META( period ));

	write_settings( period );

	if( OFA_IDBEXERCICE_META_GET_INTERFACE( period )->update_settings ){
		OFA_IDBEXERCICE_META_GET_INTERFACE( period )->update_settings( period );
		return;
	}

	g_info( "%s: ofaIDBExerciceMeta's %s implementation does not provide 'update_settings() method",
			thisfn, G_OBJECT_TYPE_NAME( period ));
}

/**
 * ofa_idbexercice_meta_delete:
 * @period: this #ofaIDBExerciceMeta instance.
 * @connect: a #ofaIDBConnect superuser connection on the DBMS.
 * @msgerr: [out][allow-none]: a placeholder for an error message.
 *
 * Delete the @instance from the DBMS.
 * Update the settings accordingly.
 *
 * Returns: %TRUE if deletion has been sucessful.
 */
gboolean
ofa_idbexercice_meta_delete( ofaIDBExerciceMeta *period, ofaIDBConnect *connect, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_idbexercice_meta_delete";
	myISettings *settings;
	const gchar *group;
	sIDBMeta *sdata;
	gboolean ok;

	g_return_val_if_fail( period && OFA_IS_IDBEXERCICE_META( period ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	ok = TRUE;
	sdata = get_instance_data( period );

	settings = ofa_idbdossier_meta_get_settings_iface( sdata->dossier_meta );
	group = ofa_idbdossier_meta_get_settings_group( sdata->dossier_meta );
	my_isettings_remove_key( settings, group, sdata->settings_key );

	if( OFA_IDBEXERCICE_META_GET_INTERFACE( period )->delete ){
		ok = OFA_IDBEXERCICE_META_GET_INTERFACE( period )->delete( period, connect, msgerr );
	} else {
		g_info( "%s: ofaIDBExerciceMeta's %s implementation does not provide 'delete() method",
				thisfn, G_OBJECT_TYPE_NAME( period ));
	}

	return( ok );
}

/**
 * ofa_idbexercice_meta_dump:
 * @period: this #ofaIDBExerciceMeta instance.
 *
 * Dump the object.
 */
void
ofa_idbexercice_meta_dump( const ofaIDBExerciceMeta *period )
{
	static const gchar *thisfn = "ofa_idbexercice_meta_dump";
	sIDBMeta *sdata;
	gchar *begin, *end;

	g_return_if_fail( period && OFA_IS_IDBEXERCICE_META( period ));

	sdata = get_instance_data( period );
	begin = my_date_to_str( &sdata->begin, MY_DATE_SQL );
	end = my_date_to_str( &sdata->end, MY_DATE_SQL );

	g_debug( "%s: period=%p (%s)",
			thisfn, ( void * ) period, G_OBJECT_TYPE_NAME( period ));
	g_debug( "%s:   dossier_meta=%p", thisfn, sdata->dossier_meta );
	g_debug( "%s:   settings_key=%s", thisfn, sdata->settings_key );
	g_debug( "%s:   settings_id=%s", thisfn, sdata->settings_id );
	g_debug( "%s:   begin=%s", thisfn, begin );
	g_debug( "%s:   end=%s", thisfn, end );
	g_debug( "%s:   current=%s", thisfn, sdata->current ? "True":"False" );
	g_debug( "%s:   ref_count=%u", thisfn, G_OBJECT( period )->ref_count );

	g_free( begin );
	g_free( end );

	if( OFA_IDBEXERCICE_META_GET_INTERFACE( period )->dump ){
		OFA_IDBEXERCICE_META_GET_INTERFACE( period )->dump( period );
	}
}

/*
 * settings are: "begin(s), end(s); current(s);"
 */
static void
read_settings( ofaIDBExerciceMeta *self )
{
	sIDBMeta *sdata;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *group, *cstr;

	sdata = get_instance_data( self );

	settings = ofa_idbdossier_meta_get_settings_iface( sdata->dossier_meta );
	group = ofa_idbdossier_meta_get_settings_group( sdata->dossier_meta );
	strlist = my_isettings_get_string_list( settings, group, sdata->settings_key );

	/* beginning date as YYYYMMDD */
	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &sdata->begin, cstr, MY_DATE_YYMD );
	}

	/* ending date as YYYYMMDD */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &sdata->end, cstr, MY_DATE_YYMD );
	}

	/* is_current */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		sdata->current = my_utils_boolean_from_str( cstr );
	}

	my_isettings_free_string_list( settings, strlist );
}

static void
write_settings( const ofaIDBExerciceMeta *self )
{
	sIDBMeta *sdata;
	myISettings *settings;
	const gchar *group;
	gchar *sbegin, *send, *str;

	sdata = get_instance_data( self );

	sbegin = my_date_is_valid( &sdata->begin ) ? my_date_to_str( &sdata->begin, MY_DATE_YYMD ) : g_strdup( "" );
	send = my_date_is_valid( &sdata->end ) ? my_date_to_str( &sdata->end, MY_DATE_YYMD ) : g_strdup( "" );

	str = g_strdup_printf( "%s;%s;%s;",
				sbegin,
				send,
				sdata->current ? "True":"False" );

	settings = ofa_idbdossier_meta_get_settings_iface( sdata->dossier_meta );
	group = ofa_idbdossier_meta_get_settings_group( sdata->dossier_meta );

	my_isettings_set_string( settings, group, sdata->settings_key, str );

	g_free( sbegin );
	g_free( send );
	g_free( str );
}

static sIDBMeta *
get_instance_data( const ofaIDBExerciceMeta *self )
{
	sIDBMeta *sdata;

	sdata = ( sIDBMeta * ) g_object_get_data( G_OBJECT( self ), IDBEXERCICE_META_DATA );

	if( !sdata ){
		sdata = g_new0( sIDBMeta, 1 );
		g_object_set_data( G_OBJECT( self ), IDBEXERCICE_META_DATA, sdata );
		g_object_weak_ref( G_OBJECT( self ), ( GWeakNotify ) on_instance_finalized, sdata );

		my_date_clear( &sdata->begin );
		my_date_clear( &sdata->end );
	}

	return( sdata );
}

static void
on_instance_finalized( sIDBMeta *sdata, GObject *finalized_period )
{
	static const gchar *thisfn = "ofa_idbexercice_meta_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_period=%p", thisfn, ( void * ) sdata, ( void * ) finalized_period );

	g_free( sdata->settings_key );
	g_free( sdata->settings_id );

	g_free( sdata );
}
