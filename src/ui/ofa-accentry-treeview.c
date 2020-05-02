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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvfilterable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "ui/ofa-accentry-store.h"
#include "ui/ofa-accentry-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean                      dispose_has_run;

	/* initialization
	 */
	ofaIGetter                   *getter;
	gchar                        *settings_prefix;

	/* runtime
	 */
	GtkTreeModelFilterVisibleFunc filter_fn;
	void                         *filter_data;
}
	ofaAccentryTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void      setup_columns( ofaAccentryTreeview *self );
static void      on_selection_changed( ofaAccentryTreeview *self, GtkTreeSelection *selection, void *empty );
static void      on_selection_activated( ofaAccentryTreeview *self, GtkTreeSelection *selection, void *empty );
static void      get_and_send( ofaAccentryTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoBase  *get_selected_with_selection( ofaAccentryTreeview *self, GtkTreeSelection *selection );
static gboolean  tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *iter );
static gint      tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaAccentryTreeview, ofa_accentry_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaAccentryTreeview ))

static void
accentry_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_accentry_treeview_finalize";
	ofaAccentryTreeviewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCENTRY_TREEVIEW( instance ));

	/* free data members here */
	priv = ofa_accentry_treeview_get_instance_private( OFA_ACCENTRY_TREEVIEW( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accentry_treeview_parent_class )->finalize( instance );
}

static void
accentry_treeview_dispose( GObject *instance )
{
	ofaAccentryTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCENTRY_TREEVIEW( instance ));

	priv = ofa_accentry_treeview_get_instance_private( OFA_ACCENTRY_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accentry_treeview_parent_class )->dispose( instance );
}

static void
ofa_accentry_treeview_init( ofaAccentryTreeview *self )
{
	static const gchar *thisfn = "ofa_accentry_treeview_init";
	ofaAccentryTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_ACCENTRY_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_accentry_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->filter_fn = NULL;
	priv->filter_data = NULL;
}

static void
ofa_accentry_treeview_class_init( ofaAccentryTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_accentry_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accentry_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = accentry_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->filter = tvbin_v_filter;
	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	/**
	 * ofaAccentryTreeview::ofa-accchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaAccentryTreeview proxyes it with this 'ofa-accchanged' signal,
	 * signal, providing the selected object.
	 *
	 * Argument is the selected object; may be %NULL, or an account or
	 * an entry.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccentryTreeview *view,
	 * 						ofoBase            *object,
	 * 						gpointer           user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-accchanged",
				OFA_TYPE_ACCENTRY_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaAccentryTreeview::ofa-accactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaAccentryTreeview proxyes it with this 'ofa-accactivated' signal,
	 * providing the selected object.
	 *
	 * Argument is the selected object, which may be an account or an
	 * entry.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccentryTreeview *view,
	 * 						ofoBase           *object,
	 * 						gpointer           user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-accactivated",
				OFA_TYPE_ACCENTRY_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
}

/**
 * ofa_accentry_treeview_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: [allow-none]: the prefix of the settings key.
 *
 * Returns: a new #ofaAccentryTreeview instance.
 */
ofaAccentryTreeview *
ofa_accentry_treeview_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaAccentryTreeview *view;
	ofaAccentryTreeviewPrivate *priv;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	view = g_object_new( OFA_TYPE_ACCENTRY_TREEVIEW,
				"ofa-tvbin-getter",  getter,
				"ofa-tvbin-shadow",  GTK_SHADOW_IN,
				NULL );

	priv = ofa_accentry_treeview_get_instance_private( view );

	priv->getter = getter;

	if( my_strlen( settings_prefix )){
		str = g_strdup_printf( "%s-%s", settings_prefix, priv->settings_prefix );
		g_free( priv->settings_prefix );
		priv->settings_prefix = str;
	}

	ofa_tvbin_set_name( OFA_TVBIN( view ), priv->settings_prefix );

	setup_columns( view );

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * the selected objects instead of just the raw GtkTreeSelection
	 */
	g_signal_connect( view, "ofa-selchanged", G_CALLBACK( on_selection_changed ), NULL );
	g_signal_connect( view, "ofa-selactivated", G_CALLBACK( on_selection_activated ), NULL );

	return( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaAccentryTreeview *self )
{
	static const gchar *thisfn = "ofa_accentry_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCENTRY_COL_ACCOUNT,              _( "Account" ),         NULL );
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), ACCENTRY_COL_LABEL,                _( "Label" ),           NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCENTRY_COL_CURRENCY,             _( "Currency" ),        NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCENTRY_COL_UPD_USER,             _( "User" ),        _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), ACCENTRY_COL_UPD_STAMP,            _( "Timestamp" ),   _( "Last update timestamp" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), ACCENTRY_COL_SETTLEABLE,           _( "S" ),           _( "Settleable" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), ACCENTRY_COL_KEEP_UNSETTLED,       _( "Kus" ),         _( "Keep unsettled" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), ACCENTRY_COL_RECONCILIABLE,        _( "R" ),           _( "Reconciliable" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), ACCENTRY_COL_KEEP_UNRECONCILIATED, _( "Kur" ),         _( "Keep unreconciliated" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), ACCENTRY_COL_DOPE,                 _( "Ope." ),        _( "Operation date" ));
	ofa_tvbin_add_column_date   ( OFA_TVBIN( self ), ACCENTRY_COL_DEFFECT,              _( "Effect" ),      _( "Effect date" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCENTRY_COL_REF,                  _( "Ref." ),        _( "Piece reference" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCENTRY_COL_LEDGER,               _( "Ledger" ),          NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), ACCENTRY_COL_OPE_TEMPLATE,         _( "Template" ),    _( "Operation template" ));
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCENTRY_COL_DEBIT,                _( "Debit" ),           NULL );
	ofa_tvbin_add_column_amount ( OFA_TVBIN( self ), ACCENTRY_COL_CREDIT,               _( "Credit" ),          NULL );
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), ACCENTRY_COL_OPE_NUMBER,           _( "Ope." ),        _( "Operation number" ));
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), ACCENTRY_COL_ENT_NUMBER,           _( "Ent.num" ),     _( "Accentry number" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), ACCENTRY_COL_STATUS,               _( "Status" ),          NULL );

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), ACCENTRY_COL_LABEL );
	ofa_itvcolumnable_twins_group_new( OFA_ITVCOLUMNABLE( self ), "amount", ACCENTRY_COL_DEBIT, ACCENTRY_COL_CREDIT, -1 );
}

