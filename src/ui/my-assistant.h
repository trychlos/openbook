/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifndef __MY_ASSISTANT_H__
#define __MY_ASSISTANT_H__

/**
 * SECTION: my_assistant
 * @short_description: #myAssistant class definition.
 * @include: ui/my-assistant.h
 *
 * This is a base class for assistants.
 */

#include "api/my-window.h"

G_BEGIN_DECLS

#define MY_TYPE_ASSISTANT                ( my_assistant_get_type())
#define MY_ASSISTANT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_ASSISTANT, myAssistant ))
#define MY_ASSISTANT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_ASSISTANT, myAssistantClass ))
#define MY_IS_ASSISTANT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_ASSISTANT ))
#define MY_IS_ASSISTANT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_ASSISTANT ))
#define MY_ASSISTANT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_ASSISTANT, myAssistantClass ))

typedef struct _myAssistantPrivate       myAssistantPrivate;

typedef struct {
	/*< public members >*/
	myWindow            parent;

	/*< private members >*/
	myAssistantPrivate *priv;
}
	myAssistant;

typedef struct {
	/*< public members >*/
	myWindowClass       parent;
}
	myAssistantClass;

/**
 * myAssistantCb:
 */
typedef void ( *myAssistantCb )( myAssistant *, gint, GtkWidget * );

/**
 * ofsAssistant:
 */
typedef struct {
	gint          page_num;
	myAssistantCb init_cb;
	myAssistantCb display_cb;
	myAssistantCb forward_cb;
}
	ofsAssistant;

/**
 * Signals defined by #myAssistant class:
 * @ASSISTANT_SIGNAL_PAGE_INIT:    sent before displaying a page the
 *                                 first time
 * @ASSISTANT_SIGNAL_PAGE_DISPLAY: sent when about to display a page
 * @ASSISTANT_SIGNAL_PAGE_FORWARD: sent when the user has clicked on the
 *                                 'forward' button
 */
#define MY_SIGNAL_PAGE_INIT             "my-assistant-signal-page-init"
#define MY_SIGNAL_PAGE_DISPLAY          "my-assistant-signal-page-display"
#define MY_SIGNAL_PAGE_FORWARD          "my-assistant-signal-page-forward"

GType         my_assistant_get_type            ( void ) G_GNUC_CONST;

void          my_assistant_run                 ( myAssistant *assistant );

void          my_assistant_set_callbacks       ( myAssistant *assistant,
														const ofsAssistant *cbs );

gulong        my_assistant_signal_connect      ( myAssistant *assistant,
														const gchar *signal,
														GCallback cb );

gboolean      my_assistant_is_page_initialized ( myAssistant *assistant,
														GtkWidget *page );

void          my_assistant_set_page_initialized( myAssistant *assistant,
														GtkWidget *page,
														gboolean initialized );

GtkAssistant *my_assistant_get_assistant       ( myAssistant *assistant );

GtkWidget    *my_assistant_get_current_page    ( myAssistant *assistant );

void          my_assistant_set_page_complete   ( myAssistant *assistant,
														gboolean complete );

void          my_assistant_set_page_type       ( myAssistant *assistant,
														GtkAssistantPageType type );

G_END_DECLS

#endif /* __MY_ASSISTANT_H__ */
