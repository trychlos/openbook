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

#ifndef __OFO_ACCOUNT_H__
#define __OFO_ACCOUNT_H__

/**
 * SECTION: ofo_account
 * @short_description: #ofoAccount class definition.
 * @include: ui/ofo-account.h
 *
 * This class implements the Account behavior, including the general
 * DB definition.
 *
 * Initialization with:
 * load data local infile '/home/pierre/data/pierre@wieser.fr@cloud.trychlos.org/GTD-TR/OLA01 - Specifications/Plan comptable notarial 1988 simplié.csv' into table OFA_T_COMPTES fields terminated by ';' (@dummy,CPT_NUMBER,CPT_LABEL,CPT_NOTES);
 */

#include "ui/ofa-sgbd.h"
#include "ui/ofo-base.h"

G_BEGIN_DECLS

#define OFO_TYPE_ACCOUNT                ( ofo_account_get_type())
#define OFO_ACCOUNT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_ACCOUNT, ofoAccount ))
#define OFO_ACCOUNT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_ACCOUNT, ofoAccountClass ))
#define OFO_IS_ACCOUNT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_ACCOUNT ))
#define OFO_IS_ACCOUNT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_ACCOUNT ))
#define OFO_ACCOUNT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_ACCOUNT, ofoAccountClass ))

typedef struct _ofoAccountClassPrivate  ofoAccountClassPrivate;

typedef struct {
	/*< private >*/
	ofoBaseClass            parent;
	ofoAccountClassPrivate *priv;
}
	ofoAccountClass;

typedef struct _ofoAccountPrivate       ofoAccountPrivate;

typedef struct {
	/*< private >*/
	ofoBase            parent;
	ofoAccountPrivate *priv;
}
	ofoAccount;

GType        ofo_account_get_type        ( void );

ofoAccount  *ofo_account_new             ( void );

GList       *ofo_account_load_chart      ( ofaSgbd *sgbd );
void         ofo_account_dump_chart      ( GList *chart );

gint         ofo_account_get_class       ( const ofoAccount *account );
const gchar *ofo_account_get_number      ( const ofoAccount *account );
const gchar *ofo_account_get_label       ( const ofoAccount *account );
gint         ofo_account_get_devise      ( const ofoAccount *account );
const gchar *ofo_account_get_notes       ( const ofoAccount *account );
const gchar *ofo_account_get_type_account( const ofoAccount *account );
gdouble      ofo_account_get_deb_mnt     ( const ofoAccount *account );
gint         ofo_account_get_deb_ecr     ( const ofoAccount *account );
const GDate *ofo_account_get_deb_date    ( const ofoAccount *account );
gdouble      ofo_account_get_cre_mnt     ( const ofoAccount *account );
gint         ofo_account_get_cre_ecr     ( const ofoAccount *account );
const GDate *ofo_account_get_cre_date    ( const ofoAccount *account );
gdouble      ofo_account_get_bro_deb_mnt ( const ofoAccount *account );
gint         ofo_account_get_bro_deb_ecr ( const ofoAccount *account );
const GDate *ofo_account_get_bro_deb_date( const ofoAccount *account );
gdouble      ofo_account_get_bro_cre_mnt ( const ofoAccount *account );
gint         ofo_account_get_bro_cre_ecr ( const ofoAccount *account );
const GDate *ofo_account_get_bro_cre_date( const ofoAccount *account );

gboolean     ofo_account_is_root         ( const ofoAccount *account );
gboolean     ofo_account_is_valid_data   ( const gchar *number, const gchar *label, gint devise, const gchar *type );

void         ofo_account_set_number      ( ofoAccount *account, const gchar *number );
void         ofo_account_set_label       ( ofoAccount *account, const gchar *label );
void         ofo_account_set_devise      ( ofoAccount *account, gint devise );
void         ofo_account_set_notes       ( ofoAccount *account, const gchar *notes );
void         ofo_account_set_type        ( ofoAccount *account, const gchar *type );
void         ofo_account_set_maj_user    ( ofoAccount *account, const gchar *user );
void         ofo_account_set_maj_stamp   ( ofoAccount *account, const GTimeVal *stamp );
void         ofo_account_set_deb_mnt     ( ofoAccount *account, gdouble mnt );
void         ofo_account_set_deb_ecr     ( ofoAccount *account, gint num );
void         ofo_account_set_deb_date    ( ofoAccount *account, const GDate *date );
void         ofo_account_set_cre_mnt     ( ofoAccount *account, gdouble mnt );
void         ofo_account_set_cre_ecr     ( ofoAccount *account, gint num );
void         ofo_account_set_cre_date    ( ofoAccount *account, const GDate *date );
void         ofo_account_set_bro_deb_mnt ( ofoAccount *account, gdouble mnt );
void         ofo_account_set_bro_deb_ecr ( ofoAccount *account, gint num );
void         ofo_account_set_bro_deb_date( ofoAccount *account, const GDate *date );
void         ofo_account_set_bro_cre_mnt ( ofoAccount *account, gdouble mnt );
void         ofo_account_set_bro_cre_ecr ( ofoAccount *account, gint num );
void         ofo_account_set_bro_cre_date( ofoAccount *account, const GDate *date );

gboolean     ofo_account_insert          ( ofoAccount *account, ofaSgbd *sgbd, const gchar *user );
gboolean     ofo_account_update          ( ofoAccount *account, ofaSgbd *sgbd, const gchar *user, const gchar *prev_number );
gboolean     ofo_account_delete          ( ofoAccount *account, ofaSgbd *sgbd, const gchar *user );

G_END_DECLS

#endif /* __OFO_ACCOUNT_H__ */
