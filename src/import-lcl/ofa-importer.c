/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <api/my-date.h>
#include <api/my-utils.h>
#include <api/ofa-file-format.h>
#include <api/ofa-iimportable.h>
#include "api/ofa-preferences.h"
#include <api/ofo-bat.h>

#include "ofa-importer.h"

/* private instance data
 */
struct _ofaLCLImporterPrivate {
	gboolean             dispose_has_run;

	const ofaFileFormat *settings;
	ofoDossier          *dossier;
	GSList              *lines;
	guint                count;
	guint                errors;

};

/* a description of the import functions we are able to manage here
 */
typedef struct {
	const gchar *label;
	gint         version;
	gboolean   (*fnTest)  ( ofaLCLImporter * );
	ofsBat *   (*fnImport)( ofaLCLImporter * );
}
	ImportFormat;

static gboolean lcl_tabulated_text_v1_check( ofaLCLImporter *lcl_importer );
static ofsBat  *lcl_tabulated_text_v1_import( ofaLCLImporter *lcl_importer );

static ImportFormat st_import_formats[] = {
		{ "LCL - Excel (tabulated text)", 1, lcl_tabulated_text_v1_check, lcl_tabulated_text_v1_import },
		{ 0 }
};

static GType         st_module_type = 0;
static GObjectClass *st_parent_class = NULL;

static void         class_init( ofaLCLImporterClass *klass );
static void         instance_init( GTypeInstance *instance, gpointer klass );
static void         instance_dispose( GObject *object );
static void         instance_finalize( GObject *object );
static void         iimportable_iface_init( ofaIImportableInterface *iface );
static guint        iimportable_get_interface_version( const ofaIImportable *lcl_importer );
static gboolean     iimportable_is_willing_to( ofaIImportable *lcl_importer, const gchar *uri, const ofaFileFormat *settings, void **ref, guint *count );
static guint        iimportable_import_uri( ofaIImportable *lcl_importer, void *ref, const gchar *uri, const ofaFileFormat *settings, ofoDossier *dossier, void **imported_id );
static GSList      *get_file_content( ofaIImportable *lcl_importer, const gchar *uri );
static GDate       *scan_date_dmyy( GDate *date, const gchar *str );
static gboolean     lcl_tabulated_text_v1_check( ofaLCLImporter *lcl_importer );
static ofsBat      *lcl_tabulated_text_v1_import( ofaLCLImporter *lcl_importer );
static const gchar *lcl_get_ref_paiement( const gchar *str );
static gchar       *lcl_concatenate_labels( gchar ***iter );
static gdouble      get_double( const gchar *str );

GType
ofa_lcl_importer_get_type( void )
{
	return( st_module_type );
}

void
ofa_lcl_importer_register_type( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_lcl_importer_register_type";

	static GTypeInfo info = {
		sizeof( ofaLCLImporterClass ),
		NULL,
		NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaLCLImporter ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	static const GInterfaceInfo iimportable_iface_info = {
		( GInterfaceInitFunc ) iimportable_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	st_module_type = g_type_module_register_type( module, G_TYPE_OBJECT, "ofaLCLImporter", &info, 0 );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IIMPORTABLE, &iimportable_iface_info );
}

static void
class_init( ofaLCLImporterClass *klass )
{
	static const gchar *thisfn = "ofa_lcl_importer_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;

	g_type_class_add_private( klass, sizeof( ofaLCLImporterPrivate ));
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_lcl_importer_instance_init";
	ofaLCLImporter *self;

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	g_return_if_fail( instance && OFA_IS_LCL_IMPORTER( instance ));

	self = OFA_LCL_IMPORTER( instance );

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_LCL_IMPORTER, ofaLCLImporterPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *object )
{
	ofaLCLImporterPrivate *priv;

	g_return_if_fail( object && OFA_IS_LCL_IMPORTER( object ));

	priv = OFA_LCL_IMPORTER( object )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( object );
}

static void
instance_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_lcl_importer_instance_finalize";

	g_return_if_fail( object && OFA_IS_LCL_IMPORTER( object ));

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( object );
}

static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofa_lcl_importer_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->is_willing_to = iimportable_is_willing_to;
	iface->import_uri = iimportable_import_uri;
}

static guint
iimportable_get_interface_version( const ofaIImportable *lcl_importer )
{
	return( 1 );
}

/*
 * do the minimum to identify the file
 * as this moment, it should not be needed to make any charmap conversion
 *
 * Returns: %TRUE if willing to import.
 */
static gboolean
iimportable_is_willing_to( ofaIImportable *lcl_importer, const gchar *uri, const ofaFileFormat *settings, void **ref, guint *count )
{
	static const gchar *thisfn = "ofa_lcl_importer_iimportable_is_willing_to";
	ofaLCLImporterPrivate *priv;
	gint i;
	gboolean ok;

	g_debug( "%s: lcl_importer=%p, uri=%s, settings=%p, count=%p",
			thisfn, ( void * ) lcl_importer, uri, ( void * ) settings, ( void * ) count );

	priv = OFA_LCL_IMPORTER( lcl_importer )->priv;
	ok = FALSE;

	priv->lines = get_file_content( lcl_importer, uri );
	priv->settings = settings;

	for( i=0 ; st_import_formats[i].label ; ++i ){
		if( st_import_formats[i].fnTest( OFA_LCL_IMPORTER( lcl_importer ))){
			*ref = GINT_TO_POINTER( i );
			*count = priv->count;
			ok = TRUE;
			break;
		}
	}

	g_slist_free_full( priv->lines, ( GDestroyNotify ) g_free );

	return( ok );
}

