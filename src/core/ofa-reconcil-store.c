/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include "my/my-date.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-igetter.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-concil-id.h"

#include "core/ofa-reconcil-store.h"

/* private instance data
 */
typedef struct {
	gboolean     dispose_has_run;

	/* initialization
	 */
	ofaIGetter  *getter;

	/* runtime
	 */
	GList       *signaler_handlers;

	/* loaded account
	 */
	ofoAccount  *account;
	ofoCurrency *currency;

	/* when loading the store by concil
	 */
	ofxCounter   concil_count;
	GList       *concil_bats;

	/* updating from hub signaling system
	 */
	gchar       *acc_number;
	gchar       *acc_currency;
}
	ofaReconcilStorePrivate;

/* store data types
 */
static GType st_col_types[RECONCIL_N_COLUMNS] = {
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* dope, deffect, label */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* ref, currency, ledger */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* ope_template, account, debit */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* credit, ope_number, stlmt_number */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,		/* stlmt_user, stlmt_stamp, ent_number_str */
	G_TYPE_ULONG,  G_TYPE_STRING, G_TYPE_STRING,		/* ent_number_int, upd_user, upd_stamp */
	G_TYPE_STRING, G_TYPE_INT,    G_TYPE_STRING,		/* status_str, status_int, rule */
	G_TYPE_INT,    G_TYPE_STRING,						/* rule_int, tiers */
	G_TYPE_STRING, G_TYPE_ULONG, G_TYPE_STRING, 		/* concil_number_str, concil_number_int, concil_date */
	G_TYPE_STRING,										/* concil_type */
	G_TYPE_OBJECT										/* the #ofoEntry or #ofoBatLine */
};

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaReconcilStore *self );
static void     entry_insert_row( ofaReconcilStore *self, const ofoEntry *entry, gboolean search, GtkTreeIter *parent_iter, GtkTreeIter *inserted_iter, ofxCounter exclude );
static void     entry_get_amount_strs( ofaReconcilStore *self, const ofoEntry *entry, gchar **sdebit, gchar **scredit );
static void     entry_set_row_by_iter( ofaReconcilStore *store, const ofoEntry *entry, GtkTreeIter *iter );
static void     bat_insert_row( ofaReconcilStore *self, ofoBatLine *batline, gboolean search, GtkTreeIter *parent_iter, GtkTreeIter *inserted_iter, ofxCounter exclude );
static void     bat_get_amount_strs( ofaReconcilStore *self, ofoBatLine *batline, gchar **sdebit, gchar **scredit );
static void     bat_set_row_by_iter( ofaReconcilStore *self, ofoBatLine *batline, GtkTreeIter *iter );
static void     concil_insert_row( ofoConcil *concil, const gchar *type, ofxCounter id, ofaReconcilStore *self );
static void     concil_set_row_by_iter( ofaReconcilStore *self, ofaIConcil *iconcil, GtkTreeIter *iter );
static void     concil_set_row_with_data( ofaReconcilStore *self, ofxCounter id, const GDate *date, GtkTreeIter *iter );
static void     insert_with_remediation( ofaReconcilStore *self, GtkTreeIter *parent_iter, GtkTreeIter *inserted_iter, gboolean parent_preferred );
static void     move_children_rec( ofaReconcilStore *self, GtkTreeRowReference *target_ref, GtkTreeRowReference *source_ref );
static gboolean search_for_parent_by_amount( ofaReconcilStore *self, ofoBase *object, GtkTreeIter *iter, ofxCounter exclude );
static gboolean find_closest_date( ofaReconcilStore *self, ofoBase *object, const GDate *obj_date, ofoBase *row_object, gint *spread, gboolean *spread_set );
static gboolean search_for_parent_by_concil( ofaReconcilStore *self, ofoConcil *concil, GtkTreeIter *iter, ofxCounter exclude );
static gboolean search_for_entry_by_number( ofaReconcilStore *self, ofxCounter number, GtkTreeIter *iter );
static gboolean search_for_entry_by_number_rec( ofaReconcilStore *self, ofxCounter number, GtkTreeIter *iter );
static gboolean find_row_by_concil_member( ofaReconcilStore *self, const gchar *type, ofxCounter id, GtkTreeIter *iter );
static gboolean find_row_by_concil_member_rec( ofaReconcilStore *self, const gchar *type, ofxCounter id, GtkTreeIter *iter );
static void     insert_new_entry( ofaReconcilStore *self, ofoEntry *entry );
static void     set_account_new_id( ofaReconcilStore *self, const gchar *prev_id, const gchar *new_id );
static void     set_currency_new_id( ofaReconcilStore *self, const gchar *prev_id, const gchar *new_id );
static void     set_ledger_new_id( ofaReconcilStore *self, const gchar *prev_id, const gchar *new_id );
static void     set_ope_template_new_id( ofaReconcilStore *self, const gchar *prev_id, const gchar *new_id );
static void     update_column( ofaReconcilStore *self, const gchar *prev_id, const gchar *new_id, guint column );
static void     update_column_rec( ofaReconcilStore *self, const gchar *prev_id, const gchar *new_id, guint column, GtkTreeIter *iter );
static void     signaler_connect_to_signaling_system( ofaReconcilStore *self );
static void     signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaReconcilStore *self );
static void     signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaReconcilStore *self );
static void     signaler_on_updated_entry( ofaReconcilStore *self, ofoEntry *entry );
static void     signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaReconcilStore *self );
static void     signaler_on_deleted_concil( ofaReconcilStore *self, ofoConcil *concil );
static void     signaler_on_deleted_concil_cb( ofoConcil *concil, const gchar *type, ofxCounter id, ofaReconcilStore *self );
static void     signaler_on_deleted_entry( ofaReconcilStore *self, ofoEntry *entry );

G_DEFINE_TYPE_EXTENDED( ofaReconcilStore, ofa_reconcil_store, OFA_TYPE_TREE_STORE, 0,
		G_ADD_PRIVATE( ofaReconcilStore ))

static void
reconcil_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_reconcil_store_finalize";
	ofaReconcilStorePrivate *priv;

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECONCIL_STORE( instance ));

	/* free data members here */
	priv = ofa_reconcil_store_get_instance_private( OFA_RECONCIL_STORE( instance ));

	g_list_free( priv->concil_bats );
	g_free( priv->acc_number );
	g_free( priv->acc_currency );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_store_parent_class )->finalize( instance );
}

static void
reconcil_store_dispose( GObject *instance )
{
	ofaReconcilStorePrivate *priv;
	ofaISignaler *signaler;

	g_return_if_fail( instance && OFA_IS_RECONCIL_STORE( instance ));

	priv = ofa_reconcil_store_get_instance_private( OFA_RECONCIL_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* disconnect from ofaISignaler signaling system */
		signaler = ofa_igetter_get_signaler( priv->getter );
		ofa_isignaler_disconnect_handlers( signaler, &priv->signaler_handlers );

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_store_parent_class )->dispose( instance );
}

