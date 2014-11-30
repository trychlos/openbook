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

#ifndef __OFO_BASE_PROT_H__
#define __OFO_BASE_PROT_H__

/**
 * SECTION: ofo_base
 * @short_description: #ofoBase class definition.
 * @include: api/ofo-base-prot.h
 *
 * The ofoBase class is the class base for application objects.
 *
 * This header is supposed to be included only by the child classes.
 */

G_BEGIN_DECLS

/* protected instance data
 * these are freely available to all derived classes
 */
struct _ofoBaseProtected {
	gboolean dispose_has_run;

	/* the fields loaded from the ofaBoxed definitions
	 */
	GList   *fields;
};

G_END_DECLS

#endif /* __OFO_BASE_PROT_H__ */
