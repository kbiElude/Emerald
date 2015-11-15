/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef RAGL_SAMPLERS_H
#define RAGL_SAMPLERS_H

#include "ogl/ogl_context.h"
#include "raGL/raGL_sampler.h"

DECLARE_HANDLE(raGL_samplers);


/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC raGL_samplers raGL_samplers_create(ogl_context context);

/** TODO.
 *
 *  Do NOT retain or release samplers retrieved by calling this function.
 **/
PUBLIC EMERALD_API raGL_sampler raGL_samplers_get_sampler(raGL_samplers samplers,
                                                          ral_sampler   sampler);

/** TODO
 *
 *  NOTE: This function will be removed after RAL integration is finished. DO NOT use.
 **/
PUBLIC EMERALD_API raGL_sampler raGL_samplers_get_sampler_from_ral_sampler_create_info(raGL_samplers                  samplers,
                                                                                       const ral_sampler_create_info* sampler_create_info_ptr);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC void raGL_samplers_release(raGL_samplers samplers);

#endif /* RAGL_SAMPLERS_H */