static void
ofa_reconcil_store_init( ofaReconcilStore *self )
{
	static const gchar *thisfn = "ofa_reconcil_store_init";
	ofaReconcilStorePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECONCIL_STORE( self ));

	priv = ofa_reconcil_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->signaler_handlers = NULL;
}

static void
ofa_reconcil_store_class_init( ofaReconcilStoreClass *klass )
{
	static const gchar *thisfn = "ofa_reconcil_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = reconcil_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = reconcil_store_finalize;
}

/**
 * ofa_reconcil_store_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a reference to a new #ofaReconcilStore, which should be
 * released by the caller.
 */
ofaReconcilStore *
ofa_reconcil_store_new( ofaIGetter *getter )
{
	ofaReconcilStore *store;
	ofaReconcilStorePrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	store = g_object_new( OFA_TYPE_RECONCIL_STORE, NULL );

	priv = ofa_reconcil_store_get_instance_private( store );

	priv->getter = getter;

	gtk_tree_store_set_column_types(
			GTK_TREE_STORE( store ), RECONCIL_N_COLUMNS, st_col_types );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );

	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( store ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	signaler_connect_to_signaling_system( store );

	return( store );
}

/*
 * sorting the store per entry number ascending
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaReconcilStore *self )
{
	gchar *typea, *typeb;
	ofxCounter numa, numb;
	gint cmp;

	gtk_tree_model_get( tmodel, a, RECONCIL_COL_CONCIL_TYPE, &typea, RECONCIL_COL_ENT_NUMBER_I, &numa, -1 );
	gtk_tree_model_get( tmodel, b, RECONCIL_COL_CONCIL_TYPE, &typeb, RECONCIL_COL_ENT_NUMBER_I, &numb, -1 );

	cmp = my_collate( typea, typeb );

	if( cmp == 0 ){
		cmp = numa < numb ? -1 : ( numa > numb ? 1 : 0 );
	}

	g_free( typea );
	g_free( typeb );

	return( cmp );
}

/**
 * ofa_reconcil_store_load_by_account:
 * store: this #ofaReconcilStore instance.
 * @account: the account identifier to be loaded.
 *
 * Loads the entries for this @account.
 *
 * Returns: the count of inserted entries.
 */
ofxCounter
ofa_reconcil_store_load_by_account( ofaReconcilStore *store, const gchar *account )
{
	ofaReconcilStorePrivate *priv;
	GList *dataset, *it;
	ofxCounter count;
	ofoEntry *entry;

	g_return_val_if_fail( store && OFA_IS_RECONCIL_STORE( store ), 0 );

	priv = ofa_reconcil_store_get_instance_private( store );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	/* keep some reference datas about this account */
	priv->currency = NULL;

	g_free( priv->acc_number );
	priv->acc_number = NULL;
	g_free( priv->acc_currency );
	priv->acc_currency = NULL;

	priv->account = ofo_account_get_by_number( priv->getter, account );
	if( priv->account ){
		priv->acc_number = g_strdup( account );
		priv->acc_currency = g_strdup( ofo_account_get_currency( priv->account ));
		priv->currency = ofo_currency_get_by_code( priv->getter, priv->acc_currency );
	}

	/* recall the unique dataset (loaded only once) */
	dataset = ofo_entry_get_dataset( priv->getter );
	count = 0;

	for( it=dataset ; it ; it=it->next ){
		entry = OFO_ENTRY( it->data );
		if( !my_collate( ofo_entry_get_account( entry ), account )){
			entry_insert_row( store, entry, TRUE, NULL, NULL, 0 );
			count += 1;
		}
	}

	return( count );
}

/*
 * @search: whether to search for the better position
 *  if %FALSE, will just insert at level zero
 * @parent_iter: if set, insert as a child of @parent_iter.
 * @inserted_iter: [out][allow-none]: iter of newly inserted row
 * @exclude: do not insert the new entry as a child of @exclude entry
 *
 * Insert the entry:
 *
 * - as a child of an existing conciliation group if the entry is
 *   conciliated, and a member of the conciliation group is already
 *   loaded
 *
 * - at level 0, as the child of an unconciliated line with a compatible
 *   amount (as a particular case, we try to have entry as parent, and
 *   batline as child)
 */
static void
entry_insert_row( ofaReconcilStore *self,
		const ofoEntry *entry, gboolean search, GtkTreeIter *parent_iter, GtkTreeIter *inserted_iter, ofxCounter exclude )
{
	ofoConcil *concil;
	GtkTreeIter row_parent, row_inserted;

	if( ofo_entry_get_status( entry ) != ENT_STATUS_DELETED ){

		if( parent_iter ){
			row_parent = *parent_iter;
			insert_with_remediation( self, &row_parent, &row_inserted, TRUE );

		} else if( search ){
			concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));

			if( concil && search_for_parent_by_concil( self, concil, &row_parent, exclude )){
				insert_with_remediation( self, &row_parent, &row_inserted, TRUE );

			} else if( !concil && search_for_parent_by_amount( self, OFO_BASE( entry ), &row_parent, exclude )){
				insert_with_remediation( self, &row_parent, &row_inserted, TRUE );

			} else {
				gtk_tree_store_insert( GTK_TREE_STORE( self ), &row_inserted, NULL, -1 );
			}

		} else {
			gtk_tree_store_insert( GTK_TREE_STORE( self ), &row_inserted, NULL, -1 );
		}

		entry_set_row_by_iter( self, entry, &row_inserted );

		if( inserted_iter ){
			*inserted_iter = row_inserted;
		}
	}
}

