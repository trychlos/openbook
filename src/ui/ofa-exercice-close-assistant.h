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

#ifndef __OFA_EXERCICE_CLOSE_ASSISTANT_H__
#define __OFA_EXERCICE_CLOSE_ASSISTANT_H__

/**
 * SECTION: ofa_exercice_close_assistant
 * @short_description: #ofaExerciceCloseAssistant class definition.
 * @include: ui/ofa-exercice-close-assistant.h
 *
 * Close the current exercice.
 *
 * Closing an exercice involves following steps:
 * - clear personal accounts: they must have a nul balance
 * - clear third party accounts, reconducting non pointed entries
 *   option: keep non settled third party entries
 *   option: keep non reconciliated entries
 *
 * Some of the accounts must be balanced for closing the exercice:
 * -> this is an account property
 *    if yes, then we must have a balance account for recording debit
 *    and credit entries
 *
 * Some of the accounts may benefit of the reconciliation process:
 * -> if yes, then the "keep non reconciliated entries" option applies
 *
 * Some of the accounts may benefit of the settling (settlement,
 * settled) process:
 * -> if yes, then the "keep non settled third party entries" option
 *    applies
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
 * 1 - recall the prerequired operation as above
 * 2 - after confirmation, and for all accounts:
 *     a) do the security and integrity checks
 *        entries and ledgers are sane (equilibrated)
 *        ledgers are closed ?
 *        all detail accounts may be set in a closing category
 * 3 - enter required parameters (which may have been configured):
 *     date of the end of current exercice
 *     dates of the beginning and the end of the next exercice
 *     the operation template for balancing entries, or
 *      the account and the ledger
 * 4 - after a new user confirmation:
 *     b1) balancing the account (so no balance carried forward), or
 *     b2) generate a balance entry for the new exercice
 *     c) archive all tables so that we may reaccess them later when
 *        consulting a closed exercice
 */

#include "ui/my-assistant.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXERCICE_CLOSE_ASSISTANT                ( ofa_exercice_close_assistant_get_type())
#define OFA_EXERCICE_CLOSE_ASSISTANT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXERCICE_CLOSE_ASSISTANT, ofaExerciceCloseAssistant ))
#define OFA_EXERCICE_CLOSE_ASSISTANT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXERCICE_CLOSE_ASSISTANT, ofaExerciceCloseAssistantClass ))
#define OFA_IS_EXERCICE_CLOSE_ASSISTANT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXERCICE_CLOSE_ASSISTANT ))
#define OFA_IS_EXERCICE_CLOSE_ASSISTANT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXERCICE_CLOSE_ASSISTANT ))
#define OFA_EXERCICE_CLOSE_ASSISTANT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXERCICE_CLOSE_ASSISTANT, ofaExerciceCloseAssistantClass ))

typedef struct _ofaExerciceCloseAssistantPrivate         ofaExerciceCloseAssistantPrivate;

typedef struct {
	/*< public members >*/
	myAssistant                       parent;

	/*< private members >*/
	ofaExerciceCloseAssistantPrivate *priv;
}
	ofaExerciceCloseAssistant;

typedef struct {
	/*< public members >*/
	myAssistantClass                  parent;
}
	ofaExerciceCloseAssistantClass;

GType  ofa_exercice_close_assistant_get_type( void ) G_GNUC_CONST;

void   ofa_exercice_close_assistant_run     ( ofaMainWindow *parent );

G_END_DECLS

#endif /* __OFA_EXERCICE_CLOSE_ASSISTANT_H__ */
