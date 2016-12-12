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

#ifndef __OPENBOOK_API_OFA_IDBEXERCICE_META_H__
#define __OPENBOOK_API_OFA_IDBEXERCICE_META_H__

/**
 * SECTION: idbperiod
 * @title: ofaIDBExerciceMeta
 * @short_description: An interface to manage the financial periods of a dossier
 * @include: openbook/ofa-idbexercice-meta.h
 *
 * The #ofaIDBExerciceMeta interface manages the financial periods of a
 * dossier, and any other external properties.
 */

#include <glib-object.h>

#include "api/ofa-idbexercice-meta-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBEXERCICE_META                      ( ofa_idbexercice_meta_get_type())
#define OFA_IDBEXERCICE_META( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBEXERCICE_META, ofaIDBExerciceMeta ))
#define OFA_IS_IDBEXERCICE_META( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBEXERCICE_META ))
#define OFA_IDBEXERCICE_META_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBEXERCICE_META, ofaIDBExerciceMetaInterface ))

 #if 0
typedef struct _ofaIDBExerciceMeta                     ofaIDBExerciceMeta;
typedef struct _ofaIDBExerciceMetaInterface            ofaIDBExerciceMetaInterface;
#endif

/**
 * ofaIDBExerciceMetaInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @get_name: [should]: returns a name.
 * @compare: [should]: compare two objects.
 * @dump: [should]: dump the object.
 *
 * This defines the interface that an #ofaIDBExerciceMeta should/must
 * implement.
 */
struct _ofaIDBExerciceMetaInterface {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/*** implementation-wide ***/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint    ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * get_name:
	 * @instance: the #ofaIDBExerciceMeta instance.
	 *
	 * Returns: a name which may better qualify the period, as a newly
	 * allocated string which should be g_free() by the caller.
	 *
	 * The exact nature of the returned name is left to the plugin. The
	 * application may expect that this name be more or less
	 * representative of the period, or its implementation. It is only
	 * used for display, never to identify it.
	 *
	 * Since: version 1
	 */
	gchar *  ( *get_name )             ( const ofaIDBExerciceMeta *instance );

	/**
	 * compare:
	 * @a: the #ofaIDBExerciceMeta instance.
	 * @b: another #ofaIDBExerciceMeta instance.
	 *
	 * Returns: 0 if @a and @b have the same content, -1 if @a < @b,
	 *  +1 if @a > @b.
	 *
	 * Since: version 1
	 */
	gint     ( *compare )              ( const ofaIDBExerciceMeta *a,
												const ofaIDBExerciceMeta *b );
	/**
	 * dump:
	 * @instance: the #ofaIDBExerciceMeta instance.
	 *
	 * Dump the object.
	 *
	 * Since: version 1
	 */
	void     ( *dump )                 ( const ofaIDBExerciceMeta *instance );
};

/*
 * Interface-wide
 */
GType        ofa_idbexercice_meta_get_type                  ( void );

guint        ofa_idbexercice_meta_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint        ofa_idbexercice_meta_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
const GDate *ofa_idbexercice_meta_get_begin_date            ( const ofaIDBExerciceMeta *period );

void         ofa_idbexercice_meta_set_begin_date            ( ofaIDBExerciceMeta *period,
																	const GDate *date );

const GDate *ofa_idbexercice_meta_get_end_date              ( const ofaIDBExerciceMeta *period );

void         ofa_idbexercice_meta_set_end_date              ( ofaIDBExerciceMeta *period,
																	const GDate *date );

gboolean     ofa_idbexercice_meta_get_current               ( const ofaIDBExerciceMeta *period );

void         ofa_idbexercice_meta_set_current               ( ofaIDBExerciceMeta *period,
																	gboolean current );

gchar       *ofa_idbexercice_meta_get_status                ( const ofaIDBExerciceMeta *period );

gchar       *ofa_idbexercice_meta_get_label                 ( const ofaIDBExerciceMeta *period );

gchar       *ofa_idbexercice_meta_get_name                  ( const ofaIDBExerciceMeta *period );

gint         ofa_idbexercice_meta_compare                   ( const ofaIDBExerciceMeta *a,
																	const ofaIDBExerciceMeta *b );

gboolean     ofa_idbexercice_meta_is_suitable               ( const ofaIDBExerciceMeta *period,
																	const GDate *begin,
																	const GDate *end );

void         ofa_idbexercice_meta_dump                      ( const ofaIDBExerciceMeta *period );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBEXERCICE_META_H__ */