static void
entry_set_row_by_iter( ofaReconcilStore *self, const ofoEntry *entry, GtkTreeIter *iter )
{
	ofaReconcilStorePrivate *priv;
	gchar *sdope, *sdeff, *sdeb, *scre, *sopenum, *ssetnum, *ssetstamp, *sentnum, *supdstamp, *stiers, *sconcnum, *sconcdate;
	const gchar *cstr, *cref, *csetuser, *cupduser;
	ofxCounter counter, entnum, concilnum;
	ofeEntryStatus status;
	ofeEntryRule rule;
	ofoConcil *concil;
	const GDate *dval;

	priv = ofa_reconcil_store_get_instance_private( self );

	sdope = my_date_to_str( ofo_entry_get_dope( entry ), ofa_prefs_date_get_display_format( priv->getter ));
	sdeff = my_date_to_str( ofo_entry_get_deffect( entry ), ofa_prefs_date_get_display_format( priv->getter ));

	cstr = ofo_entry_get_ref( entry );
	cref = cstr ? cstr : "";

	entry_get_amount_strs( self, entry, &sdeb, &scre );

	counter = ofo_entry_get_ope_number( entry );
	sopenum = counter > 0 ? g_strdup_printf( "%lu", counter ) : g_strdup( "" );

	counter = ofo_entry_get_settlement_number( entry );
	ssetnum = counter ? g_strdup_printf( "%lu", counter ) : g_strdup( "" );
	if( counter > 0 ){
		csetuser = ofo_entry_get_settlement_user( entry );
		ssetstamp = my_stamp_to_str( ofo_entry_get_settlement_stamp( entry ), MY_STAMP_DMYYHM );
	} else {
		csetuser = "";
		ssetstamp = g_strdup( "" );
	}

	entnum = ofo_entry_get_number( entry );
	sentnum = g_strdup_printf( "%lu", entnum );

	cstr = ofo_entry_get_upd_user( entry );
	cupduser = cstr ? cstr : "";
	supdstamp = my_stamp_to_str( ofo_entry_get_upd_stamp( entry ), MY_STAMP_DMYYHM );

	status = ofo_entry_get_status( entry );
	rule = ofo_entry_get_rule( entry );

	counter = ofo_entry_get_tiers( entry );
	stiers = counter > 0 ? g_strdup_printf( "%lu", counter ) : g_strdup( "" );

	concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
	concilnum = concil ? ofo_concil_get_id( concil ) : 0;
	sconcnum = concilnum > 0 ? g_strdup_printf( "%lu", concilnum ) : g_strdup( "" );

	dval = concil ? ofo_concil_get_dval( concil ) : NULL;
	sconcdate = dval ? my_date_to_str( dval, ofa_prefs_date_get_display_format( priv->getter )) : g_strdup( "" );

	gtk_tree_store_set(
				GTK_TREE_STORE( self ),
				iter,
				RECONCIL_COL_DOPE,            sdope,
				RECONCIL_COL_DEFFECT,         sdeff,
				RECONCIL_COL_LABEL,           ofo_entry_get_label( entry ),
				RECONCIL_COL_REF,             cref,
				RECONCIL_COL_CURRENCY,        ofo_entry_get_currency( entry ),
				RECONCIL_COL_LEDGER,          ofo_entry_get_ledger( entry ),
				RECONCIL_COL_OPE_TEMPLATE,    ofo_entry_get_ope_template( entry ),
				RECONCIL_COL_ACCOUNT,         ofo_entry_get_account( entry ),
				RECONCIL_COL_DEBIT,           sdeb,
				RECONCIL_COL_CREDIT,          scre,
				RECONCIL_COL_OPE_NUMBER,      sopenum,
				RECONCIL_COL_STLMT_NUMBER,    ssetnum,
				RECONCIL_COL_STLMT_USER,      csetuser,
				RECONCIL_COL_STLMT_STAMP,     ssetstamp,
				RECONCIL_COL_ENT_NUMBER,      sentnum,
				RECONCIL_COL_ENT_NUMBER_I,    entnum,
				RECONCIL_COL_UPD_USER,        cupduser,
				RECONCIL_COL_UPD_STAMP,       supdstamp,
				RECONCIL_COL_STATUS,          ofo_entry_status_get_abr( status ),
				RECONCIL_COL_STATUS_I,        status,
				RECONCIL_COL_RULE,            ofo_entry_rule_get_abr( rule ),
				RECONCIL_COL_RULE_I,          rule,
				RECONCIL_COL_TIERS,           stiers,
				RECONCIL_COL_CONCIL_NUMBER,   sconcnum,
				RECONCIL_COL_CONCIL_NUMBER_I, concilnum,
				RECONCIL_COL_CONCIL_DATE,     sconcdate,
				RECONCIL_COL_CONCIL_TYPE,     ofa_iconcil_get_instance_type( OFA_ICONCIL( entry )),
				RECONCIL_COL_OBJECT,          entry,
				-1 );

	g_free( sconcdate );
	g_free( sconcnum );
	g_free( stiers );
	g_free( supdstamp );
	g_free( sentnum );
	g_free( ssetstamp );
	g_free( ssetnum );
	g_free( sopenum );
	g_free( scre );
	g_free( sdeb );
	g_free( sdeff );
	g_free( sdope );

	concil_set_row_by_iter( self, OFA_ICONCIL( entry ), iter );
}

static void
entry_get_amount_strs( ofaReconcilStore *self, const ofoEntry *entry, gchar **sdebit, gchar **scredit )
{
	ofaReconcilStorePrivate *priv;
	ofxAmount amount;

	priv = ofa_reconcil_store_get_instance_private( self );

	amount = ofo_entry_get_debit( entry );
	*sdebit = amount ? ofa_amount_to_str( amount, priv->currency, priv->getter ) : g_strdup( "" );

	amount = ofo_entry_get_credit( entry );
	*scredit = amount ? ofa_amount_to_str( amount, priv->currency, priv->getter ) : g_strdup( "" );
}

/**
 * ofa_reconcil_store_load_by_bat:
 * store: this #ofaReconcilStore instance.
 * @bat_id: the BAT identifier to be loaded.
 *
 * Loads the lines for this @bat_id.
 *
 * Returns: the count of loaded lines.
 */
ofxCounter
ofa_reconcil_store_load_by_bat( ofaReconcilStore *store, ofxCounter bat_id )
{
	ofaReconcilStorePrivate *priv;
	GList *dataset, *it;
	ofxCounter count;
	ofoBatLine *batline;

	g_return_val_if_fail( store && OFA_IS_RECONCIL_STORE( store ), 0 );
	g_return_val_if_fail( bat_id > 0, 0 );

	priv = ofa_reconcil_store_get_instance_private( store );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	dataset = ofo_bat_line_get_dataset( priv->getter, bat_id );

	for( it=dataset ; it ; it=it->next ){
		batline = OFO_BAT_LINE( it->data );
		bat_insert_row( store, batline, TRUE, NULL, NULL, 0 );
	}

	count = g_list_length( dataset );
	ofo_bat_line_free_dataset( dataset );

	return( count );
}

/*
 * @search: whether to search for the better position
 *  if %FALSE, will just insert at level zero
 * @parent_iter: if set, insert as a child of @parent_iter.
 * @inserted_iter: [out][allow-none]: iter of newly inserted row
 * @exclude: do not insert the new entry as a child of @exclude entry
 *
 * Insert the line:
 *
 * - as a child of an existing conciliation group if the line is
 *   conciliated, and a member of the conciliation group is already
 *   loaded
 *
 * - at level 0, as the child of an unconciliated row with a compatible
 *   amount (as a particular case, we try to have entry as parent, and
 *   batline as child)
 */