/**
 * ofa_accentry_treeview_set_filter:
 * @view: this #ofaAccentryTreeview instance.
 * @filter_fn: an external filter function.
 * @filter_data: the data to be passed to the @filter_fn function.
 *
 * Setup the filtering function.
 */
void
ofa_accentry_treeview_set_filter_func( ofaAccentryTreeview *view, GtkTreeModelFilterVisibleFunc filter_fn, void *filter_data )
{
	ofaAccentryTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_ACCENTRY_TREEVIEW( view ));

	priv = ofa_accentry_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->filter_fn = filter_fn;
	priv->filter_data = filter_data;
}

static void
on_selection_changed( ofaAccentryTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-accchanged" );
}

static void
on_selection_activated( ofaAccentryTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-accactivated" );
}

static void
get_and_send( ofaAccentryTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoBase *object;

	object = get_selected_with_selection( self, selection );
	if( object ){
		g_signal_emit_by_name( self, signal, object );
	}
}

/**
 * ofa_accentry_treeview_get_selected:
 * @view: this #ofaAccentryTreeview instance.
 *
 * Returns: the selected object, which may be %NULL.
 *
 * If not %NULL, then the returned object may be an #ofoAccount or an
 * #ofoEntry.
 *
 * The returned reference is owned by the underlying ofaAccentryStore,
 * and should not be released by the caller.
 */
ofoBase *
ofa_accentry_treeview_get_selected( ofaAccentryTreeview *view )
{
	static const gchar *thisfn = "ofa_accentry_treeview_get_selected";
	ofaAccentryTreeviewPrivate *priv;
	GtkTreeSelection *selection;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_ACCENTRY_TREEVIEW( view ), NULL );

	priv = ofa_accentry_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));

	return( get_selected_with_selection( view, selection ));
}

/*
 * get_selected_with_selection:
 * @view: this #ofaAccentryTreeview instance.
 * @selection: the current #GtkTreeSelection.
 *
 * Return: the list of selected objects, or %NULL.
 */
