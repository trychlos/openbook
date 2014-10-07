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

#ifndef __OPENBOOK_API_OFA_IIMPORTER_H__
#define __OPENBOOK_API_OFA_IIMPORTER_H__

/**
 * SECTION: iimporter
 * @title: ofaIImporter
 * @short_description: The Import Interface
 * @include: openbook/ofa-iimporter.h
 *
 * The #ofaIImporter interface imports items from the outside world
 * into &prodname;.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_IIMPORTER                      ( ofa_iimporter_get_type())
#define OFA_IIMPORTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IIMPORTER, ofaIImporter ))
#define OFA_IS_IIMPORTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IIMPORTER ))
#define OFA_IIMPORTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IIMPORTER, ofaIImporterInterface ))

typedef struct _ofaIImporter                    ofaIImporter;
typedef struct _ofaIImporterParms               ofaIImporterParms;

/**
 * ofaIImporterInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @import_from_uri:       [should] imports a file.
 *
 * This defines the interface that an #ofaIImporter should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIImporter provider.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIImporter interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaIImporter *instance );

	/**
	 * import_from_uri:
	 * @instance: the #ofaIImporter provider.
	 * @parms: an #ofaIImporterParms structure.
	 *
	 * Imports the content of a file.
	 *
	 * Return value: the return code of the operation.
	 */
	guint ( *import_from_uri )      ( const ofaIImporter *instance, ofaIImporterParms *parms );
}
	ofaIImporterInterface;

/**
 * ofaIImporterStatus:
 * @IMPORTER_CODE_OK:                import ok.
 * @IMPORTER_CODE_PROGRAM_ERROR:     a program error has been detected.
 *                                   You should open a bug in
 *                                   <ulink url="https://bugzilla.gnome.org/enter_bug.cgi?product=openbook">Bugzilla</ulink>.
 * @IMPORTER_CODE_NOT_WILLING_TO:    the plugin is not willing to import the uri.
 * @IMPORTER_CODE_UNKNOWN_FORMAT:    unknown format or not a regular file.
 *
 * Defines the return status of the import operation.
 */
typedef enum {
	IMPORTER_CODE_OK = 0,
	IMPORTER_CODE_PROGRAM_ERROR,
	IMPORTER_CODE_NOT_WILLING_TO,
	IMPORTER_CODE_UNABLE_TO_PARSE
}
	ofaIImporterStatus;

typedef enum {
	IMPORTER_TYPE_BAT = 1,
	IMPORTER_TYPE_CLASS,
	IMPORTER_TYPE_ACCOUNT,
	IMPORTER_TYPE_CURRENCY,
	IMPORTER_TYPE_LEDGER,
	IMPORTER_TYPE_MODEL,
	IMPORTER_TYPE_RATE,
	IMPORTER_TYPE_ENTRY
}
	ofaIImporterType;

/**
 * ofaIImporterBatv1:
 * This structure is used when importing a bank account transaction list.
 * All data members are to be set on output (no input data here).
 */
typedef struct {
	gint     count;
	GDate    begin;
	GDate    end;
	gchar   *rib;
	gchar   *currency;
	gdouble  solde;
	gboolean solde_set;
	GList   *results;					/* list of ofaIImporterSBatv1 structs */
}
	ofaIImporterBatv1;

typedef struct {
						/* bourso                 lcl */
						/* excel95 excel2002 excel_tabulated */
	GDate   dope;		/*   X         X */
	GDate   dvaleur;	/*   X         X           X */
	gchar  *ref;		/*                         X */
	gchar  *label;		/*   X         X           X */
	gdouble amount;		/*   X         X           X */
	gchar  *currency;	/*   X         X */
}
	ofaIImporterSBatv1;

/**
 * ofaIImporterRefv1:
 * This structure is used when importing a reference table in csv format.
 * All data members are to be set on output (no input data here).
 */
typedef struct {
	gint     count;
}
	ofaIImporterRefv1;

/**
 * ofaIImporterParms:
 * @uri:      [in]
 * @messages: [in/out]
 * @type:     [out]    the type of imported data, to be set by the
 *                     importer
 * @v1:      [out]     the BAT v1 parameters, if @type is BAT1
 *
 * This structure allows to pass all needed parameters when importing
 * from an URI, taking back all the results on output.
 */
struct _ofaIImporterParms {
	const gchar *uri;
	GSList      *messages;
	gint         type;
	gint         version;
	gchar       *format;
	union {
		ofaIImporterBatv1 batv1;
		ofaIImporterRefv1 refv1;
	};
};

GType ofa_iimporter_get_type       ( void );

guint ofa_iimporter_import_from_uri( const ofaIImporter *importer, ofaIImporterParms *parms );

void  ofa_iimporter_free_output    ( ofaIImporterParms *parms );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IIMPORTER_H__ */