static void
bat_insert_row( ofaReconcilStore *self,
		ofoBatLine *batline, gboolean search, GtkTreeIter *parent_iter, GtkTreeIter *inserted_iter, ofxCounter exclude )
{
	ofoConcil *concil;
	GtkTreeIter row_parent, row_inserted;

	row_parent.stamp = 0;

	if( parent_iter ){
		row_parent = *parent_iter;
		insert_with_remediation( self, &row_parent, &row_inserted, FALSE );

	} else if( search ){
		concil = ofa_iconcil_get_concil( OFA_ICONCIL( batline ));

		if( concil && search_for_parent_by_concil( self, concil, &row_parent, exclude )){
			insert_with_remediation( self, &row_parent, &row_inserted, FALSE );

		} else if( !concil && search_for_parent_by_amount( self, OFO_BASE( batline ), &row_parent, exclude )){
			insert_with_remediation( self, &row_parent, &row_inserted, FALSE );

		} else {
			gtk_tree_store_insert( GTK_TREE_STORE( self ), &row_inserted, NULL, -1 );
		}

	} else {
		gtk_tree_store_insert( GTK_TREE_STORE( self ), &row_inserted, NULL, -1 );
	}

	bat_set_row_by_iter( self, batline, &row_inserted );

	if( inserted_iter ){
		*inserted_iter = row_inserted;
	}
}

static void
bat_set_row_by_iter( ofaReconcilStore *self, ofoBatLine *batline, GtkTreeIter *iter )
{
	ofaReconcilStorePrivate *priv;
	gchar *sdeff, *sdope, *sblnum, *sdeb, *scre, *scur, *sconcnum, *sconcdate, *suser, *sstamp;
	ofxCounter batline_number, concilnum, bat_id;
	const gchar *cstr;
	ofoConcil *concil;
	const GDate *dval;
	ofoBat *bat;

	priv = ofa_reconcil_store_get_instance_private( self );

	sdeff = my_date_to_str( ofo_bat_line_get_deffect( batline ), ofa_prefs_date_get_display_format( priv->getter ));
	sdope = my_date_to_str( ofo_bat_line_get_dope( batline ), ofa_prefs_date_get_display_format( priv->getter ));

	bat_get_amount_strs( self, batline, &sdeb, &scre );

	batline_number = ofo_bat_line_get_line_id( batline );
	sblnum = g_strdup_printf( "%lu", batline_number );

	cstr = ofo_bat_line_get_currency( batline );
	scur = g_strdup( cstr ? cstr : "" );

	concil = ofa_iconcil_get_concil( OFA_ICONCIL( batline ));
	concilnum = concil ? ofo_concil_get_id( concil ) : 0;
	sconcnum = concilnum ? g_strdup_printf( "%lu", concilnum ) : g_strdup( "" );

	dval = concil ? ofo_concil_get_dval( concil ) : NULL;
	sconcdate = dval ? my_date_to_str( dval, ofa_prefs_date_get_display_format( priv->getter )) : g_strdup( "" );

	bat_id = ofo_bat_line_get_bat_id( batline );
	bat = ofo_bat_get_by_id( priv->getter, bat_id );
	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	cstr = ofo_bat_get_upd_user( bat );
	suser = g_strdup( cstr ? cstr : "" );
	sstamp = my_stamp_to_str( ofo_bat_get_upd_stamp( bat ), MY_STAMP_DMYYHM );

	gtk_tree_store_set(
				GTK_TREE_STORE( self ),
				iter,
				RECONCIL_COL_DOPE,            sdope,
				RECONCIL_COL_DEFFECT,         sdeff,
				RECONCIL_COL_LABEL,           ofo_bat_line_get_label( batline ),
				RECONCIL_COL_REF,             ofo_bat_line_get_ref( batline ),
				RECONCIL_COL_CURRENCY,        scur,
				RECONCIL_COL_ENT_NUMBER,      sblnum,
				RECONCIL_COL_ENT_NUMBER_I,    batline_number,
				RECONCIL_COL_DEBIT,           sdeb,
				RECONCIL_COL_CREDIT,          scre,
				RECONCIL_COL_UPD_USER,        suser,
				RECONCIL_COL_UPD_STAMP,       sstamp,
				RECONCIL_COL_CONCIL_NUMBER,   sconcnum,
				RECONCIL_COL_CONCIL_NUMBER_I, concilnum,
				RECONCIL_COL_CONCIL_DATE,     sconcdate,
				RECONCIL_COL_CONCIL_TYPE,     ofa_iconcil_get_instance_type( OFA_ICONCIL( batline )),
				RECONCIL_COL_OBJECT,          batline,
				-1 );

	g_free( suser );
	g_free( sstamp );
	g_free( scur );
	g_free( sdope );
	g_free( sdeff );
	g_free( sdeb );
	g_free( scre );

	concil_set_row_by_iter( self, OFA_ICONCIL( batline ), iter );
}

static void
bat_get_amount_strs( ofaReconcilStore *self, ofoBatLine *batline, gchar **sdebit, gchar **scredit )
{
	ofaReconcilStorePrivate *priv;
	ofxAmount amount;

	priv = ofa_reconcil_store_get_instance_private( self );

	amount = ofo_bat_line_get_amount( batline );
	if( amount < 0 ){
		*sdebit = ofa_amount_to_str( -amount, priv->currency, priv->getter );
		*scredit = g_strdup( "" );
	} else {
		*sdebit = g_strdup( "" );
		*scredit = ofa_amount_to_str( amount, priv->currency, priv->getter );
	}
}

/**
 * ofa_reconcil_store_load_by_concil:
 * store: this #ofaReconcilStore instance.
 * @concil_id: the conciliation group identifier to be loaded.
 *
 * Loads the lines for this @concil_id.
 *
 * Returns: the count of loaded lines.
 */
ofxCounter
ofa_reconcil_store_load_by_concil( ofaReconcilStore *store, ofxCounter concil_id )
{
	ofaReconcilStorePrivate *priv;
	ofoConcil *concil;

	g_return_val_if_fail( store && OFA_IS_RECONCIL_STORE( store ), 0 );
	g_return_val_if_fail( concil_id > 0, 0 );

	priv = ofa_reconcil_store_get_instance_private( store );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	priv->concil_count = 0;
	priv->concil_bats = NULL;

	concil = ofo_concil_get_by_id( priv->getter, concil_id );

	if( concil ){
		ofo_concil_for_each_member( concil, ( ofoConcilEnumerate ) concil_insert_row, store );
	}

	return( priv->concil_count );
}

static void
concil_insert_row( ofoConcil *concil, const gchar *type, ofxCounter id, ofaReconcilStore *self )
{
	ofaReconcilStorePrivate *priv;
	ofxCounter bat_id;
	ofoEntry *entry;

	g_debug( "concil_insert_row: type=%s, id=%lu", type, id );

	priv = ofa_reconcil_store_get_instance_private( self );

	if( !my_collate( type, CONCIL_TYPE_BAT )){
		bat_id = ofo_bat_line_get_bat_id_from_bat_line_id( priv->getter, id );
		g_debug( "concil_insert_row: bat_id=%lu", bat_id );
		if( !g_list_find( priv->concil_bats, GUINT_TO_POINTER( bat_id ))){
			ofa_reconcil_store_load_by_bat( self, bat_id );
			priv->concil_bats = g_list_prepend( priv->concil_bats, GUINT_TO_POINTER( bat_id ));
		}

	} else {
		g_return_if_fail( my_collate( type, CONCIL_TYPE_ENTRY ) == 0 );
		entry = ofo_entry_get_by_number( priv->getter, id );
		entry_insert_row( self, entry, TRUE, NULL, NULL, 0 );
	}

	priv->concil_count += 1;
}

