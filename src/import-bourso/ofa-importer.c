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
struct _ofaBoursoImporterPrivate {
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
	gboolean   (*fnTest)  ( ofaBoursoImporter * );
	ofsBat *   (*fnImport)( ofaBoursoImporter * );
}
	ImportFormat;

static gboolean bourso_excel95_v1_check( ofaBoursoImporter *bourso_importer );
static ofsBat  *bourso_excel95_v1_import( ofaBoursoImporter *bourso_importer );
static gboolean bourso_excel2002_v1_check( ofaBoursoImporter *bourso_importer );
static ofsBat  *bourso_excel2002_v1_import( ofaBoursoImporter *bourso_importer );

static ImportFormat st_import_formats[] = {
		{ "Boursorama - Excel 95",        1, bourso_excel95_v1_check,     bourso_excel95_v1_import },
		{ "Boursorama - Excel 2002",      1, bourso_excel2002_v1_check,   bourso_excel2002_v1_import },
		{ 0 }
};

static GType         st_module_type = 0;
static GObjectClass *st_parent_class = NULL;

static void         class_init( ofaBoursoImporterClass *klass );
static void         instance_init( GTypeInstance *instance, gpointer klass );
static void         instance_dispose( GObject *object );
static void         instance_finalize( GObject *object );
static void         iimportable_iface_init( ofaIImportableInterface *iface );
static guint        iimportable_get_interface_version( const ofaIImportable *bourso_importer );
static gboolean     iimportable_is_willing_to( ofaIImportable *bourso_importer, const gchar *uri, const ofaFileFormat *settings, void **ref, guint *count );
static guint        iimportable_import_uri( ofaIImportable *bourso_importer, void *ref, const gchar *uri, const ofaFileFormat *settings, ofoDossier *dossier, void **imported_id );
static GSList      *get_file_content( ofaIImportable *bourso_importer, const gchar *uri );
static gboolean     bourso_tabulated_text_v1_check( ofaBoursoImporter *bourso_importer, const gchar *thisfn );
static ofsBat      *bourso_tabulated_text_v1_import( ofaBoursoImporter *importe, const gchar *thisfn );
static gchar       *bourso_strip_field( gchar *str );
static GDate       *scan_date_dmyy( GDate *date, const gchar *str );
static gdouble      get_double( const gchar *str );

GType
ofa_bourso_importer_get_type( void )
{
	return( st_module_type );
}

void
ofa_bourso_importer_register_type( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_bourso_importer_register_type";

	static GTypeInfo info = {
		sizeof( ofaBoursoImporterClass ),
		NULL,
		NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaBoursoImporter ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	static const GInterfaceInfo iimportable_iface_info = {
		( GInterfaceInitFunc ) iimportable_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	st_module_type = g_type_module_register_type( module, G_TYPE_OBJECT, "ofaBoursoImporter", &info, 0 );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IIMPORTABLE, &iimportable_iface_info );
}

static void
class_init( ofaBoursoImporterClass *klass )
{
	static const gchar *thisfn = "ofa_bourso_importer_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;

	g_type_class_add_private( klass, sizeof( ofaBoursoImporterPrivate ));
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_bourso_importer_instance_init";
	ofaBoursoImporter *self;

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	g_return_if_fail( instance && OFA_IS_BOURSO_IMPORTER( instance ));

	self = OFA_BOURSO_IMPORTER( instance );

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BOURSO_IMPORTER, ofaBoursoImporterPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *object )
{
	ofaBoursoImporterPrivate *priv;

	g_return_if_fail( object && OFA_IS_BOURSO_IMPORTER( object ));

	priv = OFA_BOURSO_IMPORTER( object )->priv;

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
	static const gchar *thisfn = "ofa_bourso_importer_instance_finalize";

	g_return_if_fail( object && OFA_IS_BOURSO_IMPORTER( object ));

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( object );
}

static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofa_bourso_importer_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->is_willing_to = iimportable_is_willing_to;
	iface->import_uri = iimportable_import_uri;
}

