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

#ifndef __OPENBOOK_API_UI_OFA_IENTRY_OPE_TEMPLATE_H__
#define __OPENBOOK_API_UI_OFA_IENTRY_OPE_TEMPLATE_H__

/**
 * SECTION: ientry_ope_template
 * @title: ofaIEntryOpeTemplate
 * @short_description: The IEntryOpeTemplate interface
 * @include: openbook/ui/ofa-ientry-ope-template.h
 *
 * The #ofaIEntryOpeTemplate interface lets the user enter and select
 * operation templates in the provided GtkEntry.
 *
 * Just call #ofa_ientry_ope_template_init() with each GtkEntry you want
 * set, and the function will take care of setting an icon, triggering
 * #ofaOpeTemplateSelect dialog for selection.
 */

#include <gtk/gtk.h>

#include "api/ofa-main-window-def.h"
#include "api/ofo-ope-template-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IENTRY_OPE_TEMPLATE                      ( ofa_ientry_ope_template_get_type())
#define OFA_IENTRY_OPE_TEMPLATE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IENTRY_OPE_TEMPLATE, ofaIEntryOpeTemplate ))
#define OFA_IS_IENTRY_OPE_TEMPLATE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IENTRY_OPE_TEMPLATE ))
#define OFA_IENTRY_OPE_TEMPLATE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IENTRY_OPE_TEMPLATE, ofaIEntryOpeTemplateInterface ))

typedef struct _ofaIEntryOpeTemplate                      ofaIEntryOpeTemplate;

/**
 * ofaIEntryOpeTemplateInterface:
 * @get_interface_version: [should]: returns the version of the
 *  implemented interface.
 * @on_pre_select: [may]: modify the initial selection.
 * @on_post_select: [may]: do something after the selection.
 *
 * This defines the interface that an #ofaIEntryOpeTemplate may implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIEntryOpeTemplate instance.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaIEntryOpeTemplate interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1
	 */
	guint   ( *get_interface_version )( const ofaIEntryOpeTemplate *instance );

	/**
	 * on_pre_select:
	 * @instance: the #ofaIEntryOpeTemplate instance.
	 * @entry: the #GtkEntry which embeds the pressed icon.
	 * @position: the #GtkEntryIconPosition.
	 *
	 * Called before opening the operation template selection dialog
	 * with the content of the @entry.
	 * The implementation may force the initial selection by returning
	 * a to-be-selected identifier as a newly allocated string which will
	 * be g_free() by the interface.
	 *
	 * If this method is not implemented, the interface will set the
	 * initial selection with the content of the @entry.
	 *
	 * When this method is implemented, the interface will set the initial
	 * selection with the returning string, whatever this string be.
	 *
	 * Since: version 1
	 */
	gchar * ( *on_pre_select )        ( ofaIEntryOpeTemplate *instance,
											GtkEntry *entry,
											GtkEntryIconPosition position );

	/**
	 * on_post_select:
	 * @instance: the #ofaIEntryOpeTemplate instance.
	 * @entry: the #GtkEntry which embeds the pressed icon.
	 * @position: the #GtkEntryIconPosition.
	 * @ope_template_id: the selected operation template identifier.
	 *
	 * Let the implementation modify the selection, or do something after
	 * the selection.
	 *
	 * If this method is not implemented, or returns %NULL, then the
	 * selected @ope_template_id will be used as the account identifier.
	 *
	 * When this method is implemented and returns something, then it
	 * must be a newly allocated string which whill be used a the
	 * selected account identifier, before being g_free() by the interface.
	 *
	 * Since: version 1
	 */
	gchar * ( *on_post_select )       ( ofaIEntryOpeTemplate *instance,
											GtkEntry *entry,
											GtkEntryIconPosition position,
											const gchar *ope_template_id );
}
	ofaIEntryOpeTemplateInterface;

GType ofa_ientry_ope_template_get_type                  ( void );

guint ofa_ientry_ope_template_get_interface_last_version( void );

guint ofa_ientry_ope_template_get_interface_version     ( const ofaIEntryOpeTemplate *instance );

void  ofa_ientry_ope_template_init                      ( ofaIEntryOpeTemplate *instance,
																ofaMainWindow *main_window,
																GtkEntry *entry );

G_END_DECLS

#endif /* __OFA_IENTRY_OPE_TEMPLATE_H__ */
