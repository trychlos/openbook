/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_PERIODICITY_H__
#define __OPENBOOK_API_OFA_PERIODICITY_H__

/**
 * SECTION: ofaperiodicity
 * @title: ofaPeriodicity
 * @short_description: The #ofaPeriodicity Class Definition
 * @include: openbook/ofa-periodicity.h
 *
 * Periodicity is managed and stored in the database as an unlocalized character.
 */

G_BEGIN_DECLS

/* Periodicity codes are listed here in alphabetical order
 *  to make sure there is no duplicate
 */
#define PER_MONTHLY                     "M"
#define PER_WEEKLY                      "W"
#define PER_NEVER                       "N"

/**
 * PeriodicityEnumCb:
 *
 * The #ofa_periodicity_enum() callback.
 */
typedef void (*PeriodicityEnumCb)( const gchar *code, const gchar *label, void *user_data );

gchar *ofa_periodicity_get_label( const gchar *periodicity );

void   ofa_periodicity_enum     ( PeriodicityEnumCb fn, void *user_data );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_PERIODICITY_H__ */
