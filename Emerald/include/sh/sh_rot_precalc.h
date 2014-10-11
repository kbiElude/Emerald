/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * TODO
 *
 */
#ifndef SH_ROT_PRECALC_H
#define SH_ROT_PRECALC_H

#include "shared.h"
#include "sh/sh_config.h"
#include "sh/sh_types.h"

#ifdef INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION
    /** TODO */
    PUBLIC sh_derivative_rotation_matrices sh_derivative_rotation_matrices_create(__in  uint32_t n_bands, 
                                                                                  __in  uint32_t n_derivatives);

    /** TODO */
    PUBLIC void sh_derivative_rotation_matrices_get_krivanek_ddy_diagonal(__in __notnull sh_derivative_rotation_matrices in,
                                                                          __in           float                           rotation_angle_radians,
                                                                          __deref_out    float*);


    /** TODO */
    PUBLIC void sh_derivative_rotation_matrices_get_krivanek_dy_subdiagonal(__in __notnull sh_derivative_rotation_matrices in,
                                                                            __in           float                           rotation_angle_radians,
                                                                            __deref_out    float*);

    /** TODO */
    PUBLIC uint32_t sh_derivative_rotation_matrices_get_n_coefficients_till_band(int n_band);

    /** TODO */
    PUBLIC void sh_derivative_rotation_matrices_release(__in __notnull __post_invalid sh_derivative_rotation_matrices);
#endif /* INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION */

#endif /* SH_ROT_PRECALC_H */