/**
 * ofa_reconcil_store_set_concil_data:
 * @store: this #ofaReconcilStore instance.
 * @concil_id: the conciliation identifier, or zero.
 * @concil_date: [allow-none]: the conciliation date, or %NULL.
 * @iter: the row iter to be updated.
 *
 * Update the conciliation datas of the @iter row.
 */
void
ofa_reconcil_store_set_concil_data( ofaReconcilStore *store, ofxCounter concil_id, const GDate *concil_date, GtkTreeIter *iter )
{
	ofaReconcilStorePrivate *priv;

	g_return_if_fail( store && OFA_IS_RECONCIL_STORE( store ));
	g_return_if_fail( iter );

	priv = ofa_reconcil_store_get_instance_private( store );

	g_return_if_fail( !priv->dispose_has_run );

	concil_set_row_with_data( store, concil_id, concil_date, iter );
}

static void
concil_set_row_by_iter( ofaReconcilStore *self, ofaIConcil *iconcil, GtkTreeIter *iter )
{
	ofoConcil *concil;
	ofxCounter concil_id;
	const GDate *date;

	concil = ofa_iconcil_get_concil( iconcil );
	concil_id = concil ? ofo_concil_get_id( concil ) : 0;
	date = concil ? ofo_concil_get_dval( concil ) : NULL;

	concil_set_row_with_data( self, concil_id, date, iter );
}

static void
concil_set_row_with_data( ofaReconcilStore *self, ofxCounter id, const GDate *date, GtkTreeIter *iter )
{
	ofaReconcilStorePrivate *priv;
	gchar *srappro, *snum;

	priv = ofa_reconcil_store_get_instance_private( self );

	srappro = date ? my_date_to_str( date, ofa_prefs_date_get_display_format( priv->getter )) : g_strdup( "" );
	snum = id > 0 ? g_strdup_printf( "%lu", id ) : g_strdup( "" );

	/* g_debug( "concil_number=%s", snum ); */

	gtk_tree_store_set(
				GTK_TREE_STORE( self ),
				iter,
				RECONCIL_COL_CONCIL_NUMBER,   snum,
				RECONCIL_COL_CONCIL_NUMBER_I, id,
				RECONCIL_COL_CONCIL_DATE,     srappro,
				-1 );

	g_free( snum );
	g_free( srappro );
}

/*
 * insert an empty row as a child of 'parent_iter' position,
 * returns the 'inserted_iter' of the newly inserted row
 *
 * if @parent_preferred is set *and* the parent row is a BAT line, then
 *  try to exchange the two rows, i.e. insert the new row at level 0,
 *  making the old parent a child of this new row.
 *
 * @oarent_iter: is expected to be valid here
 */
static void
insert_with_remediation( ofaReconcilStore *self, GtkTreeIter *parent_iter, GtkTreeIter *inserted_iter, gboolean parent_preferred )
{
	ofoBase *row_object, *parent_object;
	GtkTreePath *path;
	GtkTreeRowReference *parent_ref, *inserted_ref;

	if( 0 ){
		g_debug( "insert_with_remediation: parent_iter=%p, stamp=%u", parent_iter, parent_iter ? parent_iter->stamp : 0 );
	}

	if( 0 && parent_preferred ){
		gtk_tree_model_get( GTK_TREE_MODEL( self ), parent_iter, RECONCIL_COL_OBJECT, &row_object, -1 );
		g_return_if_fail( row_object && ( OFO_IS_ENTRY( row_object ) || OFO_IS_BAT_LINE( row_object )));
		g_object_unref( row_object );

		if( OFO_IS_BAT_LINE( row_object )){
			/* get a reference on old parent */
			path = gtk_tree_model_get_path( GTK_TREE_MODEL( self ), parent_iter );
			parent_ref = gtk_tree_row_reference_new( GTK_TREE_MODEL( self ), path );
			gtk_tree_path_free( path );

			/* insert a new parent row */
			gtk_tree_store_insert( GTK_TREE_STORE( self ), inserted_iter, NULL, -1 );
			path = gtk_tree_model_get_path( GTK_TREE_MODEL( self ), inserted_iter );
			inserted_ref = gtk_tree_row_reference_new( GTK_TREE_MODEL( self ), path );
			gtk_tree_path_free( path );

			/* reattach old parent and its children to the new parent */
			move_children_rec( self, inserted_ref, parent_ref );
			gtk_tree_row_reference_free( inserted_ref );
			gtk_tree_row_reference_free( parent_ref );

			/* done */
			return;
		}
	}

	if( 0 ){
		gtk_tree_model_get( GTK_TREE_MODEL( self ), parent_iter, RECONCIL_COL_OBJECT, &parent_object, -1 );
		g_debug( "insert_with_remediation: parent_label=%s", ofo_entry_get_label( OFO_ENTRY( parent_object )));
		g_object_unref( parent_object );
	}

	/* insert the new row as a child of specified parent */
	gtk_tree_store_insert( GTK_TREE_STORE( self ), inserted_iter, parent_iter, -1 );
}

static void
move_children_rec( ofaReconcilStore *self, GtkTreeRowReference *target_ref, GtkTreeRowReference *source_ref )
{
	GtkTreePath *path;
	GtkTreeIter iter, child_iter, target_iter;
	GtkTreeRowReference *child_ref;
	gboolean path_is_ok;
	gint count;
	ofoBase *object;

	path = gtk_tree_row_reference_get_path( source_ref );
	path_is_ok = gtk_tree_model_get_iter( GTK_TREE_MODEL( self ), &iter, path );
	gtk_tree_path_free( path );

	if( path_is_ok ){
		count = gtk_tree_model_iter_n_children( GTK_TREE_MODEL( self ), &iter );
		if( count ){
			while( gtk_tree_model_iter_children( GTK_TREE_MODEL( self ), &child_iter, &iter )){
				path = gtk_tree_model_get_path( GTK_TREE_MODEL( self ), &child_iter );
				child_ref = gtk_tree_row_reference_new( GTK_TREE_MODEL( self ), path );
				gtk_tree_path_free( path );
				move_children_rec( self, target_ref, child_ref );
				gtk_tree_row_reference_free( child_ref );
			}
			path = gtk_tree_row_reference_get_path( source_ref );
			gtk_tree_model_get_iter( GTK_TREE_MODEL( self ), &iter, path );
			gtk_tree_path_free( path );
		}
		/* move source_ref (path, iter) as a child of target_ref */
		gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter, RECONCIL_COL_OBJECT, &object, -1 );
		g_return_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )));
		gtk_tree_store_remove( GTK_TREE_STORE( self ), &iter );

		path = gtk_tree_row_reference_get_path( target_ref );
		gtk_tree_model_get_iter( GTK_TREE_MODEL( self ), &target_iter, path );
		gtk_tree_path_free( path );

		gtk_tree_store_insert( GTK_TREE_STORE( self ), &child_iter, &target_iter, -1 );
	}
}

