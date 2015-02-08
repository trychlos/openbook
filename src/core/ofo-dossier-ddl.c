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
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-date.h"
#include "api/ofa-dbms.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-settings.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#include "core/ofo-dossier-ddl.h"

typedef GType ( *fnType )( void );

static const gchar *st_classes          = INIT1DIR "/classes-h1.csv";
static const gchar *st_currencies       = INIT1DIR "/currencies-h1.csv";
static const gchar *st_ledgers          = INIT1DIR "/ledgers-h1.csv";
static const gchar *st_ope_templates    = INIT1DIR "/ope-templates-h2.csv";
static const gchar *st_rates            = INIT1DIR "/rates-h2.csv";

static gint        dbmodel_get_version( const ofoDossier *dossier );
static gboolean    dbmodel_to_v20( const ofoDossier *dossier );
static gboolean    insert_classes( ofoDossier *dossier );
static gboolean    insert_currencies( ofoDossier *dossier );
static gboolean    insert_ledgers( ofoDossier *dossier );
static gboolean    insert_ope_templates( ofoDossier *dossier );
static gboolean    insert_rates( ofoDossier *dossier );
static gboolean    import_utf8_comma_pipe_file( ofoDossier *dossier, const gchar *table, const gchar *fname, gint headers, fnType fn );
static gint        count_rows( const ofoDossier *dossier, const gchar *table );

/**
 * ofo_dossier_ddl_update:
 * @dossier: this #ofoDossier instance, with an already opened
 *  connection
 *
 * Update the DB model in the DBMS
 */
gboolean
ofo_dossier_ddl_update( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_ddl_update";
	gint cur_version;

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	cur_version = dbmodel_get_version( dossier );
	g_debug( "%s: cur_version=%d, THIS_DBMODEL_VERSION=%d", thisfn, cur_version, THIS_DBMODEL_VERSION );

	if( cur_version < THIS_DBMODEL_VERSION ){
		if( cur_version < 20 ){
			dbmodel_to_v20( dossier );
		}
		insert_classes( dossier );
		insert_currencies( dossier );
		insert_ledgers( dossier );
		insert_ope_templates( dossier );
		insert_rates( dossier );
	}

	return( TRUE );
}

/*
 * returns the last complete version
 * i.e. une version where the version date is set
 */
static gint
dbmodel_get_version( const ofoDossier *dossier )
{
	gint vmax;

	ofa_dbms_query_int(
			ofo_dossier_get_dbms( dossier ),
			"SELECT MAX(VER_NUMBER) FROM OFA_T_VERSION WHERE VER_DATE > 0", &vmax, FALSE );

	return( vmax );
}

/**
 * ofo_dossier_dbmodel_to_v20:
 * @dbms: an already opened #ofaDbms connection
 * @dname: the name of the dossier from settings, will be used as
 *  default label
 * @account: the current connected account.
 */
