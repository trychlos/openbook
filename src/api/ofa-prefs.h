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

#ifndef __OPENBOOK_API_OFA_PREFS_H__
#define __OPENBOOK_API_OFA_PREFS_H__

/**
 * SECTION: ofaprefs
 * @title: ofaPrefs
 * @short_description: The #ofaPrefs Class Definition
 * @include: openbook/ofa-prefs.h
 *
 * The #ofaPrefs class manages and maintains the user preferences which
 * are not related to a specific dossier.
 *
 * The #ofaPrefs object is an application-wide singleton.
 * It is available through the #ofaIGetter interface.
 */

#include "my/my-date.h"

#include "api/ofa-igetter-def.h"
#include "api/ofa-prefs-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_PREFS                ( ofa_prefs_get_type())
#define OFA_PREFS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PREFS, ofaPrefs ))
#define OFA_PREFS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PREFS, ofaPrefsClass ))
#define OFA_IS_PREFS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PREFS ))
#define OFA_IS_PREFS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PREFS ))
#define OFA_PREFS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PREFS, ofaPrefsClass ))

#if 0
typedef struct _ofaPrefs              ofaPrefs;
#endif

struct _ofaPrefs {
	/*< public members >*/
	GObject      parent;
};

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaPrefsClass;

GType        ofa_prefs_get_type                            ( void ) G_GNUC_CONST;

ofaPrefs    *ofa_prefs_new                                 ( ofaIGetter *getter );

gboolean     ofa_prefs_account_get_delete_with_children    ( ofaIGetter *getter );
gboolean     ofa_prefs_account_settle_warns_if_unbalanced  ( ofaIGetter *getter );
gboolean     ofa_prefs_account_settle_warns_unless_ctrl    ( ofaIGetter *getter );
gboolean     ofa_prefs_account_reconcil_warns_if_unbalanced( ofaIGetter *getter );
gboolean     ofa_prefs_account_reconcil_warns_unless_ctrl  ( ofaIGetter *getter );
void         ofa_prefs_account_set_user_settings           ( ofaIGetter *getter,
																	gboolean delete,
																	gboolean settle_warns,
																	gboolean settle_ctrl,
																	gboolean reconcil_warns,
																	gboolean reconcil_ctrl );

const gchar *ofa_prefs_amount_get_decimal_sep              ( ofaIGetter *getter );
const gchar *ofa_prefs_amount_get_thousand_sep             ( ofaIGetter *getter );
gboolean     ofa_prefs_amount_get_accept_dot               ( ofaIGetter *getter );
gboolean     ofa_prefs_amount_get_accept_comma             ( ofaIGetter *getter );
void         ofa_prefs_amount_set_user_settings            ( ofaIGetter *getter,
																	const gchar *decimal_sep,
																	const gchar *thousand_sep,
																	gboolean accept_dot,
																	gboolean accept_comma );

gboolean     ofa_prefs_appli_confirm_on_altf4              ( ofaIGetter *getter );
gboolean     ofa_prefs_appli_confirm_on_quit               ( ofaIGetter *getter );
void         ofa_prefs_appli_set_user_settings             ( ofaIGetter *getter,
																	gboolean confirm_on_altf4,
																	gboolean confirm_on_quit );

gboolean     ofa_prefs_assistant_quit_on_escape            ( ofaIGetter *getter );
gboolean     ofa_prefs_assistant_confirm_on_escape         ( ofaIGetter *getter );
gboolean     ofa_prefs_assistant_confirm_on_cancel         ( ofaIGetter *getter );
gboolean     ofa_prefs_assistant_is_willing_to_quit        ( ofaIGetter *getter, guint keyval );
void         ofa_prefs_assistant_set_user_settings         ( ofaIGetter *getter,
																	gboolean quit_on_escape,
																	gboolean confirm_on_escape,
																	gboolean confirm_on_cancel );

gboolean     ofa_prefs_check_integrity_get_display_all     ( ofaIGetter *getter );
void         ofa_prefs_check_integrity_set_user_settings   ( ofaIGetter *getter,
																	gboolean display );

myDateFormat ofa_prefs_date_get_display_format             ( ofaIGetter *getter );
myDateFormat ofa_prefs_date_get_check_format               ( ofaIGetter *getter );
gboolean     ofa_prefs_date_get_overwrite                  ( ofaIGetter *getter );
void         ofa_prefs_date_set_user_settings              ( ofaIGetter *getter,
																	myDateFormat display,
																	myDateFormat check,
																	gboolean overwrite );

const gchar *ofa_prefs_export_get_default_folder           ( ofaIGetter *getter );
void         ofa_prefs_export_set_user_settings            ( ofaIGetter *getter,
																	const gchar *folder );

gboolean     ofa_prefs_mainbook_get_dnd_reorder            ( ofaIGetter *getter );
gboolean     ofa_prefs_mainbook_get_with_detach_pin        ( ofaIGetter *getter );
void         ofa_prefs_mainbook_set_user_settings          ( ofaIGetter *getter,
																	gboolean dnd_reorder,
																	gboolean with_detach_pin );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_PREFS_H__ */