static guint
iimportable_get_interface_version( const ofaIImportable *bourso_importer )
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
iimportable_is_willing_to( ofaIImportable *bourso_importer, const gchar *uri, const ofaFileFormat *settings, void **ref, guint *count )
{
	static const gchar *thisfn = "ofa_bourso_importer_iimportable_is_willing_to";
	ofaBoursoImporterPrivate *priv;
	gint i;
	gboolean ok;

	g_debug( "%s: bourso_importer=%p, uri=%s, settings=%p, count=%p",
			thisfn, ( void * ) bourso_importer, uri, ( void * ) settings, ( void * ) count );

	priv = OFA_BOURSO_IMPORTER( bourso_importer )->priv;
	ok = FALSE;

	priv->lines = get_file_content( bourso_importer, uri );
	priv->settings = settings;

	for( i=0 ; st_import_formats[i].label ; ++i ){
		if( st_import_formats[i].fnTest( OFA_BOURSO_IMPORTER( bourso_importer ))){
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
iimportable_import_uri( ofaIImportable *bourso_importer, void *ref, const gchar *uri, const ofaFileFormat *settings, ofoDossier *dossier, void **imported_id )
{
	static const gchar *thisfn = "ofa_bourso_importer_iimportable_import_uri";
	ofaBoursoImporterPrivate *priv;
	gint idx;
	ofsBat *bat;

	g_debug( "%s: bourso_importer=%p, ref=%p, uri=%s, settings=%p, dossier=%p",
			thisfn, ( void * ) bourso_importer, ref,
			uri, ( void * ) settings, ( void * ) dossier );

	priv = OFA_BOURSO_IMPORTER( bourso_importer )->priv;

	priv->lines = get_file_content( bourso_importer, uri );
	priv->settings = settings;
	priv->dossier = dossier;

	idx = GPOINTER_TO_INT( ref );
	bat = NULL;

	if( st_import_formats[idx].fnImport ){
		bat = st_import_formats[idx].fnImport( OFA_BOURSO_IMPORTER( bourso_importer ));
		if( bat ){
			bat->uri = g_strdup( uri );
			bat->format = g_strdup( st_import_formats[idx].label );
			ofo_bat_import( bourso_importer, bat, dossier, ( ofxCounter ** ) imported_id );
			ofs_bat_free( bat );
		}
	}

	g_slist_free_full( priv->lines, ( GDestroyNotify ) g_free );

	return( priv->errors );
}

static GSList *
get_file_content( ofaIImportable *bourso_importer, const gchar *uri )
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
 * -----------------
 * "*** Période : 01/01/2014 - 01/06/2014"
 * "*** Compte : 40618-80264-00040200033    -EUR "
 *
 * "DATE OPERATION"        "DATE VALEUR"   "LIBELLE"       "MONTANT"       "DEVISE"
 * " 02/01/2014"   " 02/01/2014"   "*PRLV Cotisat. Boursorama Protection 0  "      -00000000001,50 "EUR "
 * " 10/01/2014"   " 10/01/2014"   "TIP CFAB COMPTE REGLEMENT TI            "      -00000000220,02 "EUR "
 *
 * where spaces are tabulations
 */
static gboolean
bourso_excel95_v1_check( ofaBoursoImporter *bourso_importer )
{
	static const gchar *thisfn = "ofa_bourso_importer_bourso_excel95_v1_check";

	return( bourso_tabulated_text_v1_check( bourso_importer, thisfn ));
}

static ofsBat *
bourso_excel95_v1_import( ofaBoursoImporter *bourso_importer )
{
	static const gchar *thisfn = "ofa_bourso_importer_bourso_excel95_v1_import";

	return( bourso_tabulated_text_v1_import( bourso_importer, thisfn ));
}

/*
 * note these definitions are only for consistancy
 * if bourso_excel95 format works fine on the input file, these
 * functions will never be called
 */
static gboolean
bourso_excel2002_v1_check( ofaBoursoImporter *bourso_importer )
{
	static const gchar *thisfn = "ofa_bourso_importer_bourso_excel2002_v1_check";

	return( bourso_tabulated_text_v1_check( bourso_importer, thisfn ));
}

static ofsBat *
bourso_excel2002_v1_import( ofaBoursoImporter *bourso_importer )
{
	static const gchar *thisfn = "ofa_bourso_importer_bourso_excel2002_v1_import";

	return( bourso_tabulated_text_v1_import( bourso_importer, thisfn ));
}

static gboolean
bourso_tabulated_text_v1_check( ofaBoursoImporter *bourso_importer, const gchar *thisfn )
{
	ofaBoursoImporterPrivate *priv;
	GSList *line;
	const gchar *str;
	gchar *found;
	GDate date;

	priv = bourso_importer->priv;
	line = priv->lines;

	/* first line: "*** Période : dd/mm/yyyy - dd/mm/yyyy" */
	str = line->data;
	if( !g_str_has_prefix( str, "\"*** P" )){
		g_debug( "%s: no '*** P' prefix", thisfn );
		return( FALSE );
	}
	found = g_strstr_len( str, -1, "riode : " );
	if( !found ){
		g_debug( "%s: 'riode : ' not found", thisfn );
		return( FALSE );
	}
	/* dd/mm/yyyy - dd/mm/yyyy */
	if( !scan_date_dmyy( &date, found+8 )){
		g_debug( "%s: date at found+8 not valid: %s", thisfn, found+8 );
		return( FALSE );
	}
	if( !scan_date_dmyy( &date, found+21 )){
		g_debug( "%s: date at found+21 not valid: %s", thisfn, found+21 );
		return( FALSE );
	}

	/* second line: "*** Compte : 40618-80264-00040200033    -EUR " */
	line = line->next;
	str = line->data;
	if( !g_str_has_prefix( str, "\"*** Compte : " )){
		g_debug( "%s: no '*** Compte : ' prefix", thisfn );
		return( FALSE );
	}
	found = g_strstr_len( str+38, -1, " -" );
	if( !found ){
		g_debug( "%s: ' - ' not found", thisfn );
		return( FALSE );
	}

	/* third line: empty */
	line = line->next;
	str = line->data;
	if( strlen( str )){
		return( FALSE );
	}

	/* fourth line: "DATE OPERATION"        "DATE VALEUR"   "LIBELLE"       "MONTANT"       "DEVISE" */
	line = line->next;
	str = line->data;
	if( !g_ascii_strcasecmp( str, "\"DATE OPERATION\"        \"DATE VALEUR\"   \"LIBELLE\"       \"MONTANT\"       \"DEVISE\"" )){
		g_debug( "%s: headers not found", thisfn );
		return( FALSE );
	}

	/* if the four first lines are ok, we are found to suppose that we
	 * have identified the input file */
	g_debug( "%s: nblines=%d", thisfn, g_slist_length( priv->lines ));
	priv->count = g_slist_length( priv->lines )-5;

	return( TRUE );
}

static ofsBat *
bourso_tabulated_text_v1_import( ofaBoursoImporter *bourso_importer, const gchar *thisfn )
{
	ofaBoursoImporterPrivate *priv;
	GSList *line;
	const gchar *str;
	ofsBat *sbat;
	ofsBatDetail *sdet;
	gchar *found;
	gchar **tokens, **iter;
	gchar *msg, *sbegin, *send;

	priv = bourso_importer->priv;
	sbat = g_new0( ofsBat, 1 );
	priv->errors = 0;

	/* line 1: begin, end */
	str = g_slist_nth( priv->lines, 0 )->data;
	found = g_strstr_len( str, -1, "riode : " );
	g_return_val_if_fail( found, -1 );
	if( !scan_date_dmyy( &sbat->begin, found+8 )){
		return( FALSE );
	}
	if( !scan_date_dmyy( &sbat->end, found+21 )){
		return( FALSE );
	}

	/* line 2: rib, currency */
	str = g_slist_nth( priv->lines, 1 )->data;
	sbat->rib = g_strstrip( g_strndup( str+14, 24 ));
	found = g_strstr_len( str+38, -1, " -" );
	sbat->currency = g_strndup( found+2, 3 );

	if( ofo_bat_exists( priv->dossier, sbat->rib, &sbat->begin, &sbat->end )){
		sbegin = my_date_to_str( &sbat->begin, ofa_prefs_date_display());
		send = my_date_to_str( &sbat->end, ofa_prefs_date_display());
		msg = g_strdup_printf( _( "Already imported BAT file: RIB=%s, begin=%s, end=%s" ),
				sbat->rib, sbegin, send );
		ofa_iimportable_set_message( OFA_IIMPORTABLE( bourso_importer ), 2, IMPORTABLE_MSG_ERROR, msg );
		g_free( msg );
		g_free( sbegin );
		g_free( send );
		priv->errors += 1;
		ofs_bat_free( sbat );
		return( NULL );
	}

	/* entries start at line 5 (counting from 1) */
	line = g_slist_nth( priv->lines, 3 );

	while( TRUE ){
		line = line->next;
		if( !line || !my_strlen( line->data )){
			break;
		}
		sdet = g_new0( ofsBatDetail, 1 );
		tokens = g_strsplit( line->data, "\t", -1 );
		iter = tokens;
		ofa_iimportable_increment_progress( OFA_IIMPORTABLE( bourso_importer ), IMPORTABLE_PHASE_IMPORT, 1 );

		found = bourso_strip_field( *iter );
		scan_date_dmyy( &sdet->dope, found );
		g_free( found );

		iter += 1;
		found = bourso_strip_field( *iter );
		scan_date_dmyy( &sdet->deffect, found );
		g_free( found );

		iter += 1;
		found = bourso_strip_field( *iter );
		/*g_debug( "%s: found='%s'", thisfn, found );*/
		sdet->label = found;

		iter +=1 ;
		found = *iter;
		sdet->amount = get_double( found );
		/*g_debug( "%s: str='%s', amount=%lf", thisfn, found, sdet->amount );*/

		iter += 1;
		found = bourso_strip_field( *iter );
		/*g_debug( "%s: found='%s'", thisfn, found );*/
		sdet->currency = found;

		sbat->details = g_list_prepend( sbat->details, sdet );
		g_strfreev( tokens );
	}

	return( sbat );
}

static gchar *
bourso_strip_field( gchar *str )
{
	gchar *s1, *s2, *e1, *e2;
	gchar *dest;

	s1 = g_utf8_strchr( str, -1, '"' );
	s2 = g_utf8_find_next_char( s1, NULL );
	e1 = g_utf8_strrchr( s2, -1, '"' );
	e2 = g_utf8_find_prev_char( s2, e1 );

	dest = g_strndup( s2, e2-s2+1 );

	return( g_strstrip ( dest ));
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
	static const gchar *thisfn = "ofa_bourso_importer_get_double";
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
