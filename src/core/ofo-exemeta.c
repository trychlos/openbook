/*
 * A double-entry accounting application for professional services.
 *
 * Open Firm Accounting
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

#include "my/my-date.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-idbconnect.h"
#include "api/ofo-dossier.h"
#include "api/ofo-exemeta.h"

/* priv instance data
 */
typedef struct {
	gboolean       dispose_has_run;

	/* initialization
	 */
	ofaIDBConnect *connect;

	/* runtime
	 */
	GDate          begin_date;
	GDate          end_date;
	gboolean       is_current;
}
	ofoExemetaPrivate;

static gboolean do_read( ofoExemeta *self );

G_DEFINE_TYPE_EXTENDED( ofoExemeta, ofo_exemeta, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofoExemeta ))

static void
exemeta_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_exemeta_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFO_IS_EXEMETA( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_exemeta_parent_class )->finalize( instance );
}

static void
exemeta_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_exemeta_dispose";
	ofoExemetaPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = ofo_exemeta_get_instance_private( OFO_EXEMETA( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_object_unref( priv->connect );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_exemeta_parent_class )->dispose( instance );
}

static void
ofo_exemeta_init( ofoExemeta *self )
{
	static const gchar *thisfn = "ofo_exemeta_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_exemeta_class_init( ofoExemetaClass *klass )
{
	static const gchar *thisfn = "ofo_exemeta_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exemeta_dispose;
	G_OBJECT_CLASS( klass )->finalize = exemeta_finalize;
}

/**
 * ofo_exemeta_new:
 * @connect: a valid connection to the provider.
 *
 * Instanciates a new object, and initializes it with data read from
 * database.
 *
 * Returns: a newly allocated #ofoExemeta object, or %NULL if an error
 * has occured.
 */
ofoExemeta *
ofo_exemeta_new( ofaIDBConnect *connect )
{
	ofoExemeta *meta;
	ofoExemetaPrivate *priv;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	meta = g_object_new( OFO_TYPE_EXEMETA, NULL );

	priv = ofo_exemeta_get_instance_private( meta );

	priv->connect = g_object_ref( connect );

	if( !do_read( meta )){
		g_clear_object( &meta );
	}

	return( meta );
}

static gboolean
do_read( ofoExemeta *self )
{
	ofoExemetaPrivate *priv;
	gboolean ok;
	gchar *query;
	GSList *result, *irow, *icol;

	priv = ofo_exemeta_get_instance_private( self );

	query = g_strdup_printf(
			"SELECT DOS_EXE_BEGIN, DOS_EXE_END, DOS_CURRENT "
			"	FROM OFA_T_DOSSIER WHERE DOS_ID=%u", DOSSIER_ROW_ID );

	ok = ofa_idbconnect_query_ex( priv->connect, query, &result, TRUE );

	if( ok ){
		irow = result;
		icol = ( GSList * ) irow->data;
		my_date_set_from_sql( &priv->begin_date, ( const gchar * ) icol->data );
		icol = icol->next;
		my_date_set_from_sql( &priv->end_date, ( const gchar * ) icol->data );
		icol = icol->next;
		priv->is_current = my_utils_boolean_from_str(( const gchar * ) icol->data );
		ofa_idbconnect_free_results( result );
	}

	g_free( query );

	return( ok );
}

/**
 * ofo_exemeta_get_exe_begin:
 * @meta: this #ofoExemeta instance.
 *
 * Returns: the beginning date of the exercice.
 */
const GDate *
ofo_exemeta_get_begin_date( ofoExemeta *meta )
{
	ofoExemetaPrivate *priv;

	g_return_val_if_fail( meta && OFO_IS_EXEMETA( meta ), NULL );

	priv = ofo_exemeta_get_instance_private( meta );

	return(( const GDate * ) &priv->begin_date );
}

/**
 * ofo_exemeta_get_exe_end:
 * @meta: this #ofoExemeta instance.
 *
 * Returns: the ending date of the exercice.
 */
const GDate *
ofo_exemeta_get_end_date( ofoExemeta *meta )
{
	ofoExemetaPrivate *priv;

	g_return_val_if_fail( meta && OFO_IS_EXEMETA( meta ), NULL );

	priv = ofo_exemeta_get_instance_private( meta );

	return(( const GDate * ) &priv->end_date );
}

/**
 * ofo_exemeta_get_current:
 * @meta: this #ofoExemeta instance.
 *
 * Returns: %TRUE if the exercice is current.
 */
gboolean
ofo_exemeta_get_current( ofoExemeta *meta )
{
	ofoExemetaPrivate *priv;

	g_return_val_if_fail( meta && OFO_IS_EXEMETA( meta ), FALSE );

	priv = ofo_exemeta_get_instance_private( meta );

	return( priv->is_current );
}

/**
 * ofo_exemeta_set_current:
 * @meta: this #ofoExemeta instance.
 * @current: whether this exercice is current.
 *
 * Set the @current indicator and update the DBMS accordingly.
 *
 * Returns: %TRUE if the update has been successful.
 */
gboolean
ofo_exemeta_set_current( ofoExemeta *meta, gboolean current )
{
	ofoExemetaPrivate *priv;
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( meta && OFO_IS_EXEMETA( meta ), FALSE );

	priv = ofo_exemeta_get_instance_private( meta );

	priv->is_current = current;

	query = g_strdup_printf(
			"UPDATE OFA_T_DOSSIER SET DOS_CURRENT='%s' WHERE DOS_ID=%u",
				current ? "Y":"N", DOSSIER_ROW_ID );

	ok = ofa_idbconnect_query( priv->connect, query, TRUE );

	g_free( query );

	return( ok );
}