/*
 * when inserting a not yet conciliated entry or batline,
 *  search for another unconciliated row at level 0 with compatible
 *  amount and date: it will be used as a parent of the being-inserted
 *  row
 *
 * Search for the closest date so that entry.dope <= batline.deffect.
 */
static gboolean
search_for_parent_by_amount( ofaReconcilStore *self, ofoBase *object, GtkTreeIter *parent_iter, ofxCounter exclude )
{
	static const gchar *thisfn = "ofa_reconcil_store_search_for_parent_by_amount";
	gboolean is_debug = FALSE;
	gboolean spread_set;
	gchar *obj_debit, *obj_credit, *row_debit, *row_credit;
	ofoBase *row_object;
	const GDate *obj_date;
	GtkTreeIter iter, closest_iter;
	gint spread;
	gboolean found;
	ofxCounter row_entnum;
	ofeEntryStatus row_status;

	g_return_val_if_fail( object && ( OFO_IS_ENTRY( object ) || OFO_IS_BAT_LINE( object )), FALSE );

	found = FALSE;
	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		/* get amounts of the being-inserted object
		 * entry: debit is positive, credit is negative
		 * batline: debit is negative, credit is positive */
		if( OFO_IS_ENTRY( object )){
			if( ofo_entry_get_status( OFO_ENTRY( object )) == ENT_STATUS_DELETED ){
				return( FALSE );
			}
			entry_get_amount_strs( self, OFO_ENTRY( object ), &obj_debit, &obj_credit );
			obj_date = ofo_entry_get_dope( OFO_ENTRY( object ));
		} else {
			bat_get_amount_strs( self, OFO_BAT_LINE( object ), &obj_debit, &obj_credit );
			obj_date = ofo_bat_line_get_deffect( OFO_BAT_LINE( object ));
		}
		spread = G_MAXINT;
		spread_set = FALSE;
		if( is_debug ){
			g_debug( "%s: object debit=%s credit=%s", thisfn, obj_debit, obj_credit );
		}
		/* we are searching for an amount opposite to those of the object
		 * considering the first row which does not have yet a child */
		while( TRUE ){
			if( !gtk_tree_model_iter_has_child( GTK_TREE_MODEL( self ), &iter )){
				gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter,
						RECONCIL_COL_STATUS_I, &row_status,
						RECONCIL_COL_ENT_NUMBER, &row_entnum,
						RECONCIL_COL_OBJECT, &row_object, -1 );
				g_return_val_if_fail( row_object && ( OFO_IS_ENTRY( row_object ) || OFO_IS_BAT_LINE( row_object )), FALSE );
				g_object_unref( row_object );
				row_debit = NULL;
				row_credit = NULL;

				if( !ofa_iconcil_get_concil( OFA_ICONCIL( row_object ))){
					if( OFO_IS_ENTRY( row_object )){
						if( row_status != ENT_STATUS_DELETED && ( exclude == 0 || exclude != row_entnum )){
							entry_get_amount_strs( self, OFO_ENTRY( row_object ), &row_debit, &row_credit );
						}
					} else {
						bat_get_amount_strs( self, OFO_BAT_LINE( row_object ), &row_debit, &row_credit );
					}
					if( is_debug ){
						g_debug( "%s: row debit=%s credit=%s", thisfn, row_debit, row_credit );
					}
					if( !my_collate( obj_debit, row_credit ) && !my_collate( obj_credit, row_debit )){
						if( find_closest_date( self, object, obj_date, row_object, &spread, &spread_set )){
							if( is_debug ){
								g_debug( "%s: setting closest_iter", thisfn );
							}
							closest_iter = iter;
						}
						if( is_debug ){
							g_debug( "%s: returning TRUE", thisfn );
						}
						found = TRUE;
					}
				}
				g_free( row_debit );
				g_free( row_credit );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
		g_free( obj_debit );
		g_free( obj_credit );
	}

	if( found ){
		*parent_iter = closest_iter;
	}

	return( found );
}

/*
 * (object, row_object) = (entry, entry) or (batline, batline) => dates must be the same
 * (object, row_object) = (entry, batline) => obj_date <= row_date
 * (object, row_object) = (batline, entry) => obj_date >= row_date
 */
static gboolean
find_closest_date( ofaReconcilStore *self, ofoBase *object, const GDate *obj_date, ofoBase *row_object, gint *spread, gboolean *spread_set )
{
	const GDate *row_date;
	gint this_spread;

	if( OFO_IS_ENTRY( object )){
		if( OFO_IS_ENTRY( row_object )){
			row_date = ofo_entry_get_dope( OFO_ENTRY( row_object ));
			this_spread = g_date_days_between( obj_date, row_date );
			if( !*spread_set || this_spread == 0 ){
				*spread = this_spread;
				*spread_set = TRUE;
				return( TRUE );
			}
		} else if( OFO_IS_BAT_LINE( row_object )){
			row_date = ofo_bat_line_get_deffect( OFO_BAT_LINE( row_object ));
			this_spread = g_date_days_between( obj_date, row_date );
			if( !*spread_set || ( this_spread >= 0 && this_spread < *spread )){
				*spread = this_spread;
				*spread_set = TRUE;
				return( TRUE );
			}
		}
	} else {
		if( OFO_IS_ENTRY( row_object )){
			row_date = ofo_entry_get_dope( OFO_ENTRY( row_object ));
			this_spread = g_date_days_between( row_date, obj_date );
			if( !*spread_set || ( this_spread >= 0 && this_spread < *spread )){
				*spread = this_spread;
				*spread_set = TRUE;
				return( TRUE );
			}
		} else {
			row_date = ofo_bat_line_get_deffect( OFO_BAT_LINE( row_object ));
			this_spread = g_date_days_between( obj_date, row_date );
			if( !*spread_set || this_spread == 0 ){
				*spread = this_spread;
				*spread_set = TRUE;
				return( TRUE );
			}
		}
	}

	return( FALSE );
}

/*
 * when inserting an already conciliated entry or bat line in the store,
 * search for the parent of the same conciliation group (if any)
 * if found, the row will be inserted as a child of the found parent
 * set the iter on the tree store if found
 * return TRUE if found
 */
