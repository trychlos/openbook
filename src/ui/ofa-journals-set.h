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

#ifndef __OFA_JOURNALS_SET_H__
#define __OFA_JOURNALS_SET_H__

/**
 * SECTION: ofa_journals_set
 * @short_description: #ofaJournalsSet class definition.
 * @include: ui/ofa-journals-set.h
 *
 * Display the chart of accounts, letting the user edit it.
 */

#include "ui/ofa-main-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_JOURNALS_SET                ( ofa_journals_set_get_type())
#define OFA_JOURNALS_SET( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_JOURNALS_SET, ofaJournalsSet ))
#define OFA_JOURNALS_SET_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_JOURNALS_SET, ofaJournalsSetClass ))
#define OFA_IS_JOURNALS_SET( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_JOURNALS_SET ))
#define OFA_IS_JOURNALS_SET_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_JOURNALS_SET ))
#define OFA_JOURNALS_SET_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_JOURNALS_SET, ofaJournalsSetClass ))

typedef struct _ofaJournalsSetPrivate        ofaJournalsSetPrivate;

typedef struct {
	/*< private >*/
	ofaMainPage            parent;
	ofaJournalsSetPrivate *private;
}
	ofaJournalsSet;

typedef struct _ofaJournalsSetClassPrivate   ofaJournalsSetClassPrivate;

typedef struct {
	/*< private >*/
	ofaMainPageClass            parent;
	ofaJournalsSetClassPrivate *private;
}
	ofaJournalsSetClass;

GType ofa_journals_set_get_type( void );

void  ofa_journals_set_run     ( ofaMainPage *this );

G_END_DECLS

#endif /* __OFA_JOURNALS_SET_H__ */
