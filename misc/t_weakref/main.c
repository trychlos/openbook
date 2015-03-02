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
 *
 *
 * To display debug messages, run the command:
 *   $ G_MESSAGES_DEBUG=OFA _install/bin/openbook
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*
 * What happens on an object which has put a weak reference on another
 * object when the former is finalized before the later ?
 *
 * RESULT:
 *
 * If we only weak_ref() a 'Dossier' object from a 'Store' object, then
 * this works safely as 'on_dossier_finalized()' is rightly called
 * between 'Dossier' dispose() and finalize(), thus letting us unreffing
 * the 'Store'.
 *
 * But if we first unref the 'Store', then the trigger is nonetheless
 * called, thus appyling on a no-more GObject.
 *
 * Using the weak_unref() when disposing 'Store' only works if the
 * 'Dossier' is alive because most probably GLib takes care of removing
 * itself the weak reference when disposing the 'Dossier'.
 *
 * So the solution is to keep trace of on_dossier_finalized() call, only
 * calling g_object_weak_unref() when this callback is not the reason
 * of the object finalization.
 */
#include <gtk/gtk.h>
#include <glib/gprintf.h>

/* the referred object
 */
#define MY_TYPE_DOSSIER            (my_dossier_get_type ())
#define MY_DOSSIER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MY_TYPE_DOSSIER, MyDossier))
#define MY_IS_DOSSIER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MY_TYPE_DOSSIER))
#define MY_DOSSIER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MY_TYPE_DOSSIER, MyDossierClass))
#define MY_IS_DOSSIER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MY_TYPE_DOSSIER))
#define MY_DOSSIER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MY_TYPE_DOSSIER, MyDossierClass))

typedef struct _MyDossierClass     MyDossierClass;

struct _MyDossierClass
{
	GObjectClass parent;
	/* class members */
	/* public virtual methods */
};

typedef struct _MyDossier          MyDossier;
typedef struct _MyDossierPrivate   MyDossierPrivate;

struct _MyDossier
{
	GObject           parent;
	/* public object members */
	/* private object members */
	MyDossierPrivate *priv;
};

GType my_dossier_get_type( void ) G_GNUC_CONST;

/* source code
 */

struct _MyDossierPrivate
{
	void *empty;
};

G_DEFINE_TYPE( MyDossier, my_dossier, G_TYPE_OBJECT )

#define MY_DOSSIER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MY_TYPE_DOSSIER, MyDossierPrivate))

static void
my_dossier_finalize( GObject *instance )
{
	g_debug( "my_dossier_finalize: self=%p", ( void * ) instance );
	/* free data here */
	/* chain up to parent class */
	G_OBJECT_CLASS( my_dossier_parent_class )->finalize( instance );
}

static void
my_dossier_dispose( GObject *instance )
{
	g_debug( "my_dossier_dispose: instance=%p", ( void * ) instance );
	/* unref member objects here */
	/* chain up to parent class */
	G_OBJECT_CLASS( my_dossier_parent_class )->dispose( instance );
}

static void
my_dossier_init( MyDossier *self )
{
	g_debug( "my_dossier_init: self=%p", ( void * ) self );
	self->priv = MY_DOSSIER_GET_PRIVATE( self );
}

static void
my_dossier_class_init( MyDossierClass *klass )
{
	g_type_class_add_private( klass, sizeof( MyDossierPrivate ));

	G_OBJECT_CLASS( klass )->dispose = my_dossier_dispose;
	G_OBJECT_CLASS( klass )->finalize = my_dossier_finalize;
}

static MyDossier *
my_dossier_new( void )
{
	return( g_object_new( MY_TYPE_DOSSIER, NULL ));
}

/* the referring object
 */
#define MY_TYPE_STORE            (my_store_get_type ())
#define MY_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MY_TYPE_STORE, MyStore))
#define MY_IS_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MY_TYPE_STORE))
#define MY_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MY_TYPE_STORE, MyStoreClass))
#define MY_IS_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MY_TYPE_STORE))
#define MY_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MY_TYPE_STORE, MyStoreClass))

typedef struct _MyStoreClass     MyStoreClass;

struct _MyStoreClass
{
	GObjectClass parent;
	/* class members */
	/* public virtual methods */
};