static ofoBase *
get_selected_with_selection( ofaAccentryTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoBase *object;

	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, ACCENTRY_COL_OBJECT, &object, -1 );
		if( object ){
			g_return_val_if_fail( OFO_IS_ACCOUNT( object ) || OFO_IS_ENTRY( object ), NULL );
			g_object_unref( object );
			return( object );
		}
	}

	return( NULL );
}

/**
 * ofa_accentry_treeview_collapse_all:
 * @view: this #ofaAccentryTreeview instance.
 *
 * Collapses all the hierarchy.
 */
void
ofa_accentry_treeview_collapse_all( ofaAccentryTreeview *view )
{
	static const gchar *thisfn = "ofa_accentry_treeview_collapse_all";
	ofaAccentryTreeviewPrivate *priv;
	GtkWidget *treeview;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_if_fail( view && OFA_IS_ACCENTRY_TREEVIEW( view ));

	priv = ofa_accentry_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	treeview = ofa_tvbin_get_tree_view( OFA_TVBIN( view ));

	gtk_tree_view_collapse_all( GTK_TREE_VIEW( treeview ));
}

/**
 * ofa_accentry_treeview_expand_all:
 * @view: this #ofaAccentryTreeview instance.
 *
 * Expands all the hierarchy.
 */
void
ofa_accentry_treeview_expand_all( ofaAccentryTreeview *view )
{
	static const gchar *thisfn = "ofa_accentry_treeview_expand_all";
	ofaAccentryTreeviewPrivate *priv;
	GtkWidget *treeview;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_if_fail( view && OFA_IS_ACCENTRY_TREEVIEW( view ));

	priv = ofa_accentry_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	treeview = ofa_tvbin_get_tree_view( OFA_TVBIN( view ));

	gtk_tree_view_expand_all( GTK_TREE_VIEW( treeview ));
}

static gboolean
tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *iter )
{
	ofaAccentryTreeviewPrivate *priv;

	priv = ofa_accentry_treeview_get_instance_private( OFA_ACCENTRY_TREEVIEW( tvbin ));

	return( priv->filter_fn ? ( *priv->filter_fn )( tmodel, iter, priv->filter_data ) : TRUE );
}