static gboolean
search_for_parent_by_concil( ofaReconcilStore *self, ofoConcil *concil, GtkTreeIter *parent_iter, ofxCounter exclude )
{
	ofxCounter concil_id, row_id, row_entnum;
	GtkTreeIter iter;

	g_return_val_if_fail( concil && OFO_IS_CONCIL( concil ), FALSE );

	concil_id = ofo_concil_get_id( concil );

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ),
					&iter, RECONCIL_COL_CONCIL_NUMBER_I, &row_id, RECONCIL_COL_ENT_NUMBER_I, &row_entnum, -1 );
			if( row_id == concil_id && ( exclude == 0 || exclude != row_entnum )){
				*parent_iter = iter;
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
	}

	return( FALSE );
}

/*
 * @iter: [out]: the found iter (if any)
 *
 * returns %TRUE if found
 */
static gboolean
search_for_entry_by_number( ofaReconcilStore *self, ofxCounter number, GtkTreeIter *iter )
{
	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		if( search_for_entry_by_number_rec( self, number, iter )){
			return( TRUE );
		}
	}
	return( FALSE );
}

static gboolean
search_for_entry_by_number_rec( ofaReconcilStore *self, ofxCounter number, GtkTreeIter *iter )
{
	ofxCounter row_id;
	GtkTreeIter child_iter;

	while( TRUE ){
		if( gtk_tree_model_iter_children( GTK_TREE_MODEL( self ), &child_iter, iter )){
			if( search_for_entry_by_number_rec( self, number, &child_iter )){
				*iter = child_iter;
				return( TRUE );
			}
		}
		gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, RECONCIL_COL_ENT_NUMBER_I, &row_id, -1 );
		if( row_id == number ){
			return( TRUE );
		}
		if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
			break;
		}
	}

	return( FALSE );
}

/*
 * search the row for this member of the conciliation group
 */
static gboolean
find_row_by_concil_member( ofaReconcilStore *self, const gchar *type, ofxCounter id, GtkTreeIter *iter )
{
	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		if( find_row_by_concil_member_rec( self, type, id, iter )){
			return( TRUE );
		}
	}
	return( FALSE );
}

static gboolean
find_row_by_concil_member_rec( ofaReconcilStore *self, const gchar *type, ofxCounter id, GtkTreeIter *iter )
{
	ofxCounter row_id;
	GtkTreeIter child_iter;
	gchar *row_type;
	gint cmp;

	while( TRUE ){
		if( gtk_tree_model_iter_children( GTK_TREE_MODEL( self ), &child_iter, iter )){
			if( find_row_by_concil_member_rec( self, type, id, &child_iter )){
				*iter = child_iter;
				return( TRUE );
			}
		}
		gtk_tree_model_get( GTK_TREE_MODEL( self ), iter,
				RECONCIL_COL_CONCIL_TYPE, &row_type, RECONCIL_COL_ENT_NUMBER_I, &row_id, -1 );
		cmp = ( id < row_id ? -1 : ( id > row_id ? 1 : 0 ));
		if( cmp == 0 ){
			cmp = my_collate( type, row_type );
		}
		if( 0 ){
			g_debug( "type=%s, id=%lu, row_type=%s, row_number=%lu, cmp=%d", type, id, row_type, row_id, cmp );
		}
		g_free( row_type );
		if( cmp == 0 ){
			return( TRUE );
		}
		if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
			break;
		}
	}

	return( FALSE );
}

/**
 * ofa_reconcil_store_insert_row:
 * @store: this #ofaReconcilStore instance.
 * @iconcil: an object which implements the #ofaIConcil interface.
 * @parent_iter: [allow-none]: the store iter of the parent;
 *  if %NULL, the insert position will be automatically chosen by the
 *  @store.
 * @iter: [out][allow-none]: the newly inserted iter.
 *
 * Insert a new row at the requested position if @parent is not %NULL,
 * or at its (supposed) better position.
 *
 * This means that the @store itself will decide where to insert the
 * @iconcil row after having examined it. The position is chosen based
 * on the @iconcil type, its conciliation status and its amount.
 */
void
ofa_reconcil_store_insert_row( ofaReconcilStore *store, ofaIConcil *iconcil, GtkTreeIter *parent_iter, GtkTreeIter *iter )
{
	ofaReconcilStorePrivate *priv;

	g_return_if_fail( store && OFA_IS_RECONCIL_STORE( store ));
	g_return_if_fail( iconcil && OFA_IS_ICONCIL( iconcil ));

	priv = ofa_reconcil_store_get_instance_private( store );

	g_return_if_fail( !priv->dispose_has_run );

	if( OFO_IS_ENTRY( iconcil )){
		entry_insert_row( store, OFO_ENTRY( iconcil ), TRUE, parent_iter, iter, 0 );
	} else {
		bat_insert_row( store, OFO_BAT_LINE( iconcil ), TRUE, parent_iter, iter, 0 );
	}
}

/**
 * ofa_reconcil_store_insert_level_zero:
 * @store: this #ofaReconcilStore instance.
 * @iconcil: an object which implements the #ofaIConcil interface.
 * @iter: [out][allow-none]: the newly inserted iter.
 *
 * Insert a new row at level zero.
 */
void
ofa_reconcil_store_insert_level_zero( ofaReconcilStore *store, ofaIConcil *iconcil, GtkTreeIter *iter )
{
	ofaReconcilStorePrivate *priv;

	g_return_if_fail( store && OFA_IS_RECONCIL_STORE( store ));
	g_return_if_fail( iconcil && OFA_IS_ICONCIL( iconcil ));
	g_return_if_fail( iter );

	priv = ofa_reconcil_store_get_instance_private( store );

	g_return_if_fail( !priv->dispose_has_run );

	if( OFO_IS_ENTRY( iconcil )){
		entry_insert_row( store, OFO_ENTRY( iconcil ), FALSE, NULL, iter, 0 );
	} else {
		bat_insert_row( store, OFO_BAT_LINE( iconcil ), FALSE, NULL, iter, 0 );
	}
}

static void
insert_new_entry( ofaReconcilStore *self, ofoEntry *entry )
{
	ofaReconcilStorePrivate *priv;
	const gchar *ent_account, *acc_number;

	priv = ofa_reconcil_store_get_instance_private( self );

	if( ofo_entry_get_status( entry ) != ENT_STATUS_DELETED ){
		ent_account = ofo_entry_get_account( entry );
		acc_number = ofo_account_get_number( priv->account );
		if( !my_collate( ent_account, acc_number )){
			entry_insert_row( self, entry, TRUE, NULL, NULL, 0 );
		}
	}
}

static void
set_account_new_id( ofaReconcilStore *self, const gchar *prev_id, const gchar *new_id )
{
	ofaReconcilStorePrivate *priv;

	priv = ofa_reconcil_store_get_instance_private( self );

	/* update in-memory private data */
	if( !my_collate( priv->acc_number, prev_id )){
		g_free( priv->acc_number );
		priv->acc_number = g_strdup( new_id );
	}

	update_column( self, prev_id, new_id, RECONCIL_COL_ACCOUNT );
}

