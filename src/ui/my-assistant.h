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

#ifndef __MY_ASSISTANT_H__
#define __MY_ASSISTANT_H__

/**
 * SECTION: my_assistant
 * @short_description: #myAssistant class definition.
 * @include: ui/my-assistant.h
 *
 * This is a base class for assistants.
 */

#include "ui/my-window.h"

G_BEGIN_DECLS

#define MY_TYPE_ASSISTANT                ( my_assistant_get_type())
#define MY_ASSISTANT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_ASSISTANT, myAssistant ))
#define MY_ASSISTANT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_ASSISTANT, myAssistantClass ))
#define MY_IS_ASSISTANT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_ASSISTANT ))
#define MY_IS_ASSISTANT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_ASSISTANT ))
#define MY_ASSISTANT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_ASSISTANT, myAssistantClass ))

typedef struct _myAssistantPrivate       myAssistantPrivate;

typedef struct {
	/*< private >*/
	myWindow            parent;
	myAssistantPrivate *private;
}
	myAssistant;

typedef struct {
	/*< private >*/
	myWindowClass parent;
}
	myAssistantClass;

GType my_assistant_get_type    ( void ) G_GNUC_CONST;

void  my_assistant_run         ( myAssistant *assistant );

gint  my_assistant_get_page_num( myAssistant *assistant, GtkWidget *page );

G_END_DECLS

#endif /* __MY_ASSISTANT_H__ */