static gint
tvbin_v_sort( const ofaTVBin *tvbin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_accentry_treeview_v_sort";
	ofaAccentryTreeviewPrivate *priv;
	gint cmp;
	gchar *accounta, *labela, *cura, *updusera, *updstampa, *seta, *kunsa, *reca, *kunra, *dopea, *deffa, *refa, *ledgera, *templatea, *debita, *credita, *openuma, *statusa;
	ofxCounter entnuma;
	gchar *accountb, *labelb, *curb, *upduserb, *updstampb, *setb, *kunsb, *recb, *kunrb, *dopeb, *deffb, *refb, *ledgerb, *templateb, *debitb, *creditb, *openumb, *statusb;
	ofxCounter entnumb;

	priv = ofa_accentry_treeview_get_instance_private( OFA_ACCENTRY_TREEVIEW( tvbin ));

	gtk_tree_model_get( tmodel, a,
			ACCENTRY_COL_ACCOUNT,              &accounta,
			ACCENTRY_COL_LABEL,                &labela,
			ACCENTRY_COL_CURRENCY,             &cura,
			ACCENTRY_COL_UPD_USER,             &updusera,
			ACCENTRY_COL_UPD_STAMP,            &updstampa,
			ACCENTRY_COL_SETTLEABLE,           &seta,
			ACCENTRY_COL_KEEP_UNSETTLED,       &kunsa,
			ACCENTRY_COL_RECONCILIABLE,        &reca,
			ACCENTRY_COL_KEEP_UNRECONCILIATED, &kunra,
			ACCENTRY_COL_DOPE,                 &dopea,
			ACCENTRY_COL_DEFFECT,              &deffa,
			ACCENTRY_COL_REF,                  &refa,
			ACCENTRY_COL_LEDGER,               &ledgera,
			ACCENTRY_COL_OPE_TEMPLATE,         &templatea,
			ACCENTRY_COL_DEBIT,                &debita,
			ACCENTRY_COL_CREDIT,               &credita,
			ACCENTRY_COL_OPE_NUMBER,           &openuma,
			ACCENTRY_COL_ENT_NUMBER_I,         &entnuma,
			ACCENTRY_COL_STATUS,               &statusa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			ACCENTRY_COL_ACCOUNT,              &accountb,
			ACCENTRY_COL_LABEL,                &labelb,
			ACCENTRY_COL_CURRENCY,             &curb,
			ACCENTRY_COL_UPD_USER,             &upduserb,
			ACCENTRY_COL_UPD_STAMP,            &updstampb,
			ACCENTRY_COL_SETTLEABLE,           &setb,
			ACCENTRY_COL_KEEP_UNSETTLED,       &kunsb,
			ACCENTRY_COL_RECONCILIABLE,        &recb,
			ACCENTRY_COL_KEEP_UNRECONCILIATED, &kunrb,
			ACCENTRY_COL_DOPE,                 &dopeb,
			ACCENTRY_COL_DEFFECT,              &deffb,
			ACCENTRY_COL_REF,                  &refb,
			ACCENTRY_COL_LEDGER,               &ledgerb,
			ACCENTRY_COL_OPE_TEMPLATE,         &templateb,
			ACCENTRY_COL_DEBIT,                &debitb,
			ACCENTRY_COL_CREDIT,               &creditb,
			ACCENTRY_COL_OPE_NUMBER,           &openumb,
			ACCENTRY_COL_ENT_NUMBER_I,         &entnumb,
			ACCENTRY_COL_STATUS,               &statusb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case ACCENTRY_COL_ACCOUNT:
			cmp = my_collate( accounta, accountb );
			break;
		case ACCENTRY_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case ACCENTRY_COL_CURRENCY:
			cmp = my_collate( cura, curb );
			break;
		case ACCENTRY_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case ACCENTRY_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		case ACCENTRY_COL_SETTLEABLE:
			cmp = my_collate( seta, setb );
			break;
		case ACCENTRY_COL_KEEP_UNSETTLED:
			cmp = my_collate( kunsa, kunsb );
			break;
		case ACCENTRY_COL_RECONCILIABLE:
			cmp = my_collate( reca, recb );
			break;
		case ACCENTRY_COL_KEEP_UNRECONCILIATED:
			cmp = my_collate( kunra, kunrb );
			break;
		case ACCENTRY_COL_DOPE:
			cmp = my_date_compare_by_str( dopea, dopeb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case ACCENTRY_COL_DEFFECT:
			cmp = my_date_compare_by_str( deffa, deffb, ofa_prefs_date_get_display_format( priv->getter ));
			break;
		case ACCENTRY_COL_REF:
			cmp = my_collate( refa, refb );
			break;
		case ACCENTRY_COL_LEDGER:
			cmp = my_collate( ledgera, ledgerb );
			break;
		case ACCENTRY_COL_OPE_TEMPLATE:
			cmp = my_collate( templatea, templateb );
			break;
		case ACCENTRY_COL_DEBIT:
			cmp = ofa_itvsortable_sort_str_amount( OFA_ITVSORTABLE( tvbin ), debita, debitb );
			break;
		case ACCENTRY_COL_CREDIT:
			cmp = ofa_itvsortable_sort_str_amount( OFA_ITVSORTABLE( tvbin ), credita, creditb );
			break;
		case ACCENTRY_COL_OPE_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( openuma, openumb );
			break;
		case ACCENTRY_COL_ENT_NUMBER:
			cmp = entnuma < entnumb ? -1 : ( entnuma > entnumb ? 1 : 0 );
			break;
		case ACCENTRY_COL_STATUS:
			cmp = my_collate( statusa, statusb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( accounta );
	g_free( labela );
	g_free( cura );
	g_free( updusera );
	g_free( updstampa );
	g_free( seta );
	g_free( kunsa );
	g_free( reca );
	g_free( kunra );
	g_free( dopea );
	g_free( deffa );
	g_free( refa );
	g_free( ledgera );
	g_free( templatea );
	g_free( debita );
	g_free( credita );
	g_free( openuma );
	g_free( statusa );

	g_free( accountb );
	g_free( labelb );
	g_free( curb );
	g_free( upduserb );
	g_free( updstampb );
	g_free( setb );
	g_free( kunsb );
	g_free( recb );
	g_free( kunrb );
	g_free( dopeb );
	g_free( deffb );
	g_free( refb );
	g_free( ledgerb );
	g_free( templateb );
	g_free( debitb );
	g_free( creditb );
	g_free( openumb );
	g_free( statusb );

	return( cmp );
}
