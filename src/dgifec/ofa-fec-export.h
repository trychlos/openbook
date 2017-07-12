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

#ifndef __OFA_FEC_EXPORT_H__
#define __OFA_FEC_EXPORT_H__

/**
 * SECTION: ofo_entry_fec
 * @short_description: #ofoEntryFec functions definition.
 * @include: core/ofo-entry-fec.h
 *
 * This is the #ofaIExporter implementation provided by the #ofoEntry
 * class to generate the FEC (Fichier des Ecritures Comptables - DGI -
 * Article A47 A-1) export file.
 */

#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define ENTRY_FEC_EXPORT_FORMAT         "FEC"

ofsIExporterFormat *ofa_fec_export_get_exporter_formats( ofaIExporter *exporter,
															GType exportable_type,
															ofaIGetter *getter );

gboolean            ofa_fec_export_run                 ( ofaIExportable *exportable );

G_END_DECLS

#endif /* __OFA_FEC_EXPORT_H__ */