typedef struct _MyStore          MyStore;
typedef struct _MyStorePrivate   MyStorePrivate;

struct _MyStore
{
	GObject           parent;
	/* public object members */
	/* private object members */
	MyStorePrivate *priv;
};

GType my_store_get_type( void ) G_GNUC_CONST;

/* source code
 */

struct _MyStorePrivate
{
	MyDossier *dossier;
	gboolean   from_finalized_dossier;
};

G_DEFINE_TYPE( MyStore, my_store, G_TYPE_OBJECT )

#define MY_STORE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MY_TYPE_STORE, MyStorePrivate))

static void on_dossier_finalized( MyStore *store, gpointer finalized_dossier );

static void
my_store_finalize( GObject *instance )
{
	g_debug( "my_store_finalize: self=%p", ( void * ) instance );
	/* free data here */
	/* chain up to parent class */
	G_OBJECT_CLASS( my_store_parent_class )->finalize( instance );
}

static void
my_store_dispose( GObject *instance )
{
	MyStorePrivate *priv;
	g_debug( "my_store_dispose: instance=%p", ( void * ) instance );
	/* unref member objects here */
	priv = MY_STORE( instance )->priv;
	if( !priv->from_finalized_dossier ){
		g_object_weak_unref( G_OBJECT( priv->dossier ), ( GWeakNotify ) on_dossier_finalized, instance );
	}

	/* chain up to parent class */
	G_OBJECT_CLASS( my_store_parent_class )->dispose( instance );
}

static void
my_store_init( MyStore *self )
{
	g_debug( "my_store_init: self=%p", ( void * ) self );
	self->priv = MY_STORE_GET_PRIVATE( self );
}

static void
my_store_class_init( MyStoreClass *klass )
{
	g_type_class_add_private( klass, sizeof( MyStorePrivate ));

	G_OBJECT_CLASS( klass )->dispose = my_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = my_store_finalize;
}

static MyStore *
my_store_new( void )
{
	return( g_object_new( MY_TYPE_STORE, NULL ));
}

static void
on_dossier_finalized( MyStore *store, gpointer finalized_dossier )
{
	static const gchar *thisfn = "my_store_on_dossier_finalized";

	g_debug( "%s: store=%p (%s), ref_count=%d, finalized_dossier=%p",
			thisfn,
			( void * ) store, G_OBJECT_TYPE_NAME( store ), G_OBJECT( store )->ref_count,
			( void * ) finalized_dossier );

	g_return_if_fail( store && MY_IS_STORE( store ));

	store->priv->from_finalized_dossier = TRUE;

	g_object_unref( store );
}

/*
 *  first test case: first delete the dossier triggers the store
 * callback, which itself unref the store
 */
static void
first_test( void )
{
	MyDossier *dossier = my_dossier_new();
	MyStore *store = my_store_new();

	/* the store hosts a weak ref on the dossier, so that dossier
	 * finalization will trigger a store callback */
	g_object_weak_ref( G_OBJECT( dossier ), ( GWeakNotify ) on_dossier_finalized, store );
	store->priv->dossier = dossier;

	/* now delete the dossier */
	g_object_unref( dossier );

	/* see what happens when unreffing the dossier */
	g_debug( "first test: ok" );
}

/*
 *  second test case: first delete the store, then delete the dossier
 */
static void
second_test( void )
{
	MyDossier *dossier = my_dossier_new();
	MyStore *store = my_store_new();

	/* the store hosts a weak ref on the dossier, so that dossier
	 * finalization will trigger a store callback */
	g_object_weak_ref( G_OBJECT( dossier ), ( GWeakNotify ) on_dossier_finalized, store );
	store->priv->dossier = dossier;

	/* now delete the store */
	g_object_unref( store );

	/* see what happens when unreffing the dossier */
	g_object_unref( dossier );

	/* seg fault */
	/* GLib-GObject-WARNING **: invalid unclassed pointer in cast to 'GObject' */
	g_debug( "second test: ok" );
}

int
main( int argc, char *argv[] )
{
	/* first test case: first delete the dossier triggers the store
	 * callback, which itself unref the store
	 */
	first_test();

	/* second test case: first delete the store, then delete the dossier
	 */
	second_test();

	return 0;
}
