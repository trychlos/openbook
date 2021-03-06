/*
 * Open Firm OpeTemplateing
 * A double-entry templateing application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
 *
 * Open Firm OpeTemplateing is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm OpeTemplateing is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm OpeTemplateing; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "my/my-utils.h"

#include "api/ofa-buttons-box.h"
#include "api/ofa-igetter.h"
#include "api/ofa-istore.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-ope-template-store.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-ope-template-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
	gchar      *settings_prefix;
	gchar      *ledger;
}
	ofaOpeTemplateTreeviewPrivate;

/* class properties
 */
enum {
	PROP_LEDGER_ID = 1,
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void            setup_columns( ofaOpeTemplateTreeview *self );
static void            on_selection_changed( ofaOpeTemplateTreeview *self, GtkTreeSelection *selection, void *empty );
static void            on_selection_activated( ofaOpeTemplateTreeview *self, GtkTreeSelection *selection, void *empty );
static void            on_selection_delete( ofaOpeTemplateTreeview *self, GtkTreeSelection *selection, void *empty );
static void            get_and_send( ofaOpeTemplateTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoOpeTemplate *get_selected_with_selection( ofaOpeTemplateTreeview *self, GtkTreeSelection *selection );
static gboolean        tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *model, GtkTreeIter *iter );
static gint            tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaOpeTemplateTreeview, ofa_ope_template_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaOpeTemplateTreeview ))

static void
ope_template_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_treeview_finalize";
	ofaOpeTemplateTreeviewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_TREEVIEW( instance ));
	priv = ofa_ope_template_treeview_get_instance_private( OFA_OPE_TEMPLATE_TREEVIEW( instance ));

	priv = ofa_ope_template_treeview_get_instance_private( OFA_OPE_TEMPLATE_TREEVIEW( instance ));

	/* free data members here */
	g_free( priv->settings_prefix );
	g_free( priv->ledger );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_treeview_parent_class )->finalize( instance );
}

static void
ope_template_treeview_dispose( GObject *instance )
{
	ofaOpeTemplateTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_TREEVIEW( instance ));

	priv = ofa_ope_template_treeview_get_instance_private( OFA_OPE_TEMPLATE_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_treeview_parent_class )->dispose( instance );
}

/*
 * user asks for a property
 * we have so to put the corresponding data into the provided GValue
 */