/*
 * import the file
 */
static guint
iimportable_import_uri( ofaIImportable *lcl_importer, void *ref, const gchar *uri, const ofaFileFormat *settings, ofoDossier *dossier, void **imported_id )
{
	static const gchar *thisfn = "ofa_lcl_importer_iimportable_import_uri";
	ofaLCLImporterPrivate *priv;
	gint idx;
	ofsBat *bat;

	g_debug( "%s: lcl_importer=%p, ref=%p, uri=%s, settings=%p, dossier=%p",
			thisfn, ( void * ) lcl_importer, ref,
			uri, ( void * ) settings, ( void * ) dossier );

	priv = OFA_LCL_IMPORTER( lcl_importer )->priv;

	priv->lines = get_file_content( lcl_importer, uri );
	priv->settings = settings;
	priv->dossier = dossier;

	idx = GPOINTER_TO_INT( ref );
	bat = NULL;

	if( st_import_formats[idx].fnImport ){
		bat = st_import_formats[idx].fnImport( OFA_LCL_IMPORTER( lcl_importer ));
		if( bat ){
			bat->uri = g_strdup( uri );
			bat->format = g_strdup( st_import_formats[idx].label );
			ofo_bat_import( lcl_importer, bat, dossier, ( ofxCounter ** ) imported_id );
			ofs_bat_free( bat );
		}
	}

	g_slist_free_full( priv->lines, ( GDestroyNotify ) g_free );

	return( priv->errors );
}

static GSList *
get_file_content( ofaIImportable *lcl_importer, const gchar *uri )
{
	GFile *gfile;
	gchar *contents;
	gchar **lines, **iter;
	GSList *list;

	list = NULL;
	gfile = g_file_new_for_uri( uri );
	if( g_file_load_contents( gfile, NULL, &contents, NULL, NULL, NULL )){
		lines = g_strsplit( contents, "\n", -1 );
		g_free( contents );
		iter = lines;
		while( *iter ){
			list = g_slist_prepend( list, g_strstrip( g_strdup( *iter )));
			iter++;
		}
		g_strfreev( lines );
	}

	return( g_slist_reverse( list ));
}

/*
 * As of 2014- 6- 1:
 * ----------------
 * 17/04/2014 -> -80,0   -> Carte     ->->       CB  BUFFETTI STILO   15/04/14                          0       Divers
 * 18/04/2014 -> 10000,0 -> Virement  ->->-> VIREMENT WIESER
 * 23/04/2014 -> -12,0   -> Ch<E8>que -> 8341505 ->->->->->
 *
 * Last line is the balance of the account.
 *
 * Ref may be:
 *  'Carte'        -> 'CB'
 *  'Virement'     -> 'Vir.'
 *  'Prélèvement'  -> 'Pr.'
 *  'Chèque'       -> 'Ch.'
 *  'TIP'          -> 'TIP'
 *
 * There is an unknown field, maybe empty, most of time at zero
 *
 * The category may be empty. Most of time, it is set. When it is set,
 * it is always equal to 'Divers'.
 *
 * The unknown field and the category arealways set, or unset, together.
 */
static gboolean
lcl_tabulated_text_v1_check( ofaLCLImporter *lcl_importer )
{
	ofaLCLImporterPrivate *priv;
	gchar **tokens, **iter;
	GDate date;

	priv = lcl_importer->priv;
	tokens = g_strsplit( priv->lines->data, "\t", -1 );
	/* only interpret first line */
	/* first field = value date */
	iter = tokens;
	if( !scan_date_dmyy( &date, *iter )){
		return( FALSE );
	}
	iter += 1;
	if( !get_double( *iter )){
		return( FALSE );
	}
	/* other fields may be empty */

	priv->count = g_slist_length( priv->lines )-1;
	g_strfreev( tokens );

	return( TRUE );
}

