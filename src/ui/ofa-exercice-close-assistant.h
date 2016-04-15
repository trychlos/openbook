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

#ifndef __OFA_EXERCICE_CLOSE_ASSISTANT_H__
#define __OFA_EXERCICE_CLOSE_ASSISTANT_H__

/**
 * SECTION: ofa_exercice_close_assistant
 * @short_description: #ofaExerciceCloseAssistant class definition.
 * @include: ui/ofa-exercice-close-assistant.h
 *
 * Close the current exercice.
 *
 * Some of the accounts may benefit of the reconciliation process:
 * -> if yes, then the "keep unreconciliated entries" option applies.
 *
 * Some of the accounts may benefit of the settling (settlement,
 * settled) process:
 * -> if yes, then the "keep unsettled entries" option applies.
 *
 * Some of the accounts start the exercice with a solde equal to those
 * at the end of the previous exercice (e.g. bank accounts):
 * -> if yes, then the "create carried forward entries" option applies.
 *
 * From "EBP Compta Fr":
 *   La clôture de l'exercice a pour but de calculer le résultat de
 *   l'exercice en soldant les comptes de charge et de produit, et de
 *   générer les écritures de report à nouveau pour tous les comptes
 *   d'actif et de passif (classes 1 à 5).
 *   Les opérations suivantes doivent avoir été effectuées au préalable:
 *   - lettrer tous les comptes de tiers
 *   - valider les écritures de simulation que vous souhaitez conserver
 *   - éditer tous les états comptables de fin d'exercice
 *   - procéder à une sauvegarde des données.
 *
 * From Openbook point of view:
 *
 * 1 - recall the prerequired operation as above
 *
 * 2 - enter required parameters (which may have been configured):
 *     date of the end of current exercice
 *     dates of the beginning and the end of the next exercice
 *     the operation template for balancing entries, or
 *      the account and the ledger
 *
 * 3 - after confirmation, run the security and integrity checks:
 *     a) accounts, ledgers and entries are balanced
 *     b) dbms integrity os ok
 *
 * 4 - after a new user confirmation:
 *     let the user backup the dossier on option
 *
 * 5 - close the exercice:
 *     a) as soon as the ending date of the exercice is set, some
 *        entries may have to be remediated (from future to rough or
 *        from rough to future)
 *     b) validate remaining rough entries on the exercice
 *     c) balance all detail accounts
 *        this means writing a solde entry on the account, balancing
 *        it to a general balance account;
 *        -> on settleable accounts, these solde entries are marked
 *           settled;
 *        -> on reconciliable accounts, these solde entries are marked
 *           reconciliated;
 *        -> on forwardable accounts, a new couple of entries is prepared
 *           for later insertion in the new exercice, balanced with the
 *           same balance account.
 *     d) close all ledgers
 *        this validates the new solde entries, and update ledgers balances
 *     e) archive the being-closed dossier
 *        - update the settings
 *        - update the 'current' flag in the OFA_T_DOSSIER table
 *     f) open the new dossier
 *        - is a raw copy of the closed dossier
 *        - update the settings
 *        - update dossier properties with this new exercice
 *     g) cleanup this new dossier of now-obsolete datas
 *        - empty audit table (no archive)
 *        - archive and empty accounts archived balances
 *        - archive entries, only keeping unsettled and unreconciliable
 *          ones + set them as 'past'
 *        - archive bat, only keeping those which are not fully
 *          reconciliated
 *        - reset accounts and ledgers rough and validated balances to zero
 *        - insert prepared forward entries and validates them
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXERCICE_CLOSE_ASSISTANT                ( ofa_exercice_close_assistant_get_type())
#define OFA_EXERCICE_CLOSE_ASSISTANT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXERCICE_CLOSE_ASSISTANT, ofaExerciceCloseAssistant ))
#define OFA_EXERCICE_CLOSE_ASSISTANT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXERCICE_CLOSE_ASSISTANT, ofaExerciceCloseAssistantClass ))
#define OFA_IS_EXERCICE_CLOSE_ASSISTANT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXERCICE_CLOSE_ASSISTANT ))
#define OFA_IS_EXERCICE_CLOSE_ASSISTANT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXERCICE_CLOSE_ASSISTANT ))
#define OFA_EXERCICE_CLOSE_ASSISTANT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXERCICE_CLOSE_ASSISTANT, ofaExerciceCloseAssistantClass ))

typedef struct {
	/*< public members >*/
	GtkAssistant      parent;
}
	ofaExerciceCloseAssistant;

typedef struct {
	/*< public members >*/
	GtkAssistantClass parent;
}
	ofaExerciceCloseAssistantClass;

GType  ofa_exercice_close_assistant_get_type( void ) G_GNUC_CONST;

void   ofa_exercice_close_assistant_run     ( ofaIGetter *getter,
													GtkWindow *parent);

G_END_DECLS

#endif /* __OFA_EXERCICE_CLOSE_ASSISTANT_H__ */
