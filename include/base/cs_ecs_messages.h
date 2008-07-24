/*============================================================================
 *
 *     This file is part of the Code_Saturne Kernel, element of the
 *     Code_Saturne CFD tool.
 *
 *     Copyright (C) 1998-2008 EDF S.A., France
 *
 *     contact: saturne-support@edf.fr
 *
 *     The Code_Saturne Kernel is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General Public License
 *     as published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     The Code_Saturne Kernel is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *     of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with the Code_Saturne Kernel; if not, write to the
 *     Free Software Foundation, Inc.,
 *     51 Franklin St, Fifth Floor,
 *     Boston, MA  02110-1301  USA
 *
 *============================================================================*/

#ifndef __CS_ECS_MESSAGES_H__
#define __CS_ECS_MESSAGES_H__

/*============================================================================
 * Exchange of data between Code_Saturne Kernel and Preprocessor
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Standard C library headers
 *----------------------------------------------------------------------------*/

#include <stdarg.h>

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "cs_base.h"
#include "cs_mesh.h"

/*----------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#if 0
} /* Fake brace to force Emacs auto-indentation back to column 0 */
#endif
#endif /* __cplusplus */

/*============================================================================
 *  Public functions definition for Fortran API
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Receive messages from the pre-processor about the dimensions of mesh
 * parameters
 *
 * FORTRAN Interface:
 *
 * SUBROUTINE LEDEVI(NOMRUB, TYPENT, NBRENT, TABENT)
 * *****************
 *
 * INTEGER          NDIM        : <-- : Dimension de l'espace (3)
 * INTEGER          NCEL        : <-- : Nombre d'elements actifs
 * INTEGER          NFAC        : <-- : Nombre de faces internes
 * INTEGER          NFABOR      : <-- : Nombre de faces de bord
 * INTEGER          NFML        : <-- : Nombre de familles des faces de bord
 * INTEGER          NPRFML      : <-- : Nombre de proprietes max par famille
 * INTEGER          NSOM        : <-- : Nombre de sommets (optionnel)
 * INTEGER          LNDFAC      : <-- : Longueur de SOMFAC (optionnel)
 * INTEGER          LNDFBR      : <-- : Longueur de SOMFBR (optionnel)
 * INTEGER          IPERIO      : <-- : Indicateur de periodicite
 * INTEGER          IPEROT      : <-- : Nombre de periodicites de rotation
 *----------------------------------------------------------------------------*/

void CS_PROCF(ledevi, LEDEVI)
(
 cs_int_t   *const ndim,    /* <-- dimension de l'espace                      */
 cs_int_t   *const ncel,    /* <-- nombre d'elements actifs                   */
 cs_int_t   *const nfac,    /* <-- nombre de faces internes                   */
 cs_int_t   *const nfabor,  /* <-- nombre de faces de bord                    */
 cs_int_t   *const nfml,    /* <-- nombre de familles des faces de bord       */
 cs_int_t   *const nprfml,  /* <-- nombre de proprietes max par famille       */
 cs_int_t   *const nsom,    /* <-- nombre de sommets (optionnel)              */
 cs_int_t   *const lndfac,  /* <-- longueur de somfac (optionnel)             */
 cs_int_t   *const lndfbr,  /* <-- longueur de somfbr (optionnel)             */
 cs_int_t   *const iperio,  /* <-- indicateur de periodicite                  */
 cs_int_t   *const iperot   /* <-- nombre de periodicites de rotation         */
);

/*============================================================================
 *  Public functions definition
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Receive data from the pre-processor and finalize communication with the
 * pre-processor
 *
 * parameters:
 *   mesh  <-- pointer to mesh structure
 *----------------------------------------------------------------------------*/

void
cs_ecs_messages_read_data(cs_mesh_t  *mesh);

/*----------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CS_ECS_MESSAGES_H__ */

