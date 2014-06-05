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

#include <string.h>

#include "api/ofa-iimporter.h"
#include "ui/ofa-importer.h"
#include "ui/ofa-plugin.h"
#include "ui/ofo-bat.h"
#include "ui/ofo-bat-line.h"
#include "ui/ofo-dossier.h"

static gint try_to_import_uri( const ofoDossier *dossier, GList *modules, ofaIImporterParms *parms );
static gint insert_imported_bat_v1( const ofoDossier *dossier, const gchar *uri, ofaIImporterBatv1 *batv1 );

/**
 * ofa_importer_import_from_uri:
 *
 * Returns: the internal identifier of the imported file in the
 * OFA_T_IMPORT_BAT table
 */
gint
ofa_importer_import_from_uri( const ofoDossier *dossier, const gchar *uri )
{
	static const gchar *thisfn = "ofa_importer_import_from_uri";
	GList *modules;
	ofaIImporterParms parms;
	gint id;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), -1 );

	g_debug( "%s: dossier=%p, uri=%s", thisfn, ( void * ) dossier, uri );

	if( !uri || !g_utf8_strlen( uri, -1 )){
		return( -1 );
	}

	memset( &parms, '\0', sizeof( ofaIImporterParms ));
	parms.uri = ( const gchar * ) uri;
	parms.messages = NULL;

	modules = ofa_plugin_get_extensions_for_type( OFA_TYPE_IIMPORTER );

	id = try_to_import_uri( dossier, modules, &parms );

	ofa_plugin_free_extensions( modules );

	return( id );
}

/**
 * ofa_importer_import_from_uris:
 *
 * Returns: the count of successfully imported URIs
 */
guint
ofa_importer_import_from_uris( const ofoDossier *dossier, GSList *uris )
{
	static const gchar *thisfn = "ofa_importer_import_from_uris";
	GList *modules;
	ofaIImporterParms parms;
	GSList *uri;
	guint count;

	g_debug( "%s: uris=%p", thisfn, ( void * ) uris );

	count = 0;
	memset( &parms, '\0', sizeof( ofaIImporterParms ));
	parms.messages = NULL;

	modules = ofa_plugin_get_extensions_for_type( OFA_TYPE_IIMPORTER );

	for( uri=uris ; uri ; uri=uri->next ){

		parms.uri = ( const gchar * ) uri->data;
		if( try_to_import_uri( dossier, modules, &parms ) > 0 ){
			count += 1;
		}
	}

	ofa_plugin_free_extensions( modules );

	return( count );
}

/*
 * propose the uri to each managing object until one be successful
 *
 * Returns the internal id of the imported file, or -1.
 */
static gint
try_to_import_uri( const ofoDossier *dossier, GList *modules, ofaIImporterParms *parms )
{
	static const gchar *thisfn = "ofa_importer_try_to_import_uri";
	GList *im;
	guint code;
	gint id;

	id = -1;

	for( im=modules ; im ; im=im->next ){

		code = ofa_iimporter_import_from_uri( OFA_IIMPORTER( im->data ), parms );
		if( code == IMPORTER_CODE_OK ){
			switch( parms->type ){

				case IMPORTER_TYPE_BAT1:
					id = insert_imported_bat_v1( dossier, parms->uri, &parms->batv1 );
					break;

				default:
					g_warning( "%s: TO BE WRITTEN", thisfn );
			}
			break;
		}
	}

	return( id );
}

static gint
insert_imported_bat_v1( const ofoDossier *dossier, const gchar *uri, ofaIImporterBatv1 *batv1 )
{
	ofoBat *bat;
	ofoBatLine *batline;
	ofaIImporterSBatv1 *str;
	GList *line;
	gint id;

	bat = ofo_bat_new();

	ofo_bat_set_uri( bat, uri );
	ofo_bat_set_format( bat, batv1->format );
	ofo_bat_set_count( bat, batv1->count );
	ofo_bat_set_begin( bat, &batv1->begin );
	ofo_bat_set_end( bat, &batv1->end );
	ofo_bat_set_solde( bat, batv1->solde );
	ofo_bat_set_solde_set( bat, batv1->solde_set );
	ofo_bat_set_rib( bat, batv1->rib );
	ofo_bat_set_currency( bat, batv1->currency );

	if( !ofo_bat_insert( bat, dossier )){
		g_clear_object( &bat );
		return( -1 );
	}

	id = ofo_bat_get_id( bat );

	for( line=batv1->results ; line ; line=line->next ){

		str = ( ofaIImporterSBatv1 * ) line->data;
		batline = ofo_bat_line_new( id );

		ofo_bat_line_set_valeur( batline, &str->dvaleur );
		ofo_bat_line_set_ope( batline, &str->dope );
		ofo_bat_line_set_ref( batline, str->ref );
		ofo_bat_line_set_label( batline, str->label );
		ofo_bat_line_set_montant( batline, str->amount );
		ofo_bat_line_set_currency( batline, str->currency );

		ofo_bat_line_insert( batline, dossier );
	}

	return( id );
}