static gboolean
dbmodel_to_v20( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_dbmodel_to_v20";
	static guint this_version = 20;
	const ofaDbms *dbms;
	gchar *query;

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	dbms = ofo_dossier_get_dbms( dossier );

	/* default value for timestamp cannot be null */
	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_VERSION ("
			"	VER_NUMBER INTEGER NOT NULL UNIQUE DEFAULT 0     COMMENT 'DB model version number',"
			"	VER_DATE   TIMESTAMP DEFAULT 0                   COMMENT 'Version application timestamp') "
			"CHARACTER SET utf8",
			TRUE)){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO OFA_T_VERSION "
			"	(VER_NUMBER, VER_DATE) VALUES (%u, 0)", this_version );
	if( !ofa_dbms_query( dbms, query, TRUE )){
		return( FALSE );
	}
	g_free( query );

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_ACCOUNTS ("
			"	ACC_NUMBER          VARCHAR(20) BINARY NOT NULL UNIQUE COMMENT 'Account number',"
			"	ACC_LABEL           VARCHAR(80)   NOT NULL       COMMENT 'Account label',"
			"	ACC_CURRENCY        VARCHAR(3)                   COMMENT 'ISO 3A identifier of the currency of the account',"
			"	ACC_NOTES           VARCHAR(4096)                COMMENT 'Account notes',"
			"	ACC_TYPE            CHAR(1)                      COMMENT 'Account type, values R/D',"
			"	ACC_SETTLEABLE      CHAR(1)                      COMMENT 'Whether the account is settleable',"
			"	ACC_RECONCILIABLE   CHAR(1)                      COMMENT 'Whether the account is reconciliable',"
			"	ACC_FORWARD         CHAR(1)                      COMMENT 'Whether the account supports carried forwards',"
			"	ACC_UPD_USER        VARCHAR(20)                  COMMENT 'User responsible of properties last update',"
			"	ACC_UPD_STAMP       TIMESTAMP                    COMMENT 'Properties last update timestamp',"
			"	ACC_VAL_DEBIT       DECIMAL(20,5)                COMMENT 'Debit balance of validated entries',"
			"	ACC_VAL_CREDIT      DECIMAL(20,5)                COMMENT 'Credit balance of validated entries',"
			"	ACC_ROUGH_DEBIT     DECIMAL(20,5)                COMMENT 'Debit balance of rough entries',"
			"	ACC_ROUGH_CREDIT    DECIMAL(20,5)                COMMENT 'Credit balance of rough entries',"
			"	ACC_OPEN_DEBIT      DECIMAL(20,5)                COMMENT 'Debit balance at the exercice opening',"
			"	ACC_OPEN_CREDIT     DECIMAL(20,5)                COMMENT 'Credit balance at the exercice opening',"
			"	ACC_FUT_DEBIT       DECIMAL(20,5)                COMMENT 'Debit balance of future entries',"
			"	ACC_FUT_CREDIT      DECIMAL(20,5)                COMMENT 'Credit balance of future entries'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

#if 0
	/* defined post v1 */
	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_ASSETS ("
			"	ASS_ID        INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Intern asset identifier',"
			"	ASS_LABEL     VARCHAR(80)                 COMMENT 'Asset label',"
			"	ASS_DATE_IN   DATE                        COMMENT 'Entry date',"
			"	ASS_TOTAL     DECIMAL(20,5)               COMMENT 'Total payed (TVA inc.)',"
			"	ASS_IMMO      DECIMAL(20,5)               COMMENT 'Montant immobilisé',"
			"	ASS_IMMO_FISC INTEGER                     COMMENT 'Montant fiscal immobilisé (à amortir)',"
			"	ASS_DUREE     INTEGER                     COMMENT 'Durée d\\'amortissement',"
			"	ASS_TYPE      VARCHAR(1)                  COMMENT 'Type d\\'amortissement',"
			"	ASS_COEF_DEG  DECIMAL(20,5)               COMMENT 'Coefficient degressif',"
			"	ASS_RATE      DECIMAL(20,5)               COMMENT 'Taux d\\'amortissement',"
			"	ASS_DATE_OUT  DATE                        COMMENT 'Outgoing date',"
			"	ASS_NOTES     VARCHAR(4096)               COMMENT 'Notes',"
			"	ASS_UPD_USER  VARCHAR(20)                 COMMENT 'User responsible of last update',"
			"	ASS_UPD_STAMP TIMESTAMP                   COMMENT 'Last update timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	/* defined post v1 */
	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_ASSETS_EXE ("
			"	ASS_ID           INTEGER                  COMMENT 'Intern asset identifier',"
			"	ASS_EXE_NUM      INTEGER                  COMMENT 'Numéro d\\'annuité',"
			"	ASS_EXE_DUREE    INTEGER                  COMMENT 'Duree (en mois)',"
			"	ASS_EXE_PREV     INTEGER                  COMMENT 'Total des amortissements antérieurs',"
			"	ASS_EXE_TAUX_LIN DECIMAL(20,5)            COMMENT 'Taux lineaire',"
			"	ASS_EXE_TAUX_DEG DECIMAL(20,5)            COMMENT 'Taux degressif',"
			"	ASS_EXE_AMORT    INTEGER                  COMMENT 'Montant de l\\'annuite',"
			"	ASS_EXE_REST     INTEGER                  COMMENT 'Valeur residuelle',"
			"	CONSTRAINT PRIMARY KEY (ASS_ID,ASS_EXE_NUM)"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}
#endif

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_BAT ("
			"	BAT_ID        BIGINT      NOT NULL UNIQUE COMMENT 'Intern import identifier',"
			"	BAT_URI       VARCHAR(256)                COMMENT 'Imported URI',"
			"	BAT_FORMAT    VARCHAR(80)                 COMMENT 'Identified file format',"
			"	BAT_BEGIN     DATE                        COMMENT 'Begin date of the transaction list',"
			"	BAT_END       DATE                        COMMENT 'End date of the transaction list',"
			"	BAT_RIB       VARCHAR(80)                 COMMENT 'Bank provided RIB',"
			"	BAT_CURRENCY  VARCHAR(3)                  COMMENT 'Account currency',"
			"	BAT_SOLDE     DECIMAL(20,5)               COMMENT 'Signed balance of the account',"
			"	BAT_NOTES     VARCHAR(4096)               COMMENT 'Import notes',"
			"	BAT_UPD_USER  VARCHAR(20)                 COMMENT 'User responsible of import',"
			"	BAT_UPD_STAMP TIMESTAMP                   COMMENT 'Import timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_BAT_LINES ("
			"	BAT_ID             BIGINT   NOT NULL      COMMENT 'Intern import identifier',"
			"	BAT_LINE_ID        BIGINT   NOT NULL UNIQUE COMMENT 'Intern imported line identifier',"
			"	BAT_LINE_DEFFECT   DATE                   COMMENT 'Effect date',"
			"	BAT_LINE_DOPE      DATE                   COMMENT 'Operation date',"
			"	BAT_LINE_REF       VARCHAR(80)            COMMENT 'Bank reference',"
			"	BAT_LINE_LABEL     VARCHAR(80)            COMMENT 'Line label',"
			"	BAT_LINE_CURRENCY  VARCHAR(3)             COMMENT 'Line currency',"
			"	BAT_LINE_AMOUNT    DECIMAL(20,5)          COMMENT 'Signed amount of the line',"
			"	BAT_LINE_ENTRY     BIGINT                 COMMENT 'Reciliated entry',"
			"	BAT_LINE_UPD_USER  VARCHAR(20)            COMMENT 'User responsible of the reconciliation',"
			"	BAT_LINE_UPD_STAMP TIMESTAMP              COMMENT 'Reconciliation timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_CLASSES ("
			"	CLA_NUMBER       INTEGER     NOT NULL UNIQUE   COMMENT 'Class number',"
			"	CLA_LABEL        VARCHAR(80) NOT NULL          COMMENT 'Class label',"
			"	CLA_NOTES        VARCHAR(4096)                 COMMENT 'Class notes',"
			"	CLA_UPD_USER     VARCHAR(20)                   COMMENT 'User responsible of properties last update',"
			"	CLA_UPD_STAMP    TIMESTAMP                     COMMENT 'Properties last update timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

#if 0
	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (1,'Comptes de capitaux')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (2,'Comptes d\\'immobilisations')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (3,'Comptes de stocks et en-cours')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (4,'Comptes de tiers')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (5,'Comptes financiers')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (6,'Comptes de charges')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (7,'Comptes de produits')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (8,'Comptes spéciaux')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_CLASSES "
			"	(CLA_NUMBER,CLA_LABEL) VALUES (9,'Comptes analytiques')", TRUE )){
		return( FALSE );
	}
#endif

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_CURRENCIES ("
			"	CUR_CODE      VARCHAR(3) BINARY NOT NULL      UNIQUE COMMENT 'ISO-3A identifier of the currency',"
			"	CUR_LABEL     VARCHAR(80) NOT NULL                   COMMENT 'Currency label',"
			"	CUR_SYMBOL    VARCHAR(3)  NOT NULL                   COMMENT 'Label of the currency',"
			"	CUR_DIGITS    INTEGER     DEFAULT 2                  COMMENT 'Decimal digits on display',"
			"	CUR_NOTES     VARCHAR(4096)                          COMMENT 'Currency notes',"
			"	CUR_UPD_USER  VARCHAR(20)                            COMMENT 'User responsible of properties last update',"
			"	CUR_UPD_STAMP TIMESTAMP                              COMMENT 'Properties last update timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

#if 0
	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_CURRENCIES "
			"	(CUR_CODE,CUR_LABEL,CUR_SYMBOL,CUR_DIGITS) VALUES ('EUR','Euro','€',2)", TRUE )){
		return( FALSE );
	}
#endif

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER ("
			"	DOS_ID               INTEGER   NOT NULL UNIQUE COMMENT 'Row identifier',"
			"	DOS_DEF_CURRENCY     VARCHAR(3)                COMMENT 'Default currency identifier',"
			"	DOS_EXE_BEGIN        DATE                      COMMENT 'Exercice beginning date',"
			"	DOS_EXE_END          DATE                      COMMENT 'Exercice ending date',"
			"	DOS_EXE_LENGTH       INTEGER                   COMMENT 'Exercice length in months',"
			"	DOS_EXE_NOTES        VARCHAR(4096)             COMMENT 'Exercice notes',"
			"	DOS_FORW_OPE         VARCHAR(6)                COMMENT 'Operation mnemo for carried forward entries',"
			"	DOS_IMPORT_LEDGER    VARCHAR(6)                COMMENT 'Default import ledger',"
			"	DOS_LABEL            VARCHAR(80)               COMMENT 'Raison sociale',"
			"	DOS_NOTES            VARCHAR(4096)             COMMENT 'Dossier notes',"
			"	DOS_SIREN            VARCHAR(9)                COMMENT 'Siren identifier',"
			"	DOS_SLD_OPE          VARCHAR(6)                COMMENT 'Operation mnemo for balancing entries',"
			"	DOS_UPD_USER         VARCHAR(20)               COMMENT 'User responsible of properties last update',"
			"	DOS_UPD_STAMP        TIMESTAMP                 COMMENT 'Properties last update timestamp',"
			"	DOS_LAST_BAT         BIGINT  DEFAULT 0         COMMENT 'Last BAT file number used',"
			"	DOS_LAST_BATLINE     BIGINT  DEFAULT 0         COMMENT 'Last BAT line number used',"
			"	DOS_LAST_ENTRY       BIGINT  DEFAULT 0         COMMENT 'Last entry number used',"
			"	DOS_LAST_SETTLEMENT  BIGINT  DEFAULT 0         COMMENT 'Last settlement number used',"
			"	DOS_STATUS           CHAR(1)                   COMMENT 'Status of this exercice'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	query = g_strdup_printf(
			"INSERT IGNORE INTO OFA_T_DOSSIER "
			"	(DOS_ID,DOS_LABEL,DOS_EXE_LENGTH,DOS_DEF_CURRENCY,"
			"	 DOS_STATUS,DOS_FORW_OPE,DOS_SLD_OPE) "
			"	VALUES (1,'%s',%u,'EUR','%s','%s','%s')",
			ofo_dossier_get_name( dossier ),
			DOS_DEFAULT_LENGTH, DOS_STATUS_OPENED, "CLORAN", "CLSLD" );
	if( !ofa_dbms_query( dbms, query, TRUE )){
		g_free( query );
		return( FALSE );
	}
	g_free( query );

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_DOSSIER_CUR ("
			"	DOS_ID               INTEGER   NOT NULL        COMMENT 'Row identifier',"
			"	DOS_CURRENCY         VARCHAR(3)                COMMENT 'Currency identifier',"
			"	DOS_SLD_ACCOUNT      VARCHAR(20)               COMMENT 'Balancing account when closing the exercice',"
			"	CONSTRAINT PRIMARY KEY (DOS_ID,DOS_CURRENCY)"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_ENTRIES ("
			"	ENT_DEFFECT      DATE NOT NULL            COMMENT 'Imputation effect date',"
			"	ENT_NUMBER       BIGINT  NOT NULL UNIQUE  COMMENT 'Entry number',"
			"	ENT_DOPE         DATE NOT NULL            COMMENT 'Operation date',"
			"	ENT_LABEL        VARCHAR(80)              COMMENT 'Entry label',"
			"	ENT_REF          VARCHAR(20)              COMMENT 'Piece reference',"
			"	ENT_ACCOUNT      VARCHAR(20)              COMMENT 'Account number',"
			"	ENT_CURRENCY     VARCHAR(3)               COMMENT 'ISO 3A identifier of the currency',"
			"	ENT_DEBIT        DECIMAL(20,5) DEFAULT 0  COMMENT 'Debiting amount',"
			"	ENT_CREDIT       DECIMAL(20,5) DEFAULT 0  COMMENT 'Crediting amount',"
			"	ENT_LEDGER       VARCHAR(6)               COMMENT 'Mnemonic identifier of the ledger',"
			"	ENT_OPE_TEMPLATE VARCHAR(6)               COMMENT 'Mnemonic identifier of the operation template',"
			"	ENT_STATUS       INTEGER       DEFAULT 1  COMMENT 'Is the entry validated or deleted ?',"
			"	ENT_UPD_USER     VARCHAR(20)              COMMENT 'User responsible of last update',"
			"	ENT_UPD_STAMP    TIMESTAMP                COMMENT 'Last update timestamp',"
			"	ENT_CONCIL_DVAL  DATE                     COMMENT 'Reconciliation value date',"
			"	ENT_CONCIL_USER  VARCHAR(20)              COMMENT 'User responsible of the reconciliation',"
			"	ENT_CONCIL_STAMP TIMESTAMP                COMMENT 'Reconciliation timestamp',"
			"	ENT_STLMT_NUMBER BIGINT                   COMMENT 'Settlement number',"
			"	ENT_STLMT_USER   VARCHAR(20)              COMMENT 'User responsible of the settlement',"
			"	ENT_STLMT_STAMP  TIMESTAMP                COMMENT 'Settlement timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_LEDGERS ("
			"	LED_MNEMO     VARCHAR(6) BINARY  NOT NULL UNIQUE COMMENT 'Mnemonic identifier of the ledger',"
			"	LED_LABEL     VARCHAR(80) NOT NULL        COMMENT 'Ledger label',"
			"	LED_NOTES     VARCHAR(4096)               COMMENT 'Ledger notes',"
			"	LED_UPD_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	LED_UPD_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp',"
			"	LED_LAST_CLO  DATE                        COMMENT 'Last closing date'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_LEDGERS_CUR ("
			"	LED_MNEMO            VARCHAR(6) NOT NULL  COMMENT 'Internal ledger identifier',"
			"	LED_CUR_CODE         VARCHAR(3) NOT NULL  COMMENT 'Internal currency identifier',"
			"	LED_CUR_VAL_DEBIT    DECIMAL(20,5)        COMMENT 'Validated debit total for this exercice on this journal',"
			"	LED_CUR_VAL_CREDIT   DECIMAL(20,5)        COMMENT 'Validated credit total for this exercice on this journal',"
			"	LED_CUR_ROUGH_DEBIT  DECIMAL(20,5)        COMMENT 'Rough debit total for this exercice on this journal',"
			"	LED_CUR_ROUGH_CREDIT DECIMAL(20,5)        COMMENT 'Rough credit total for this exercice on this journal',"
			"	LED_CUR_FUT_DEBIT    DECIMAL(20,5)        COMMENT 'Futur debit total on this journal',"
			"	LED_CUR_FUT_CREDIT   DECIMAL(20,5)        COMMENT 'Futur credit total on this journal',"
			"	CONSTRAINT PRIMARY KEY (LED_MNEMO,LED_CUR_CODE)"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

#if 0
	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('ACH','Journal des achats','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('VEN','Journal des ventes','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('EXP','Journal de l\\'exploitant','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('OD','Journal des opérations diverses','Default')", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"INSERT IGNORE INTO OFA_T_LEDGERS (LED_MNEMO, LED_LABEL, LED_UPD_USER) "
			"	VALUES ('BQ','Journal de banque','Default')", TRUE )){
		return( FALSE );
	}
#endif

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_OPE_TEMPLATES ("
			"	OTE_MNEMO      VARCHAR(6) BINARY NOT NULL UNIQUE COMMENT 'Operation template mnemonic',"
			"	OTE_LABEL      VARCHAR(80)       NOT NULL        COMMENT 'Template label',"
			"	OTE_LED_MNEMO  VARCHAR(6)                        COMMENT 'Generated entries imputation ledger',"
			"	OTE_LED_LOCKED INTEGER                           COMMENT 'Ledger is locked',"
			"	OTE_REF        VARCHAR(20)                       COMMENT 'Operation reference',"
			"	OTE_REF_LOCKED INTEGER                           COMMENT 'Operation reference is locked',"
			"	OTE_NOTES      VARCHAR(4096)                     COMMENT 'Template notes',"
			"	OTE_UPD_USER   VARCHAR(20)                       COMMENT 'User responsible of properties last update',"
			"	OTE_UPD_STAMP  TIMESTAMP                         COMMENT 'Properties last update timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_OPE_TEMPLATES_DET ("
			"	OTE_MNEMO              VARCHAR(6) NOT NULL     COMMENT 'Operation template menmonic',"
			"	OTE_DET_ROW            INTEGER    NOT NULL     COMMENT 'Detail line number',"
			"	OTE_DET_COMMENT        VARCHAR(80)             COMMENT 'Detail line comment',"
			"	OTE_DET_ACCOUNT        VARCHAR(20)             COMMENT 'Account number',"
			"	OTE_DET_ACCOUNT_LOCKED INTEGER                 COMMENT 'Account number is locked',"
			"	OTE_DET_LABEL          VARCHAR(80)             COMMENT 'Entry label',"
			"	OTE_DET_LABEL_LOCKED   INTEGER                 COMMENT 'Entry label is locked',"
			"	OTE_DET_DEBIT          VARCHAR(80)             COMMENT 'Debit amount',"
			"	OTE_DET_DEBIT_LOCKED   INTEGER                 COMMENT 'Debit amount is locked',"
			"	OTE_DET_CREDIT         VARCHAR(80)             COMMENT 'Credit amount',"
			"	OTE_DET_CREDIT_LOCKED  INTEGER                 COMMENT 'Credit amount is locked',"
			"	CONSTRAINT PRIMARY KEY (OTE_MNEMO, OTE_DET_ROW)"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

#if 0
	/* defined post v1 */
	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_RECURRENT ("
			"	REC_ID        INTEGER AUTO_INCREMENT NOT NULL UNIQUE COMMENT 'Internal identifier',"
			"	REC_MOD_MNEMO VARCHAR(6)                  COMMENT 'Entry model mnemmonic',"
			"	REC_PERIOD    VARCHAR(1)                  COMMENT 'Periodicity',"
			"	REC_DAY       INTEGER                     COMMENT 'Day of the period',"
			"	REC_NOTES     VARCHAR(4096)               COMMENT 'Notes',"
			"	REC_UPD_USER  VARCHAR(20)                 COMMENT 'User responsible of properties last update',"
			"	REC_UPD_STAMP TIMESTAMP                   COMMENT 'Properties last update timestamp',"
			"	REC_LAST      DATE                        COMMENT 'Effect date of the last generation'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}
#endif

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_RATES ("
			"	RAT_MNEMO         VARCHAR(6) BINARY NOT NULL UNIQUE COMMENT 'Mnemonic identifier of the rate',"
			"	RAT_LABEL         VARCHAR(80)       NOT NULL        COMMENT 'Rate label',"
			"	RAT_NOTES         VARCHAR(4096)                     COMMENT 'Rate notes',"
			"	RAT_UPD_USER      VARCHAR(20)                       COMMENT 'User responsible of properties last update',"
			"	RAT_UPD_STAMP     TIMESTAMP                         COMMENT 'Properties last update timestamp'"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	if( !ofa_dbms_query( dbms,
			"CREATE TABLE IF NOT EXISTS OFA_T_RATES_VAL ("
			"	RAT_UNUSED        INTEGER AUTO_INCREMENT PRIMARY KEY COMMENT 'An unused counter to have a unique key with NULL values',"
			"	RAT_MNEMO         VARCHAR(6) BINARY NOT NULL        COMMENT 'Mnemonic identifier of the rate',"
			"	RAT_VAL_BEG       DATE    DEFAULT NULL              COMMENT 'Validity begin date',"
			"	RAT_VAL_END       DATE    DEFAULT NULL              COMMENT 'Validity end date',"
			"	RAT_VAL_RATE      DECIMAL(20,5)                     COMMENT 'Rate value',"
			"	UNIQUE (RAT_MNEMO,RAT_VAL_BEG,RAT_VAL_END)"
			") CHARACTER SET utf8", TRUE )){
		return( FALSE );
	}

	/* we do this only at the end of the model creation
	 * as a mark that all has been successfully done
	 */
	query = g_strdup_printf(
			"UPDATE OFA_T_VERSION SET VER_DATE=NOW() WHERE VER_NUMBER=%u", this_version );
	if( !ofa_dbms_query( dbms, query, TRUE )){
		return( FALSE );
	}
	g_free( query );

	return( TRUE );
}

static gboolean
insert_classes( ofoDossier *dossier )
{
	return( import_utf8_comma_pipe_file(
			dossier, "OFA_T_CLASSES", st_classes, 1, ofo_class_get_type ));
}

static gboolean
insert_currencies( ofoDossier *dossier )
{
	return( import_utf8_comma_pipe_file(
			dossier, "OFA_T_CURRENCIES", st_currencies, 1, ofo_currency_get_type ));
}

static gboolean
insert_ledgers( ofoDossier *dossier )
{
	return( import_utf8_comma_pipe_file(
			dossier, "OFA_T_LEDGERS", st_ledgers, 1, ofo_ledger_get_type ));
}

static gboolean
insert_ope_templates( ofoDossier *dossier )
{
	return( import_utf8_comma_pipe_file(
			dossier, "OFA_T_OPE_TEMPLATES", st_ope_templates, 2, ofo_ope_template_get_type ));
}

static gboolean
insert_rates( ofoDossier *dossier )
{
	return( import_utf8_comma_pipe_file(
			dossier, "OFA_T_RATES", st_rates, 2, ofo_rate_get_type ));
}

static gboolean
import_utf8_comma_pipe_file( ofoDossier *dossier, const gchar *table, const gchar *fname, gint headers, fnType fn )
{
	gint count;
	gboolean ok;
	ofaFileFormat *settings;
	ofoBase *object;
	gchar *uri;

	ok = TRUE;
	count = count_rows( dossier, table );
	if( !count ){
		settings = ofa_file_format_new( SETTINGS_IMPORT_SETTINGS );
		ofa_file_format_set( settings,
				NULL, OFA_FFTYPE_CSV, OFA_FFMODE_IMPORT, "UTF-8", MY_DATE_SQL, ',', '|', headers );
		object = g_object_new( fn(), NULL );
		uri = g_filename_to_uri( fname, NULL, NULL );
		count = ofa_dossier_misc_import_csv(
				dossier, OFA_IIMPORTABLE( object ), uri, settings, NULL, NULL );
		ok = ( count > 0 );
		g_free( uri );
		g_object_unref( object );
		g_object_unref( settings );
	}

	return( ok );
}

static gint
count_rows( const ofoDossier *dossier, const gchar *table )
{
	gint count;
	gchar *query;

	query = g_strdup_printf( "SELECT COUNT(*) FROM %s", table );
	ofa_dbms_query_int( ofo_dossier_get_dbms( dossier ), query, &count, TRUE );
	g_free( query );

	return( count );
}