static void
ope_template_treeview_get_property( GObject *instance, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaOpeTemplateTreeviewPrivate *priv;

	g_return_if_fail( OFA_IS_OPE_TEMPLATE_TREEVIEW( instance ));

	priv = ofa_ope_template_treeview_get_instance_private( OFA_OPE_TEMPLATE_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case PROP_LEDGER_ID:
				g_value_set_string( value, priv->ledger );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

/*
 * the user asks to set a property and provides it into a GValue
 * read the content of the provided GValue and set our instance datas
 */
static void
ope_template_treeview_set_property( GObject *instance, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaOpeTemplateTreeviewPrivate *priv;

	g_return_if_fail( OFA_IS_OPE_TEMPLATE_TREEVIEW( instance ));

	priv = ofa_ope_template_treeview_get_instance_private( OFA_OPE_TEMPLATE_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case PROP_LEDGER_ID:
				g_free( priv->ledger );
				priv->ledger = g_strdup( g_value_get_string( value ));
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

static void
ofa_ope_template_treeview_init( ofaOpeTemplateTreeview *self )
{
	static const gchar *thisfn = "ofa_ope_template_treeview_init";
	ofaOpeTemplateTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_ope_template_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->ledger = NULL;
}

static void
ofa_ope_template_treeview_class_init( ofaOpeTemplateTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = ope_template_treeview_get_property;
	G_OBJECT_CLASS( klass )->set_property = ope_template_treeview_set_property;
	G_OBJECT_CLASS( klass )->dispose = ope_template_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->filter = tvbin_v_filter;
	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_LEDGER_ID,
			g_param_spec_string(
					"ofa-ope-template-treeview-ledger",
					"Ledger",
					"Filtered ledger",
					NULL,
					G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	/**
	 * ofaOpeTemplateTreeview::ofa-opechanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaOpeTemplateTreeview proxyes it with this 'ofa-opechanged' signal,
	 * providing the #ofoOpeTemplate selected object.
	 *
	 * Argument is the current #ofoOpeTemplate object, may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateTreeview *view,
	 * 						ofoOpeTemplate       *object,
	 * 						gpointer              user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-opechanged",
				OFA_TYPE_OPE_TEMPLATE_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaOpeTemplateTreeview::ofa-opeactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaOpeTemplateTreeview proxyes it with this 'ofa-opeactivated' signal,
	 * providing the #ofoOpeTemplate selected object.
	 *
	 * Argument is the current #ofoOpeTemplate object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateTreeview *view,
	 * 						ofoOpeTemplate       *object,
	 * 						gpointer              user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-opeactivated",
				OFA_TYPE_OPE_TEMPLATE_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaOpeTemplateTreeview::ofa-opedelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaOpeTemplateTreeview proxyes it with this 'ofa-opedelete' signal,
	 * providing the #ofoOpeTemplate selected object.
	 *
	 * Argument is the current #ofoOpeTemplate object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaOpeTemplateTreeview *view,
	 * 						ofoOpeTemplate       *object,
	 * 						gpointer              user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-opedelete",
				OFA_TYPE_OPE_TEMPLATE_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );
}

/**
 * ofa_ope_template_treeview_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: [allow-none]: the prefix of the key in user settings;
 *  if %NULL, then rely on this class name;
 *  when set, then this class automatically adds its name as a suffix.
 * @ledger: the filtered ledger.
 *  It must be set at instanciation time as it is also used as a
 *  qualifier for the actions group name.
 *
 * Returns: a new instance.
 */
ofaOpeTemplateTreeview *
ofa_ope_template_treeview_new( ofaIGetter *getter, const gchar *settings_prefix, const gchar *ledger )
{
	ofaOpeTemplateTreeview *view;
	ofaOpeTemplateTreeviewPrivate *priv;
	gchar *str;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	view = g_object_new( OFA_TYPE_OPE_TEMPLATE_TREEVIEW,
				"ofa-tvbin-getter",  getter,
				NULL );

	priv = ofa_ope_template_treeview_get_instance_private( view );

	priv->getter = getter;
	priv->ledger = g_strdup( ledger );

	if( my_strlen( settings_prefix )){
		str = g_strdup_printf( "%s-%s", settings_prefix, priv->settings_prefix );
		g_free( priv->settings_prefix );
		priv->settings_prefix = str;
	}

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoOpeTemplate object instead of just the raw GtkTreeSelection
	 */
	g_signal_connect( view, "ofa-selchanged", G_CALLBACK( on_selection_changed ), NULL );
	g_signal_connect( view, "ofa-selactivated", G_CALLBACK( on_selection_activated ), NULL );

	/* the 'ofa-seldelete' signal is sent in response to the Delete key press.
	 * There may be no current selection.
	 * in this case, the signal is just ignored (not proxied).
	 */
	g_signal_connect( view, "ofa-seldelete", G_CALLBACK( on_selection_delete ), NULL );

	/* because the ofaOpeTemplateTreeview is built to live inside of a
	 * GtkNotebook, not each view will save its settings, but only
	 * the last being saw by the user (see ofaOpeTemplateFrameBin::dispose)
	 */
	ofa_tvbin_set_write_settings( OFA_TVBIN( view ), FALSE );
	ofa_tvbin_set_name( OFA_TVBIN( view ), priv->settings_prefix );
	setup_columns( view );

	return( view );
}

/**
 * ofa_ope_template_treeview_get_ledger:
 * @view: this #ofaOpeTemplateTreeview instance.
 *
 * Returns: the ledger mnemonic associated to this @view.
 */
const gchar *
ofa_ope_template_treeview_get_ledger( ofaOpeTemplateTreeview *view )
{
	ofaOpeTemplateTreeviewPrivate *priv;

	g_return_val_if_fail( view && OFA_IS_OPE_TEMPLATE_TREEVIEW( view ), NULL );

	priv = ofa_ope_template_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->ledger );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaOpeTemplateTreeview *self )
{
	static const gchar *thisfn = "ofa_ope_template_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_MNEMO,         _( "Mnemo" ),        _( "Identifier" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_CRE_USER,      _( "Cre.user" ),     _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_CRE_STAMP,     _( "Cre.stamp" ),    _( "Last update timestamp" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), OPE_TEMPLATE_COL_LABEL,         _( "Label" ),            NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_LEDGER,        _( "Ledger" ),           NULL );
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_LEDGER_LOCKED, _( "Led.locked" ),   _( "Ledger is locked" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_REF,           _( "Ref." ),         _( "Piece reference" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_REF_LOCKED,    _( "Ref.locked" ),   _( "Reference is locked" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_REF_MANDATORY, _( "Ref.mandat" ),   _( "Reference is mandatory" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_PAM_ROW,       _( "PAM" ),          _( "Target of the mean of paiement" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_HAVE_TIERS,    _( "Have.tiers" ),   _( "Have tiers" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_TIERS,         _( "Tiers" ),            NULL );
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_TIERS_LOCKED,  _( "Tiers.locked" ), _( "Tiers is locked" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_HAVE_QPPRO,    _( "Have.pro" ),     _( "Have professional part" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_QPPRO,         _( "Pro.part" ),     _( "Professional part" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_QPPRO_LOCKED,  _( "Pro.locked" ),   _( "Professional part is locked" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_HAVE_RULE,     _( "Have.rule" ),    _( "Have rule" ));
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_RULE,          _( "Rule" ),             NULL );
	ofa_tvbin_add_column_text_c ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_RULE_LOCKED,   _( "Rule.locked" ),  _( "Rule is locked" ));
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), OPE_TEMPLATE_COL_NOTES,         _( "Notes" ),            NULL );
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_NOTES_PNG,        "",               _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_UPD_USER,      _( "Upd.user" ),     _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), OPE_TEMPLATE_COL_UPD_STAMP,     _( "Upd.stamp" ),    _( "Last update timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), OPE_TEMPLATE_COL_LABEL );
}

static void
on_selection_changed( ofaOpeTemplateTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-opechanged" );
}

static void
on_selection_activated( ofaOpeTemplateTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-opeactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaOpeTemplateTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-opedelete" );
}

/*
 * OpeTemplate may be %NULL when selection is empty (on 'ofa-opechanged' signal)
 */
static void
get_and_send( ofaOpeTemplateTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoOpeTemplate *template;

	template = get_selected_with_selection( self, selection );
	g_return_if_fail( !template || OFO_IS_OPE_TEMPLATE( template ));

	g_signal_emit_by_name( self, signal, template );
}

/**
 * ofa_ope_template_treeview_get_selected:
 * @view: this #ofaOpeTemplateTreeview instance.
 *
 * Return: the currently selected BAT file, or %NULL.
 */
ofoOpeTemplate *
ofa_ope_template_treeview_get_selected( ofaOpeTemplateTreeview *view )
{
	static const gchar *thisfn = "ofa_ope_template_treeview_get_selected";
	ofaOpeTemplateTreeviewPrivate *priv;
	GtkTreeSelection *selection;
	ofoOpeTemplate *template;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_OPE_TEMPLATE_TREEVIEW( view ), NULL );

	priv = ofa_ope_template_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	template = get_selected_with_selection( view, selection );

	return( template );
}

/*
 * get_selected_with_selection:
 * @view: this #ofaOpeTemplateTreeview instance.
 * @selection: the current #GtkTreeSelection.
 *
 * Return: the currently selected #ofoOpeTemplate, or %NULL.
 */
static ofoOpeTemplate *
get_selected_with_selection( ofaOpeTemplateTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoOpeTemplate *template;

	template = NULL;
	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, OPE_TEMPLATE_COL_OBJECT, &template, -1 );
		g_return_val_if_fail( template && OFO_IS_OPE_TEMPLATE( template ), NULL );
		g_object_unref( template );
	}

	return( template );
}

/**
 * ofa_ope_template_treeview_set_selected:
 * @view: this #ofaOpeTemplateTreeview instance.
 * @template_id: the template identifier to be selected.
 *
 * Selects the template identified by @template_id, or the closest row if
 * the identifier is not visible in this @view.
 */
void
ofa_ope_template_treeview_set_selected( ofaOpeTemplateTreeview *view, const gchar *template_id )
{
	static const gchar *thisfn = "ofa_ope_template_treeview_set_selected";
	ofaOpeTemplateTreeviewPrivate *priv;
	GtkWidget *treeview;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gchar *row_id;
	gint cmp;

	g_debug( "%s: view=%p, template_id=%s", thisfn, ( void * ) view, template_id );

	g_return_if_fail( view && OFA_IS_OPE_TEMPLATE_TREEVIEW( view ));

	priv = ofa_ope_template_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	treeview = ofa_tvbin_get_tree_view( OFA_TVBIN( view ));
	if( treeview ){
		tmodel = gtk_tree_view_get_model( GTK_TREE_VIEW( treeview ));
		if( gtk_tree_model_get_iter_first( tmodel, &iter )){
			while( TRUE ){
				gtk_tree_model_get( tmodel, &iter, OPE_TEMPLATE_COL_MNEMO, &row_id, -1 );
				cmp = my_collate( row_id, template_id );
				//g_debug( "row_id=%s, template_id=%s, cmp=%d", row_id, template_id, cmp );
				g_free( row_id );
				if( cmp >= 0 ){
					ofa_tvbin_select_row( OFA_TVBIN( view ), &iter );
					break;
				}
				if( !gtk_tree_model_iter_next( tmodel, &iter )){
					ofa_tvbin_select_row( OFA_TVBIN( view ), &iter );
					break;
				}
			}
		}
	}
}

/*
 * We are here filtering the child model of the GtkTreeModelFilter,
 * which happens to be the sort model, itself being built on top of
 * the ofaTreeStore
 */
static gboolean
tvbin_v_filter( const ofaTVBin *tvbin, GtkTreeModel *model, GtkTreeIter *iter )
{
	ofaOpeTemplateTreeviewPrivate *priv;
	gchar *ledger;
	gint cmp;

	priv = ofa_ope_template_treeview_get_instance_private( OFA_OPE_TEMPLATE_TREEVIEW( tvbin ));

	gtk_tree_model_get( model, iter, OPE_TEMPLATE_COL_LEDGER, &ledger, -1 );
	cmp = my_collate( ledger, priv->ledger );
	g_free( ledger );

	return( cmp == 0 );
}

static gint
tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_ope_template_treeview_v_sort";
	GtkTreeModel *child_model;
	gint cmp;
	gchar *mnemoa, *labela, *ledgera, *ledlcka, *refa, *reflcka, *refmdta, *havtiera, *tiera, *tierlcka, *havproa, *proa,
			*prolcka, *havrula, *rula, *rullcka, *notesa, *creusera, *crestampa, *updusera, *updstampa;
	gchar *mnemob, *labelb, *ledgerb, *ledlckb, *refb, *reflckb, *refmdtb, *havtierb, *tierb, *tierlckb, *havprob, *prob,
			*prolckb, *havrulb, *rulb, *rullckb, *notesb, *creuserb, *crestampb, *upduserb, *updstampb;
	GdkPixbuf *pnga, *pngb;
	gboolean handled;

	gtk_tree_model_get( tmodel, a,
			OPE_TEMPLATE_COL_MNEMO,         &mnemoa,
			OPE_TEMPLATE_COL_CRE_USER,      &creusera,
			OPE_TEMPLATE_COL_CRE_STAMP,     &crestampa,
			OPE_TEMPLATE_COL_LABEL,         &labela,
			OPE_TEMPLATE_COL_LEDGER,        &ledgera,
			OPE_TEMPLATE_COL_LEDGER_LOCKED, &ledlcka,
			OPE_TEMPLATE_COL_REF,           &refa,
			OPE_TEMPLATE_COL_REF_LOCKED,    &reflcka,
			OPE_TEMPLATE_COL_REF_MANDATORY, &refmdta,
			OPE_TEMPLATE_COL_HAVE_TIERS,    &havtiera,
			OPE_TEMPLATE_COL_TIERS,         &tiera,
			OPE_TEMPLATE_COL_TIERS_LOCKED,  &tierlcka,
			OPE_TEMPLATE_COL_HAVE_QPPRO,    &havproa,
			OPE_TEMPLATE_COL_QPPRO,         &proa,
			OPE_TEMPLATE_COL_QPPRO_LOCKED,  &prolcka,
			OPE_TEMPLATE_COL_HAVE_RULE,     &havrula,
			OPE_TEMPLATE_COL_RULE,          &rula,
			OPE_TEMPLATE_COL_RULE_LOCKED,   &rullcka,
			OPE_TEMPLATE_COL_NOTES,         &notesa,
			OPE_TEMPLATE_COL_NOTES_PNG,     &pnga,
			OPE_TEMPLATE_COL_UPD_USER,      &updusera,
			OPE_TEMPLATE_COL_UPD_STAMP,     &updstampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			OPE_TEMPLATE_COL_MNEMO,         &mnemob,
			OPE_TEMPLATE_COL_CRE_USER,      &creuserb,
			OPE_TEMPLATE_COL_CRE_STAMP,     &crestampb,
			OPE_TEMPLATE_COL_LABEL,         &labelb,
			OPE_TEMPLATE_COL_LEDGER_LOCKED, &ledlckb,
			OPE_TEMPLATE_COL_LEDGER,        &ledgerb,
			OPE_TEMPLATE_COL_REF,           &refb,
			OPE_TEMPLATE_COL_REF_LOCKED,    &reflckb,
			OPE_TEMPLATE_COL_REF_MANDATORY, &refmdtb,
			OPE_TEMPLATE_COL_HAVE_TIERS,    &havtierb,
			OPE_TEMPLATE_COL_TIERS,         &tierb,
			OPE_TEMPLATE_COL_TIERS_LOCKED,  &tierlckb,
			OPE_TEMPLATE_COL_HAVE_QPPRO,    &havprob,
			OPE_TEMPLATE_COL_QPPRO,         &prob,
			OPE_TEMPLATE_COL_QPPRO_LOCKED,  &prolckb,
			OPE_TEMPLATE_COL_HAVE_RULE,     &havrulb,
			OPE_TEMPLATE_COL_RULE,          &rulb,
			OPE_TEMPLATE_COL_RULE_LOCKED,   &rullckb,
			OPE_TEMPLATE_COL_NOTES,         &notesb,
			OPE_TEMPLATE_COL_NOTES_PNG,     &pngb,
			OPE_TEMPLATE_COL_UPD_USER,      &upduserb,
			OPE_TEMPLATE_COL_UPD_STAMP,     &updstampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case OPE_TEMPLATE_COL_MNEMO:
			cmp = my_collate( mnemoa, mnemob );
			break;
		case OPE_TEMPLATE_COL_CRE_USER:
			cmp = my_collate( creusera, creuserb );
			break;
		case OPE_TEMPLATE_COL_CRE_STAMP:
			cmp = my_collate( crestampa, crestampb );
			break;
		case OPE_TEMPLATE_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case OPE_TEMPLATE_COL_LEDGER:
			cmp = my_collate( ledgera, ledgerb );
			break;
		case OPE_TEMPLATE_COL_LEDGER_LOCKED:
			cmp = my_collate( ledlcka, ledlckb );
			break;
		case OPE_TEMPLATE_COL_REF:
			cmp = my_collate( refa, refb );
			break;
		case OPE_TEMPLATE_COL_REF_LOCKED:
			cmp = my_collate( reflcka, reflckb );
			break;
		case OPE_TEMPLATE_COL_REF_MANDATORY:
			cmp = my_collate( refmdta, refmdtb );
			break;
		case OPE_TEMPLATE_COL_HAVE_TIERS:
			cmp = my_collate( havtiera, havtierb );
			break;
		case OPE_TEMPLATE_COL_TIERS:
			cmp = my_collate( tiera, tierb );
			break;
		case OPE_TEMPLATE_COL_TIERS_LOCKED:
			cmp = my_collate( tierlcka, tierlckb );
			break;
		case OPE_TEMPLATE_COL_HAVE_QPPRO:
			cmp = my_collate( havproa, havprob );
			break;
		case OPE_TEMPLATE_COL_QPPRO:
			cmp = my_collate( proa, prob );
			break;
		case OPE_TEMPLATE_COL_QPPRO_LOCKED:
			cmp = my_collate( prolcka, prolckb );
			break;
		case OPE_TEMPLATE_COL_HAVE_RULE:
			cmp = my_collate( havrula, havrulb );
			break;
		case OPE_TEMPLATE_COL_RULE:
			cmp = my_collate( rula, rulb );
			break;
		case OPE_TEMPLATE_COL_RULE_LOCKED:
			cmp = my_collate( rullcka, rullckb );
			break;
		case OPE_TEMPLATE_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case OPE_TEMPLATE_COL_NOTES_PNG:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		case OPE_TEMPLATE_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case OPE_TEMPLATE_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		default:
			handled = FALSE;
			if( GTK_IS_TREE_MODEL_FILTER( tmodel )){
				child_model = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( tmodel ));
				if( OFA_IS_ISTORE( child_model )){
					handled = ofa_istore_sort( OFA_ISTORE( child_model ), tmodel, a, b, column_id, &cmp );
				}
			}
			if( !handled ){
				g_warning( "%s: unhandled column: %d", thisfn, column_id );
			}
			break;
	}

	g_free( mnemoa );
	g_free( creusera );
	g_free( crestampa );
	g_free( labela );
	g_free( ledgera );
	g_free( ledlcka );
	g_free( refa );
	g_free( reflcka );
	g_free( refmdta );
	g_free( havtiera );
	g_free( tiera );
	g_free( tierlcka );
	g_free( havproa );
	g_free( proa );
	g_free( prolcka );
	g_free( havrula );
	g_free( rula );
	g_free( rullcka );
	g_free( notesa );
	g_free( updusera );
	g_free( updstampa );
	g_clear_object( &pnga );

	g_free( mnemob );
	g_free( creuserb );
	g_free( crestampb );
	g_free( labelb );
	g_free( ledgerb );
	g_free( ledlckb );
	g_free( refb );
	g_free( reflckb );
	g_free( refmdtb );
	g_free( havtierb );
	g_free( tierb );
	g_free( tierlckb );
	g_free( havprob );
	g_free( prob );
	g_free( prolckb );
	g_free( havrulb );
	g_free( rulb );
	g_free( rullckb );
	g_free( notesb );
	g_free( upduserb );
	g_free( updstampb );
	g_clear_object( &pngb );

	return( cmp );
}
