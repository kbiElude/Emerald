/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * TODO
 *
 */
#ifndef SH_SAMPLES_H
#define SH_SAMPLES_H

#include "shared.h"
#include "sh/sh_types.h"

REFCOUNT_INSERT_DECLARATIONS(sh_samples, sh_samples)

/** TODO. */
PUBLIC EMERALD_API sh_samples sh_samples_create(__in __notnull ogl_context context, __in uint32_t n_samples_squareable, __in uint32_t n_sh_bands, __in __notnull system_hashed_ansi_string name);

/** TODO */
RENDERING_CONTEXT_CALL PUBLIC EMERALD_API void sh_samples_execute(__in __notnull sh_samples);

/** TODO */
PUBLIC EMERALD_API uint32_t sh_samples_get_amount_of_bands(__in __notnull sh_samples);

/** TODO */
PUBLIC EMERALD_API uint32_t sh_samples_get_amount_of_coeffs(__in __notnull sh_samples);

/** TODO */
PUBLIC EMERALD_API uint32_t sh_samples_get_amount_of_samples(__in __notnull sh_samples);

/** TODO */
PUBLIC EMERALD_API GLuint sh_samples_get_sample_sh_coeffs_bo_id(sh_samples);

/** TODO */
PUBLIC EMERALD_API ogl_texture sh_samples_get_sample_sh_coeffs_tbo(sh_samples);

/** TODO */
PUBLIC EMERALD_API GLuint sh_samples_get_sample_theta_phi_bo_id(sh_samples);

/** TODO */
PUBLIC EMERALD_API ogl_texture sh_samples_get_sample_theta_phi_tbo(sh_samples);

/** TODO */
PUBLIC EMERALD_API GLuint sh_samples_get_sample_unit_vec_bo_id(sh_samples);

/** TODO */
PUBLIC EMERALD_API ogl_texture sh_samples_get_sample_unit_vec_tbo(sh_samples);

#endif /* SH_SAMPLES_H */