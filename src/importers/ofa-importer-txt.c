/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include <gio/gio.h>

#include "my/my-utils.h"

#include "importers/ofa-importer-txt.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaImporterTxtPrivate;

G_DEFINE_TYPE_EXTENDED( ofaImporterTxt, ofa_importer_txt, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaImporterTxt ))

static void
importer_txt_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_importer_txt_finalize";

	g_return_if_fail( instance && OFA_IS_IMPORTER_TXT( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_txt_parent_class )->finalize( instance );
}

static void
importer_txt_dispose( GObject *instance )
{
	ofaImporterTxtPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORTER_TXT( instance ));

	priv = ofa_importer_txt_get_instance_private( OFA_IMPORTER_TXT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_txt_parent_class )->dispose( instance );
}

static void
ofa_importer_txt_init( ofaImporterTxt *self )
{
	static const gchar *thisfn = "ofa_importer_txt_init";
	ofaImporterTxtPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORTER_TXT( self ));

	priv = ofa_importer_txt_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_importer_txt_class_init( ofaImporterTxtClass *klass )
{
	static const gchar *thisfn = "ofa_importer_txt_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = importer_txt_dispose;
	G_OBJECT_CLASS( klass )->finalize = importer_txt_finalize;
}

/**
 * ofa_importer_txt_is_willing_to:
 * @instance: a #ofaImporterTxt instance.
 * @uri: the uri to the filename to be imported.
 * @accepted_contents: the #GList of the accepted mimetypes.
 *
 * Returns: %TRUE if the guessed content of the @uri is listed in the
 * @accepted_contents.
 */
gboolean
ofa_importer_txt_is_willing_to( const ofaImporterTxt *instance, const gchar *uri, const GList *accepted_contents )
{
	gchar *filename, *content;
	gboolean ok;

	filename = g_filename_from_uri( uri, NULL, NULL );
	content = g_content_type_guess( filename, NULL, 0, NULL );

	ok = my_utils_str_in_list( content, accepted_contents );

	g_free( content );
	g_free( filename );

	return( ok );
}
