/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_TVA_STYLE_H__
#define __OFA_TVA_STYLE_H__

/**
 * SECTION: ofatvastyle
 * @include: tva/ofa-style.h
 *
 * VAT style provider class.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_TVA_STYLE                ( ofa_tva_style_get_type())
#define OFA_TVA_STYLE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVA_STYLE, ofaTVAStyle ))
#define OFA_TVA_STYLE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVA_STYLE, ofaTVAStyleClass ))
#define OFA_IS_TVA_STYLE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVA_STYLE ))
#define OFA_IS_TVA_STYLE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVA_STYLE ))
#define OFA_TVA_STYLE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVA_STYLE, ofaTVAStyleClass ))

typedef struct {
	/*< public members >*/
	GtkCssProvider      parent;
}
	ofaTVAStyle;

/**
 * ofaTVAStyleClass:
 */
typedef struct {
	/*< public members >*/
	GtkCssProviderClass parent;
}
	ofaTVAStyleClass;

GType        ofa_tva_style_get_type ( void );

ofaTVAStyle *ofa_tva_style_new      ( ofaIGetter *getter );

void         ofa_tva_style_set_style( ofaTVAStyle *provider,
											GtkWidget *widget,
											const gchar *style );

G_END_DECLS

#endif /* __OFA_TVA_STYLE_H__ */
