#ifndef __CS_WALL_FUNCTIONS_H__
#define __CS_WALL_FUNCTIONS_H__

/*============================================================================
 * Wall functions
 *============================================================================*/

/*
  This file is part of Code_Saturne, a general-purpose CFD tool.

  Copyright (C) 1998-2015 EDF S.A.

  This program is free software; you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation; either version 2 of the License, or (at your option) any later
  version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
  Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  Local headers
 *----------------------------------------------------------------------------*/

#include "cs_base.h"
#include "cs_turbulence_model.h"

/*----------------------------------------------------------------------------*/

BEGIN_C_DECLS

/*=============================================================================
 * Local Macro definitions
 *============================================================================*/

/*============================================================================
 * Type definition
 *============================================================================*/

/* Wall function type */
/*--------------------*/

typedef enum {

  CS_WALL_F_DISABLED,
  CS_WALL_F_1SCALE_POWER,
  CS_WALL_F_1SCALE_LOG,
  CS_WALL_F_2SCALES_LOG,
  CS_WALL_F_SCALABLE_2SCALES_LOG,
  CS_WALL_F_2SCALES_VDRIEST,

} cs_wall_function_type_t;

/* Wall functions descriptor */
/*---------------------------*/

typedef struct {

  cs_wall_function_type_t iwallf;  /* wall function type */

  int                     iwallt;  /* exchange coefficient correlation
                                      - 0: not used by default
                                      - 1: exchange coefficient computed with a
                                           correlation */

  double                  ypluli;  /* limit value of y+ for the viscous
                                      sublayer */

} cs_wall_functions_t;

/*============================================================================
 *  Global variables
 *============================================================================*/

/* Pointer to wall functions descriptor structure */

extern const cs_wall_functions_t *cs_glob_wall_functions;

/*============================================================================
 * Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*/
/*!
 * \brief Power law: Werner & Wengle.
 *
 * \param[in]     l_visc        kinematic viscosity
 * \param[in]     vel           wall projected cell center velocity
 * \param[in]     y             wall distance
 * \param[out]    iuntur        indicator: 0 in the viscous sublayer
 * \param[out]    nsubla        counter of cell in the viscous sublayer
 * \param[out]    nlogla        counter of cell in the log-layer
 * \param[out]    ustar         friction velocity
 * \param[out]    uk            friction velocity
 * \param[out]    yplus         dimensionless distance to the wall
 * \param[out]    ypup          yplus projected vel ratio
 * \param[out]    cofimp        \f$\frac{|U_F|}{|U_I^p|}\f$ to ensure a good
 *                              turbulence production
 */
/*----------------------------------------------------------------------------*/

