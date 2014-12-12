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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

#include "api/my-utils.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-iimportable.h"
#include "api/ofo-base-def.h"

/* data set against the imported object
 */
typedef struct {

	/* initialization
	 */
	const ofaFileFormat *settings;
	void                *caller;
	ofoDossier          *dossier;

	/* runtime data
	 */
	guint                count;			/* total count of lines to be imported */
	guint                progress;		/* import progression */
	guint                last_error;	/* last line number where an error occured */
	guint                insert;		/* insert progression */
}
	sIImportable;

#define IIMPORTABLE_LAST_VERSION        1
#define IIMPORTABLE_DATA                "ofa-iimportable-data"

static guint st_initializations = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaIImportableInterface *klass );
static void          interface_base_finalize( ofaIImportableInterface *klass );
static void          render_progress( ofaIImportable *importable, sIImportable *sdata, gint number, const gchar *signal );
static sIImportable *get_iimportable_data( ofaIImportable *importable );
static void          on_importable_weak_notify( sIImportable *sdata, GObject *finalized_object );

/**
 * ofa_iimportable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iimportable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iimportable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iimportable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIImportableInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIImportable", &info, 0 );

	g_type_interface_add_prerequisite( type, OFO_TYPE_BASE );

	return( type );
}

static void
interface_base_init( ofaIImportableInterface *klass )
{
	static const gchar *thisfn = "ofa_iimportable_interface_base_init";

	if( !st_initializations ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;

	if( st_initializations == 1 ){

		/* declare here the default implementations */
	}
}

static void
interface_base_finalize( ofaIImportableInterface *klass )
{
	static const gchar *thisfn = "ofa_iimportable_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iimportable_get_interface_last_version:
 * @instance: this #ofaIImportable instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iimportable_get_interface_last_version( void )
{
	return( IIMPORTABLE_LAST_VERSION );
}

/**
 * ofa_iimportable_import:
 * @importable: this #ofaIImportable instance.
 * @lines: the content of the imported file as a #GSList list of
 *  #GSList fields.
 * @settings: an #ofaFileFormat object.
 * @dossier: the current dossier.
 * @caller: the caller instance.
 *
 * Import the dataset from the named file.
 *
 * Returns: the count of found errors.
 */
gint
ofa_iimportable_import( ofaIImportable *importable,
									GSList *lines, const ofaFileFormat *settings,
									ofoDossier *dossier, void *caller )
{
	sIImportable *sdata;
	gint errors;

	g_return_val_if_fail( importable && OFA_IS_IIMPORTABLE( importable ), 0 );
	g_return_val_if_fail( OFO_IS_BASE( importable ), 0 );
	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), 0 );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );

	sdata = get_iimportable_data( importable );
	g_return_val_if_fail( sdata, 0 );

	errors = 0;
	sdata->settings = settings;
	sdata->caller = caller;
	sdata->dossier = dossier;
	sdata->count = g_slist_length( lines );

	if( OFA_IIMPORTABLE_GET_INTERFACE( importable )->import ){
		errors = OFA_IIMPORTABLE_GET_INTERFACE( importable )->import( importable, lines, sdata->dossier );
	}

	return( errors );
}

/**
 * ofa_iimportable_set_import_ok:
 * @importable: this #ofaIImportable instance.
 *
 * Increment the import progress count.
 *
 * This function should be called by the implementation each time a new
 * line is validated.
 */
void
ofa_iimportable_set_import_ok( ofaIImportable *importable )
{
	sIImportable *sdata;

	g_return_if_fail( OFA_IS_IIMPORTABLE( importable ));

	sdata = get_iimportable_data( importable );
	g_return_if_fail( sdata );

	sdata->progress += 1;
	render_progress( importable, sdata, sdata->progress, "progress" );
}

/**
 * ofa_iimportable_set_import_error:
 * @importable: this #ofaIImportable instance.
 * @line: the line count, counted from 1.
 * @msg: the error message.
 *
 * Send the error message to the caller.
 *
 * This function should be called by the implementation each time an
 * error is found in an imported line.
 */
void
ofa_iimportable_set_import_error( ofaIImportable *importable, guint line_number, const gchar *msg )
{
	sIImportable *sdata;

	g_return_if_fail( OFA_IS_IIMPORTABLE( importable ));

	sdata = get_iimportable_data( importable );
	g_return_if_fail( sdata );

	if( sdata->last_error != line_number ){
		sdata->progress = line_number;
	}

	render_progress( importable, sdata, sdata->progress, "progress" );

	if( OFA_IS_IIMPORTER( sdata->caller )){
		g_signal_emit_by_name( sdata->caller, "error", line_number, msg );
	}
}

/**
 * ofa_iimportable_set_insert_ok:
 * @importable: this #ofaIImportable instance.
 *
 * Increment the insert progress count.
 *
 * This function should be called by the implementation each time a new
 * line is inserted into the database.
 */
void
ofa_iimportable_set_insert_ok( ofaIImportable *importable )
{
	sIImportable *sdata;

	g_return_if_fail( OFA_IS_IIMPORTABLE( importable ));

	sdata = get_iimportable_data( importable );
	g_return_if_fail( sdata );

	sdata->insert += 1;
	render_progress( importable, sdata, sdata->insert, "insert" );
}

static void
render_progress( ofaIImportable *importable, sIImportable *sdata, gint number, const gchar *signal )
{
	gdouble progress;
	gchar *text;

	if( OFA_IS_IIMPORTER( sdata->caller )){
		progress = ( gdouble ) number / ( gdouble ) sdata->count;
		text = g_strdup_printf( "%u/%u", number, sdata->count );
		g_signal_emit_by_name( sdata->caller, signal, progress, text );
		g_free( text );
	}
}

static sIImportable *
get_iimportable_data( ofaIImportable *importable )
{
	sIImportable *sdata;

	sdata = ( sIImportable * ) g_object_get_data( G_OBJECT( importable ), IIMPORTABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sIImportable, 1 );
		g_object_set_data( G_OBJECT( importable ), IIMPORTABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( importable ), ( GWeakNotify ) on_importable_weak_notify, sdata );
	}

	return( sdata );
}

static void
on_importable_weak_notify( sIImportable *sdata, GObject *finalized_object )
{
	static const gchar *thisfn = "ofa_iimportable_on_importable_weak_notify";

	g_debug( "%s: sdata=%p, finalized_object=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_object );

	g_free( sdata );
}