static void
set_currency_new_id( ofaReconcilStore *self, const gchar *prev_id, const gchar *new_id )
{
	update_column( self, prev_id, new_id, RECONCIL_COL_CURRENCY );
}

static void
set_ledger_new_id( ofaReconcilStore *self, const gchar *prev_id, const gchar *new_id )
{
	update_column( self, prev_id, new_id, RECONCIL_COL_LEDGER );
}

static void
set_ope_template_new_id( ofaReconcilStore *self, const gchar *prev_id, const gchar *new_id )
{
	update_column( self, prev_id, new_id, RECONCIL_COL_OPE_TEMPLATE );
}

static void
update_column( ofaReconcilStore *self, const gchar *prev_id, const gchar *new_id, guint column )
{
	GtkTreeIter iter;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		update_column_rec( self, prev_id, new_id, column, &iter );
	}
}

static void
update_column_rec( ofaReconcilStore *self, const gchar *prev_id, const gchar *new_id, guint column, GtkTreeIter *iter )
{
	GtkTreeIter child_iter;
	gchar *row_id;

	while( TRUE ){
		if( gtk_tree_model_iter_children( GTK_TREE_MODEL( self ), &child_iter, iter )){
			update_column_rec( self, prev_id, new_id, column, &child_iter );
		}
		gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, column, &row_id, -1 );
		if( !my_collate( row_id, prev_id )){
			gtk_tree_store_set( GTK_TREE_STORE( self ), iter, column, new_id, -1 );
		}
		g_free( row_id );
		if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
			break;
		}
	}
}

/*
 * Connect to ofaISignaler signaling system
 */
static void
signaler_connect_to_signaling_system( ofaReconcilStore *self )
{
	ofaReconcilStorePrivate *priv;
	ofaISignaler *signaler;
	gulong handler;

	priv = ofa_reconcil_store_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );

	handler = g_signal_connect( signaler, SIGNALER_BASE_NEW, G_CALLBACK( signaler_on_new_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_BASE_UPDATED, G_CALLBACK( signaler_on_updated_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_BASE_DELETED, G_CALLBACK( signaler_on_deleted_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );
}

/*
 * SIGNALER_BASE_NEW signal handler
 */
static void
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaReconcilStore *self )
{
	static const gchar *thisfn = "ofa_reconcil_store_signaler_on_new_base";

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_ENTRY( object )){
		insert_new_entry( self, OFO_ENTRY( object ));
	}
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaReconcilStore *self )
{
	static const gchar *thisfn = "ofa_reconcil_store_signaler_on_updated_base";

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( prev_id ){
		if( OFO_IS_ACCOUNT( object )){
			set_account_new_id( self, prev_id, ofo_account_get_number( OFO_ACCOUNT( object )));

		} else if( OFO_IS_CURRENCY( object )){
			set_currency_new_id( self, prev_id, ofo_currency_get_code( OFO_CURRENCY( object )));

		} else if( OFO_IS_LEDGER( object )){
			set_ledger_new_id( self, prev_id, ofo_ledger_get_mnemo( OFO_LEDGER( object )));

		} else if( OFO_IS_OPE_TEMPLATE( object )){
			set_ope_template_new_id( self, prev_id, ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object )));
		}

	} else if( OFO_IS_ENTRY( object )){
		signaler_on_updated_entry( self, OFO_ENTRY( object ));
	}
}

static void
signaler_on_updated_entry( ofaReconcilStore *self, ofoEntry *entry )
{
	ofaReconcilStorePrivate *priv;
	const gchar *entry_account;
	GtkTreeIter iter;

	priv = ofa_reconcil_store_get_instance_private( self );

	if( search_for_entry_by_number( self, ofo_entry_get_number( entry ), &iter )){
		/* if the entry was present in the store, it is easy to remediate it */
		entry_set_row_by_iter( self, entry, &iter );

	} else {
		/* else, should it be present now ? */
		entry_account = ofo_entry_get_account( entry );
		if( !my_collate( priv->acc_number, entry_account )){
			entry_insert_row( self, entry, TRUE, NULL, NULL, 0 );
		}
	}
}

/*
 * SIGNALER_BASE_DELETED signal handler
 */
static void
signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaReconcilStore *self )
{
	static const gchar *thisfn = "ofa_reconcil_store_signaler_on_deleted_base";

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CONCIL( object )){
		signaler_on_deleted_concil( self, OFO_CONCIL( object ));

	} else if( OFO_IS_ENTRY( object )){
		signaler_on_deleted_entry( self, OFO_ENTRY( object ));
	}
}

static void
signaler_on_deleted_concil( ofaReconcilStore *self, ofoConcil *concil )
{
	ofo_concil_for_each_member( concil, ( ofoConcilEnumerate ) signaler_on_deleted_concil_cb, self );
}

static void
signaler_on_deleted_concil_cb( ofoConcil *concil, const gchar *type, ofxCounter id, ofaReconcilStore *self )
{
	static const gchar *thisfn = "ofa_reconcil_store_signaler_on_deleted_concil_cb";
	GtkTreeIter iter;
	ofoBase *row_object;

	if( find_row_by_concil_member( self, type, id, &iter )){
		gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter, RECONCIL_COL_OBJECT, &row_object, -1 );
		g_return_if_fail( row_object && OFA_IS_ICONCIL( row_object ));
		g_object_unref( row_object );
		ofa_iconcil_clear_data( OFA_ICONCIL( row_object ));
		concil_set_row_with_data( self, 0, NULL, &iter );

	} else {
		g_debug( "%s: type=%s, id=%lu not found", thisfn, type, id );
	}
}

static void
signaler_on_deleted_entry( ofaReconcilStore *self, ofoEntry *entry )
{
	static const gchar *thisfn = "ofa_reconcil_store_signaler_on_deleted_entry";
	GtkTreeIter iter, child_iter;
	ofoBase *row_object;
	ofxCounter entnum;

	entnum = ofo_entry_get_number( entry );

	if( search_for_entry_by_number( self, entnum, &iter )){
		while( gtk_tree_model_iter_children( GTK_TREE_MODEL( self ), &child_iter, &iter )){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &child_iter, RECONCIL_COL_OBJECT, &row_object, -1 );
			gtk_tree_store_remove( GTK_TREE_STORE( self ), &child_iter );
			if( OFO_IS_ENTRY( row_object )){
				entry_insert_row( self, OFO_ENTRY( row_object ), TRUE, NULL, NULL, entnum );
			} else {
				bat_insert_row( self, OFO_BAT_LINE( row_object ), TRUE, NULL, NULL, entnum );
			}
			g_object_unref( row_object );
			if( !search_for_entry_by_number( self, entnum, &iter )){
				g_return_if_reached();
			}
		}
		gtk_tree_store_remove( GTK_TREE_STORE( self ), &iter );

	} else {
		g_debug( "%s: entry_number=%lu not found", thisfn, entnum );
	}
}