inline static void
cs_wall_functions_1scale_power(cs_real_t   l_visc,
                               cs_real_t   vel,
                               cs_real_t   y,
                               int        *iuntur,
                               cs_lnum_t  *nsubla,
                               cs_lnum_t  *nlogla,
                               cs_real_t  *ustar,
                               cs_real_t  *uk,
                               cs_real_t  *yplus,
                               cs_real_t  *ypup,
                               cs_real_t  *cofimp)
{
  const double ypluli = cs_glob_wall_functions->ypluli;

  const double ydvisc =  y / l_visc;

  /* Compute the friction velocity ustar */

  *ustar = pow((vel/(cs_turb_apow * pow(ydvisc, cs_turb_bpow))), cs_turb_dpow);
  *uk = *ustar;
  *yplus = *ustar * ydvisc;

  /* In the viscous sub-layer: U+ = y+ */
  if (*yplus <= ypluli) {

    *ustar = sqrt(vel / ydvisc);
    *yplus = *ustar * ydvisc;
    *uk = *ustar;
    *ypup = 1.;
    *cofimp = 0.;

    /* Disable the wall funcion count the cell in the viscous sub-layer */
    *iuntur = 0;
    *nsubla += 1;

  /* In the log layer */
  } else {
    *ypup =   pow(vel, 2. * cs_turb_dpow-1.)
            / pow(cs_turb_apow, 2. * cs_turb_dpow);
    *cofimp = 1. + cs_turb_bpow
                   * pow(*ustar, cs_turb_bpow + 1. - 1./cs_turb_dpow)
                   * (pow(2., cs_turb_bpow - 1.) - 2.);

    /* Count the cell in the log layer */
    *nlogla += 1;

  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Log law: piecewise linear and log, with one velocity scale based on
 * the friction.
 *
 * \param[in]     ifac          face number
 * \param[in]     l_visc        kinematic viscosity
 * \param[in]     vel           wall projected cell center velocity
 * \param[in]     y             wall distance
 * \param[out]    iuntur        indicator: 0 in the viscous sublayer
 * \param[out]    nsubla        counter of cell in the viscous sublayer
 * \param[out]    nlogla        counter of cell in the log-layer
 * \param[out]    ustar         friction velocity
 * \param[out]    uk            friction velocity
 * \param[out]    yplus         dimensionless distance to the wall
 * \param[out]    ypup          yplus projected vel ratio
 * \param[out]    cofimp        \f$\frac{|U_F|}{|U_I^p|}\f$ to ensure a good
 *                              turbulence production
 */
/*----------------------------------------------------------------------------*/

inline static void
cs_wall_functions_1scale_log(cs_lnum_t    ifac,
                             cs_real_t    l_visc,
                             cs_real_t    vel,
                             cs_real_t    y,
                             int         *iuntur,
                             cs_lnum_t   *nsubla,
                             cs_lnum_t   *nlogla,
                             cs_real_t   *ustar,
                             cs_real_t   *uk,
                             cs_real_t   *yplus,
                             cs_real_t   *ypup,
                             cs_real_t   *cofimp)
{
  const double ypluli = cs_glob_wall_functions->ypluli;

  double ustarwer, ustarmin, ustaro, ydvisc;
  double eps = 0.001;
  int niter_max = 100;
  int iter = 0;
  double reynolds;

  /* Compute the local Reynolds number */

  ydvisc = y / l_visc;
  reynolds = vel * ydvisc;

  /*
   * Compute the friction velocity ustar
   */

  /* In the viscous sub-layer: U+ = y+ */
  if (reynolds <= ypluli * ypluli) {

    *ustar = sqrt(vel / ydvisc);
    *yplus = *ustar * ydvisc;
    *uk = *ustar;
    *ypup = 1.;
    *cofimp = 0.;

    /* Disable the wall funcion count the cell in the viscous sub-layer */
    *iuntur = 0;
    *nsubla += 1;

  /* In the log layer */
  } else {

    /* The initial value is Wener or the minimun ustar to ensure convergence */
    ustarwer = pow(fabs(vel) / cs_turb_apow / pow(ydvisc, cs_turb_bpow),
                   cs_turb_dpow);
    ustarmin = exp(-cs_turb_cstlog * cs_turb_xkappa)/ydvisc;
    ustaro = CS_MAX(ustarwer, ustarmin);
    *ustar = (cs_turb_xkappa * vel + ustaro)
           / (log(ydvisc * ustaro) + cs_turb_xkappa * cs_turb_cstlog + 1.);

    /* Iterative solving */
    for (iter = 0;   iter < niter_max
                  && fabs(*ustar - ustaro) >= eps * ustaro; iter++) {
      ustaro = *ustar;
      *ustar = (cs_turb_xkappa * vel + ustaro)
             / (log(ydvisc * ustaro) + cs_turb_xkappa * cs_turb_cstlog + 1.);
    }

    if (iter >= niter_max) {
      bft_printf(_("WARNING: non-convergence in the computation\n"
                   "******** of the friction velocity\n\n"
                   "face number: %d \n"
                   "friction vel: %f \n" ), ifac, *ustar);
    }

    *uk = *ustar;
    *yplus = *ustar * ydvisc;
    *ypup = *yplus / (log(*yplus) / cs_turb_xkappa + cs_turb_cstlog);
    *cofimp = 1. - *ypup / cs_turb_xkappa * 1.5 / *yplus;

    /* Count the cell in the log layer */
    *nlogla += 1;

  }

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Log law: piecewise linear and log, with two velocity scales based on
 * the friction and the turbulent kinetic energy.
 *
 * \param[in]     l_visc        kinematic viscosity
 * \param[in]     t_visc        turbulent kinematic viscosity
 * \param[in]     vel           wall projected cell center velocity
 * \param[in]     y             wall distance
 * \param[in]     kinetic_en    turbulente kinetic energy
 * \param[out]    iuntur        indicator: 0 in the viscous sublayer
 * \param[out]    nsubla        counter of cell in the viscous sublayer
 * \param[out]    nlogla        counter of cell in the log-layer
 * \param[out]    ustar         friction velocity
 * \param[out]    uk            friction velocity
 * \param[out]    yplus         dimensionless distance to the wall
 * \param[out]    ypup          yplus projected vel ratio
 * \param[out]    cofimp        \f$\frac{|U_F|}{|U_I^p|}\f$ to ensure a good
 *                              turbulence production
 */
/*----------------------------------------------------------------------------*/

inline static void
cs_wall_functions_2scales_log(cs_real_t   l_visc,
                              cs_real_t   t_visc,
                              cs_real_t   vel,
                              cs_real_t   y,
                              cs_real_t   kinetic_en,
                              int        *iuntur,
                              cs_lnum_t  *nsubla,
                              cs_lnum_t  *nlogla,
                              cs_real_t  *ustar,
                              cs_real_t  *uk,
                              cs_real_t  *yplus,
                              cs_real_t  *ypup,
                              cs_real_t  *cofimp)
{
  const double ypluli = cs_glob_wall_functions->ypluli;

  double rcprod, ml_visc, Re, g;

  /* Compute the friction velocity ustar */

  /* Blending for very low values of k */
  Re = sqrt(kinetic_en) * y / l_visc;
  g = exp(-Re/11.);

  *uk = sqrt( (1.-g) * cs_turb_cmu025 * cs_turb_cmu025 * kinetic_en
            + g * l_visc * vel / y);

  *yplus = *uk * y / l_visc;

  /* log layer */
  if (*yplus > ypluli) {

    *ustar = vel / (log(*yplus) / cs_turb_xkappa + cs_turb_cstlog);
    *ypup = *yplus / (log(*yplus) / cs_turb_xkappa + cs_turb_cstlog);
    /* Mixing length viscosity */
    ml_visc = cs_turb_xkappa * l_visc * *yplus;
    rcprod = CS_MIN(cs_turb_xkappa, CS_MAX(1., sqrt(ml_visc / t_visc)) / *yplus);
    *cofimp = 1. - *ypup / cs_turb_xkappa * ( 2. * rcprod - 1. / (2. * *yplus));

    *nlogla += 1;

  /* viscous sub-layer */
  } else {

    if (*yplus > 1.e-12) {
      *ustar = fabs(vel / *yplus); /* FIXME remove that: its is here only to
                                      be fully equivalent to the former code. */
    } else {
      *ustar = 0.;
    }
    *ypup = 1.;
    *cofimp = 0.;

    *iuntur = 0;
    *nsubla += 1;

  }
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Scalable wall function: shift the wall if \f$ y^+ < y^+_{lim} \f$.
 *
 * \param[in]     l_visc        kinematic viscosity
 * \param[in]     t_visc        turbulent kinematic viscosity
 * \param[in]     vel           wall projected cell center velocity
 * \param[in]     y             wall distance
 * \param[in]     kinetic_en    turbulente kinetic energy
 * \param[out]    iuntur        indicator: 0 in the viscous sublayer
 * \param[out]    nsubla        counter of cell in the viscous sublayer
 * \param[out]    nlogla        counter of cell in the log-layer
 * \param[out]    ustar         friction velocity
 * \param[out]    uk            friction velocity
 * \param[out]    yplus         dimensionless distance to the wall
 * \param[out]    dplus         dimensionless shift to the wall for scalable
 *                              wall functions
 * \param[out]    ypup          yplus projected vel ratio
 * \param[out]    cofimp        \f$\frac{|U_F|}{|U_I^p|}\f$ to ensure a good
 *                              turbulence production
 */
/*----------------------------------------------------------------------------*/

inline static void
cs_wall_functions_2scales_scalable(cs_real_t   l_visc,
                                   cs_real_t   t_visc,
                                   cs_real_t   vel,
                                   cs_real_t   y,
                                   cs_real_t   kinetic_en,
                                   int        *iuntur,
                                   cs_lnum_t  *nsubla,
                                   cs_lnum_t  *nlogla,
                                   cs_real_t  *ustar,
                                   cs_real_t  *uk,
                                   cs_real_t  *yplus,
                                   cs_real_t  *dplus,
                                   cs_real_t  *ypup,
                                   cs_real_t  *cofimp)
{
  const double ypluli = cs_glob_wall_functions->ypluli;

  double rcprod, ml_visc, Re, g;
  /* Compute the friction velocity ustar */

  /* Blending for very low values of k */
  Re = sqrt(kinetic_en) * y / l_visc;
  g = exp(-Re/11.);

  *uk = sqrt( (1.-g) * cs_turb_cmu025 * cs_turb_cmu025 * kinetic_en
            + g * l_visc * vel / y);

  *yplus = *uk * y / l_visc;

  /* Compute the friction velocity ustar */
  *uk = cs_turb_cmu025 * sqrt(kinetic_en);
  *yplus = *uk * y / l_visc;

  /* Log layer */
  if (*yplus > ypluli) {

    *dplus = 0.;

    *nlogla += 1;

  /* Viscous sub-layer and therefore shift */
  } else {

    *dplus = ypluli - *yplus;
    *yplus = ypluli;

    /* Count the cell as if it was in the viscous sub-layer */
    *nsubla += 1;

  }

  /* Mixing length viscosity */
  ml_visc = cs_turb_xkappa * l_visc * *yplus;
  rcprod = CS_MIN(cs_turb_xkappa, CS_MAX(1., sqrt(ml_visc / t_visc)) / *yplus);

  *ustar = vel / (log(*yplus) / cs_turb_xkappa + cs_turb_cstlog);
  *ypup = (*yplus - *dplus) / (log(*yplus) / cs_turb_xkappa + cs_turb_cstlog);
  *cofimp = 1. - *ypup
                 / cs_turb_xkappa * (2. * rcprod - 1. / (2. * *yplus - *dplus));
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Two velocity scales wall function with Van Driest mixing length.
 *
 * \f$ u^+ \f$ is computed as follows:
 *   \f[ u^+ = \int_0^{y_k^+} \dfrac{dy_k^+}{1+L_m^k} \f]
 * with \f$ L_m^k \f$ standing for Van Driest mixing length:
 *  \f[ L_m^k = \kappa y_k^+ (1 - exp(\dfrac{-y_k^+}{A})) \f].
 *
 * A polynome fitting the integral is used for \f$ y_k^+ < 200 \f$,
 * and a log law is used for \f$ y_k^+ >= 200 \f$.
 *
 * \param[in]     rnnb          \f$\vec{n}.(\tens{R}\vec{n})\f$
 * \param[in]     l_visc        kinematic viscosity
 * \param[in]     t_visc        turbulent kinematic viscosity
 * \param[in]     vel           wall projected cell center velocity
 * \param[in]     y             wall distance
 * \param[in]     kinetic_en    turbulente kinetic energy
 * \param[out]    iuntur        indicator: 0 in the viscous sublayer
 * \param[out]    nsubla        counter of cell in the viscous sublayer
 * \param[out]    nlogla        counter of cell in the log-layer
 * \param[out]    ustar         friction velocity
 * \param[out]    uk            friction velocity
 * \param[out]    yplus         dimensionless distance to the wall
 * \param[out]    ypup          yplus projected vel ratio
 * \param[out]    cofimp        \f$\frac{|U_F|}{|U_I^p|}\f$ to ensure a good
 *                              turbulence production
 * \param[in]     lmk           dimensionless mixing length
 * \param[in]     kr            wall roughness
 * \param[in]     wf            enable full wall function computation,
 *                              if false, uk is not recomputed and uplus is the
 *                              only output
 */
/*----------------------------------------------------------------------------*/

inline static void
cs_wall_functions_2scales_vdriest(cs_real_t   rnnb,
                                  cs_real_t   l_visc,
                                  cs_real_t   vel,
                                  cs_real_t   y,
                                  cs_real_t   kinetic_en,
                                  int        *iuntur,
                                  cs_lnum_t  *nsubla,
                                  cs_lnum_t  *nlogla,
                                  cs_real_t  *ustar,
                                  cs_real_t  *uk,
                                  cs_real_t  *yplus,
                                  cs_real_t  *ypup,
                                  cs_real_t  *cofimp,
                                  cs_real_t  *lmk,
                                  cs_real_t   kr,
                                  bool        wf)
{
  double y1,y2,y3,y4,y5,y6,y7,y8,y9,y10;
  double uplus, lmk15;

  /* Coefficients of the polynome fitting u+ for yk < 200 */
  static double aa[11] = {-0.0091921, 3.9577, 0.031578,
                          -0.51013, -2.3254, -0.72665,
                          2.969, 0.48506, -1.5944,
                          0.087309, 0.1987 };
  if (wf)
    *uk = sqrt(sqrt((1.-cs_turb_crij2)/cs_turb_crij1 * rnnb * kinetic_en));

  /* Set a low threshold value in case tangential velocity is zero */
  *yplus = CS_MAX(*uk * y / l_visc, 1.e-4);

  /* Dimensionless roughness */
  cs_real_t krp = *uk * kr / l_visc;

  /* Extension of Van Driest mixing length according to Rotta (1962) with
     Cebeci & Chang (1978) correlation */
  cs_real_t dyrp = 0.9 * (sqrt(krp) - krp * exp(-krp / 6.));
  cs_real_t yrplus = *yplus + dyrp;

  if (yrplus <= 1.e-1) {

    uplus = *yplus;

    if (wf) {
      *iuntur = 0;
      *nsubla += 1;

      *lmk = 0.;

      *ypup = 1.;

      *cofimp = 0.;
    }

  } else if (yrplus <= 200.) {

    y1 = 0.25 * log(yrplus);
    y2 = y1 * y1;
    y3 = y2 * y1;
    y4 = y3 * y1;
    y5 = y4 * y1;
    y6 = y5 * y1;
    y7 = y6 * y1;
    y8 = y7 * y1;
    y9 = y8 * y1;
    y10= y9 * y1;

    uplus =   aa[0]
            + aa[1]  * y1
            + aa[2]  * y2
            + aa[3]  * y3
            + aa[4]  * y4
            + aa[5]  * y5
            + aa[6]  * y6
            + aa[7]  * y7
            + aa[8]  * y8
            + aa[9]  * y9
            + aa[10] * y10;

    uplus = exp(uplus);

    if (wf) {
      *nlogla += 1;

      *ypup = *yplus / uplus;

      /* Mixing length in y+ */
      *lmk = cs_turb_xkappa * (*yplus) *(1-exp(- (*yplus) / cs_turb_vdriest));

      /* Mixing length in 3/2*y+ */
      lmk15 = cs_turb_xkappa * 1.5 * (*yplus) *(1-exp(- 1.5 * (*yplus)
                                                    / cs_turb_vdriest));

      *cofimp = 1. - (2. / (1. + *lmk) - 1. / (1. + lmk15)) * *ypup;
    }

  } else {

    uplus = 16.088739022054590 + log((yrplus)/(200.+dyrp)) / cs_turb_xkappa;

    if (wf) {
      *nlogla += 1;

      *ypup = *yplus / uplus;

      /* Mixing length in y+ */
      *lmk = cs_turb_xkappa * (*yplus) *(1-exp(- (*yplus) / cs_turb_vdriest));

      /* Mixing length in 3/2*y+ */
      lmk15 = cs_turb_xkappa * 1.5 * (*yplus) *(1-exp(- 1.5 * (*yplus)
                                                    / cs_turb_vdriest));

      *cofimp = 1. - (2. / *lmk - 1. / lmk15) * *ypup;
    }

  }

  *ustar = vel / uplus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief No wall function.
 *
 * \param[in]     l_visc        kinematic viscosity
 * \param[in]     t_visc        turbulent kinematic viscosity
 * \param[in]     vel           wall projected cell center velocity
 * \param[in]     y             wall distance
 * \param[out]    iuntur        indicator: 0 in the viscous sublayer
 * \param[out]    nsubla        counter of cell in the viscous sublayer
 * \param[out]    nlogla        counter of cell in the log-layer
 * \param[out]    ustar         friction velocity
 * \param[out]    uk            friction velocity
 * \param[out]    yplus         dimensionless distance to the wall
 * \param[out]    dplus         dimensionless shift to the wall for scalable
 *                              wall functions
 * \param[out]    ypup          yplus projected vel ratio
 * \param[out]    cofimp        \f$\frac{|U_F|}{|U_I^p|}\f$ to ensure a good
 *                              turbulence production
 */
/*----------------------------------------------------------------------------*/

inline static void
cs_wall_functions_disabled(cs_real_t   l_visc,
                           cs_real_t   t_visc,
                           cs_real_t   vel,
                           cs_real_t   y,
                           int        *iuntur,
                           cs_lnum_t  *nsubla,
                           cs_lnum_t  *nlogla,
                           cs_real_t  *ustar,
                           cs_real_t  *uk,
                           cs_real_t  *yplus,
                           cs_real_t  *dplus,
                           cs_real_t  *ypup,
                           cs_real_t  *cofimp)
{
  const double ypluli = cs_glob_wall_functions->ypluli;

  /* Compute the friction velocity ustar */

  *ustar = sqrt(vel * l_visc / y);
  *yplus = *ustar * y / l_visc;
  *uk = *ustar;
  *ypup = l_visc / (l_visc + t_visc);
  *cofimp = 0.;
  *iuntur = 0;

  if (*yplus <= ypluli) {

    /* Disable the wall funcion count the cell in the viscous sub-layer */
    *nsubla += 1;

  } else {

    /* Count the cell as if it was in the viscous sub-layer */
    *nsubla += 1;

  }
}

/*============================================================================
 * Public function definitions for Fortran API
 *============================================================================*/

/*----------------------------------------------------------------------------
 * Wrapper to cs_wall_functions_velocity.
 *----------------------------------------------------------------------------*/

void CS_PROCF (wallfunctions, WALLFUNCTIONS)
(
 const cs_int_t   *const iwallf,
 const cs_lnum_t  *const ifac,
 const cs_real_t  *const viscosity,
 const cs_real_t  *const t_visc,
 const cs_real_t  *const vel,
 const cs_real_t  *const y,
 const cs_real_t  *const rnnb,
 const cs_real_t  *const kinetic_en,
       cs_int_t         *iuntur,
       cs_lnum_t        *nsubla,
       cs_lnum_t        *nlogla,
       cs_real_t        *ustar,
       cs_real_t        *uk,
       cs_real_t        *yplus,
       cs_real_t        *ypup,
       cs_real_t        *cofimp,
       cs_real_t        *dplus
);

/*----------------------------------------------------------------------------
 * Wrapper to cs_wall_functions_scalar.
 *----------------------------------------------------------------------------*/

void CS_PROCF (hturbp, HTURBP)
(
 const cs_real_t  *const prl,
 const cs_real_t  *const prt,
 const cs_real_t  *const yplus,
 const cs_real_t  *const dplus,
       cs_real_t        *htur,
       cs_real_t        *yplim
);

/*=============================================================================
 * Public function prototypes
 *============================================================================*/

/*! \brief  Compute the friction velocity and \f$y^+\f$ / \f$u^+\f$.

*/
/*-------------------------------------------------------------------------------
  Arguments
 ______________________________________________________________________________.
   mode           name          role                                           !
 ______________________________________________________________________________*/
/*!
 * \param[in]     iwallf        wall function type
 * \param[in]     ifac          face number
 * \param[in]     l_visc        kinematic viscosity
 * \param[in]     t_visc        turbulent kinematic viscosity
 * \param[in]     vel           wall projected
 *                              cell center velocity
 * \param[in]     y             wall distance
 * \param[in]     rnnb          \f$\vec{n}.(\tens{R}\vec{n})\f$
 * \param[in]     kinetic_en    turbulente kinetic energy
 * \param[in]     iuntur        indicator:
 *                              0 in the viscous sublayer
 * \param[in]     nsubla        counter of cell in the viscous
 *                              sublayer
 * \param[in]     nlogla        counter of cell in the log-layer
 * \param[out]    ustar         friction velocity
 * \param[out]    uk            friction velocity
 * \param[out]    yplus         non-dimension wall distance
 * \param[out]    ypup          yplus projected vel ratio
 * \param[out]    cofimp        \f$\frac{|U_F|}{|U_I^p|}\f$ to ensure a good
 *                              turbulence production
 * \param[out]    dplus         dimensionless shift to the wall
 *                              for scalable wall functions
 */
/*-------------------------------------------------------------------------------*/

void
cs_wall_functions_velocity(cs_wall_function_type_t  iwallf,
                           cs_lnum_t                ifac,
                           cs_real_t                l_visc,
                           cs_real_t                t_visc,
                           cs_real_t                vel,
                           cs_real_t                y,
                           cs_real_t                rnnb,
                           cs_real_t                kinetic_en,
                           int                     *iuntur,
                           cs_lnum_t               *nsubla,
                           cs_lnum_t               *nlogla,
                           cs_real_t               *ustar,
                           cs_real_t               *uk,
                           cs_real_t               *yplus,
                           cs_real_t               *ypup,
                           cs_real_t               *cofimp,
                           cs_real_t               *dplus);

/*-------------------------------------------------------------------------------*/

/*! \brief Compute the correction of the exchange coefficient between the fluid and
  the wall for a turbulent flow.

  This is function of the dimensionless
  distance to the wall \f$ y^+ = \dfrac{\centip \centf u_\star}{\nu}\f$.

  Then the return coefficient reads:
  \f[
  h_{tur} = Pr \dfrac{y^+}{T^+}
  \f]

  This coefficient is computed thanks to a similarity model between
  dynamic viscous sub-layer and themal sub-layer.

  \f$ T^+ \f$ is computed as follows:

  - For a laminar Prandtl number smaller than 0.1 (such as liquid metals),
    the standard model with two sub-layers (Prandtl-Taylor) is used.

  - For a laminar Prandtl number larger than 0.1 (such as liquids and gaz),
    a model with three sub-layers (Arpaci-Larsen) is used.

  The final exchange coefficient is:
  \f[
  h = \dfrac{K}{\centip \centf} h_{tur}
  \f]

*/
/*-------------------------------------------------------------------------------
  Arguments
 ______________________________________________________________________________.
   mode           name          role                                           !
 ______________________________________________________________________________*/
/*!
 * \param[in]     prl           laminar Prandtl number
 * \param[in]     prt           turbulent Prandtl number
 * \param[in]     yplus         dimensionless distance to the wall
 * \param[in]     dplus         dimensionless distance for scalable
 *                              wall functions
 * \param[out]    htur          corrected exchange coefficient
 * \param[out]    yplim         value of the limit for \f$ y^+ \f$
 */
/*-------------------------------------------------------------------------------*/

void
cs_wall_functions_scalar(double  prl,
                         double  prt,
                         double  yplus,
                         double  dplus,
                         double  *htur,
                         double  *yplim);

/*----------------------------------------------------------------------------*/

END_C_DECLS

#endif /* __CS_WALL_FUNCTIONS_H__ */