static ofsBat  *
lcl_tabulated_text_v1_import( ofaLCLImporter *lcl_importer )
{
	ofaLCLImporterPrivate *priv;
	GSList *line;
	ofsBat *sbat;
	ofsBatDetail *sdet;
	gchar **tokens, **iter;
	gint count, nb;
	gchar *msg, *sbegin, *send;

	priv = lcl_importer->priv;
	line = priv->lines;
	sbat = g_new0( ofsBat, 1 );
	nb = g_slist_length( priv->lines );
	count = 0;
	priv->errors = 0;

	while( TRUE ){
		if( !line || !my_strlen( line->data )){
			break;
		}
		tokens = g_strsplit( line->data, "\t", -1 );
		iter = tokens;
		count += 1;

		/* detail line */
		if( count < nb ){
			sdet = g_new0( ofsBatDetail, 1 );
			ofa_iimportable_increment_progress( OFA_IIMPORTABLE( lcl_importer ), IMPORTABLE_PHASE_IMPORT, 1 );

			scan_date_dmyy( &sdet->deffect, *iter );

			iter += 1;
			sdet->amount = get_double( *iter );

			iter += 1;
			if( my_strlen( *iter )){
				sdet->ref = g_strdup( lcl_get_ref_paiement( *iter ));
			}

			iter += 1;
			sdet->label = lcl_concatenate_labels( &iter );

			/* do not interpret
			 * unknown field nor category */
			sbat->details = g_list_prepend( sbat->details, sdet );

		/* last line is the file footer */
		} else {
			scan_date_dmyy( &sbat->end, *iter );

			iter +=1 ;
			sbat->end_solde = get_double( *iter );
			sbat->end_solde_set = TRUE;

			iter += 1;
			/* no ref */

			iter += 1;
			sbat->rib = lcl_concatenate_labels( &iter );

			if( ofo_bat_exists( priv->dossier, sbat->rib, &sbat->begin, &sbat->end )){
				sbegin = my_date_to_str( &sbat->begin, ofa_prefs_date_display());
				send = my_date_to_str( &sbat->end, ofa_prefs_date_display());
				msg = g_strdup_printf( _( "Already imported BAT file: RIB=%s, begin=%s, end=%s" ),
						sbat->rib, sbegin, send );
				ofa_iimportable_set_message( OFA_IIMPORTABLE( lcl_importer ), nb, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( sbegin );
				g_free( send );
				priv->errors += 1;
				ofs_bat_free( sbat );
				sbat = NULL;
			}
		}

		g_strfreev( tokens );
		line = line->next;
	}

	return( sbat );
}

typedef struct {
	const gchar *bat_label;
	const gchar *ofa_label;
}
	lclPaiement;

static const lclPaiement st_lcl_paiements[] = {
		{ "Carte",       "CB" },
		{ "Virement",    "Vir" },
		{ "Prélèvement", "Prel" },
		{ "Chèque",      "Ch" },
		{ "TIP",         "TIP" },
		{ 0 }
};

static const gchar *
lcl_get_ref_paiement( const gchar *str )
{
	gint i;

	if( my_strlen( str )){
		for( i=0 ; st_lcl_paiements[i].bat_label ; ++ i ){
			if( !g_utf8_collate( str, st_lcl_paiements[i].bat_label )){
				return( st_lcl_paiements[i].ofa_label );
			}
		}
	}

	return( NULL );
}

/*
 * return a newly allocated stripped string
 */
static gchar *
lcl_concatenate_labels( gchar ***iter )
{
	GString *lab1;
	gchar *lab2;

	lab1 = g_string_new( "" );
	if( **iter ){
		lab2 = g_strdup( **iter );
		g_strstrip( lab2 );
		if( my_strlen( lab2 )){
			if( my_strlen( lab1->str )){
				lab1 = g_string_append( lab1, " " );
			}
			lab1 = g_string_append( lab1, lab2 );
		}
		g_free( lab2 );

		*iter += 1;
		if( **iter ){
			lab2 = g_strdup( **iter );
			g_strstrip( lab2 );
			if( my_strlen( lab2 )){
				if( my_strlen( lab1->str )){
					lab1 = g_string_append( lab1, " " );
				}
				lab1 = g_string_append( lab1, lab2 );
			}
			g_free( lab2 );

			*iter += 1;
			if( my_strlen( **iter )){
				lab2 = g_strdup( **iter );
				g_strstrip( lab2 );
				if( my_strlen( lab2 )){
					if( my_strlen( lab1->str )){
						lab1 = g_string_append( lab1, " " );
					}
					lab1 = g_string_append( lab1, lab2 );
				}
				g_free( lab2 );
			}
		}
	}

	g_strstrip( lab1->str );
	return( g_string_free( lab1, FALSE ));
}

/*
 * parse a 'dd/mm/yyyy' date
 */
static GDate *
scan_date_dmyy( GDate *date, const gchar *str )
{
	gint d, m, y;

	my_date_clear( date );
	sscanf( str, "%d/%d/%d", &d, &m, &y );
	if( d <= 0 || m <= 0 || y < 0 || d > 31 || m > 12 ){
		return( NULL );
	}
	g_date_set_dmy( date, d, m, y );
	if( !my_date_is_valid( date )){
		return( NULL );
	}

	return( date );
}

static gdouble
get_double( const gchar *str )
{
	static const gchar *thisfn = "ofa_lcl_importer_get_double";
	gdouble amount1, amount2;
	gdouble entier1, entier2;

	amount1 = g_ascii_strtod( str, NULL );
	entier1 = trunc( amount1 );
	if( entier1 == amount1 ){
		amount2 = strtod( str, NULL );
		entier2 = trunc( amount2 );
		if( entier2 == amount2 ){
			if( entier1 != entier2 ){
				g_warning( "%s: unable to get double from str='%s'", thisfn, str );
				return( 0 );
			}
		}
		return( amount2 );
	}
	return( amount1 );
}
