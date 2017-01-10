/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
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

#ifndef __OPENBOOK_API_OFA_JSON_HEADER_H__
#define __OPENBOOK_API_OFA_JSON_HEADER_H__

/**
 * SECTION: ofahub
 * @title: ofaJsonHeader
 * @short_description: The #ofaJsonHeader Class Definition
 * @include: openbook/ofa-json-header.h
 *
 * The #ofaJsonHeader class manages the JSON header inserted on top of
 * backup files. It identifies the database stored after.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_JSON_HEADER                ( ofa_json_header_get_type())
#define OFA_JSON_HEADER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_JSON_HEADER, ofaJsonHeader ))
#define OFA_JSON_HEADER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_JSON_HEADER, ofaJsonHeaderClass ))
#define OFA_IS_JSON_HEADER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_JSON_HEADER ))
#define OFA_IS_JSON_HEADER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_JSON_HEADER ))
#define OFA_JSON_HEADER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_JSON_HEADER, ofaJsonHeaderClass ))

typedef struct _ofaJsonHeader               ofaJsonHeader;

struct _ofaJsonHeader {
	/*< public members >*/
	GObject      parent;
};

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaJsonHeaderClass;

GType           ofa_json_header_get_type            ( void ) G_GNUC_CONST;

ofaJsonHeader  *ofa_json_header_new                 ( void );

gboolean        ofa_json_header_get_is_current      ( ofaJsonHeader *header );

void            ofa_json_header_set_is_current      ( ofaJsonHeader *header,
														gboolean is_current );

const GDate    *ofa_json_header_get_begin_date      ( ofaJsonHeader *header );

void            ofa_json_header_set_begin_date      ( ofaJsonHeader *header,
														const GDate *date );

const GDate    *ofa_json_header_get_end_date        ( ofaJsonHeader *header );

void            ofa_json_header_set_end_date        ( ofaJsonHeader *header,
														const GDate *date );

const gchar    *ofa_json_header_get_openbook_version( ofaJsonHeader *header );

void            ofa_json_header_set_openbook_version( ofaJsonHeader *header,
														const gchar *version );

void            ofa_json_header_set_plugin          ( ofaJsonHeader *header,
														const gchar *canon_name,
														const gchar *display_name,
														const gchar *version );

void            ofa_json_header_set_dbmodel         ( ofaJsonHeader *header,
														const gchar *id,
														const gchar *version );

const gchar    *ofa_json_header_get_comment         ( ofaJsonHeader *header );

void            ofa_json_header_set_comment         ( ofaJsonHeader *header,
														const gchar *comment );

const GTimeVal *ofa_json_header_get_current_stamp   ( ofaJsonHeader *header );

void            ofa_json_header_set_current_stamp   ( ofaJsonHeader *header,
														const GTimeVal *stamp );

const gchar    *ofa_json_header_get_current_user    ( ofaJsonHeader *header );

void            ofa_json_header_set_current_user    ( ofaJsonHeader *header,
														const gchar *userid );

gchar          *ofa_json_header_get_string          ( ofaJsonHeader *header );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_JSON_HEADER_H__ */
