/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Implements a two-fold texture pool:
 *
 * a) holds textures created from gfx_image instances. Due to simplified design ;),
 *    these textures are assumed to live forever throughout the demo life-time.
 *    TODO: We will probably need to make these textures releaseable at some point.
 * b) holds re-usable textures created with ogl_textures_get_texture_from_pool().
 *    These textures should be returned by acquirers when no longer used, so that
 *    they can be recycled.
 *
 *    TODO: Implement a "garbage collector" which should release available
 *          re-usable textures if not used for a longer period of time (5-10 s?)
 *
 * NOTE: Current implementation is NOT thread-safe.
 *
 * NOTE: We are in the process of porting existing OGL_ code to an OpenGL back-end. During the process, this header
 *       will be moved to raGL/ directory and a new RAL object will be introduced to provide an abstracted way of
 *       accessing the functionality. Once that happens, you should only use this module if you don't want your content
 *       to work with the demo/ modules.
 */
#ifndef OGL_CONTEXT_TEXTURES_H
#define OGL_CONTEXT_TEXTURES_H

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"

/** TODO.
 *
 *  This function is not exported */
PUBLIC void ogl_context_textures_add_texture(ogl_context_textures textures,
                                             ogl_texture          texture);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC ogl_context_textures ogl_context_textures_create(ogl_context context);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC void ogl_context_textures_delete_texture(ogl_context_textures      textures,
                                                system_hashed_ansi_string texture_name);

/** TODO */
PUBLIC EMERALD_API ogl_texture ogl_context_textures_get_texture_by_filename(ogl_context_textures      textures,
                                                                            system_hashed_ansi_string texture_filename);

/** TODO */
PUBLIC EMERALD_API ogl_texture ogl_context_textures_get_texture_by_name(ogl_context_textures      textures,
                                                                        system_hashed_ansi_string texture_name);

/** This function checks if a texture of requested parameter is already available
 *  in the texture pool:
 *
 *  a) If so, the texture will be returned to the caller and pulled out of the pool.
 *     The caller should call ogl_textures_return_reusable() to return the texture
 *     back to the pool, when the texture is no longer needed.
 *
 *  b) If not, a new texture object of requested parameters will be instantiated
 *     and returned to the caller. Same ogl_textures_return_reusable() rule as in a)
 *     applies.
 *
 *  The returned texture object may contain data from previous requests. This also applies
 *  for the texture state, which is undefined. Make sure you re-set the state you care about
 *  prior to using the texture object.
 *  For performance reasons, the storage is not cleaned in-between; if texture invalidation
 *  extension is supported, the storage may be cleared by a glInvalidateTexImage() call.
 *
 *  @param context            Rendering context to use for potential GL texture creation requests.
 *                            For optimal performance, make sure to call ogl_textures_get_texture_from_pool()
 *                            directly from a thread, to which the context has already been bound!
 *                            Must not be NULL.
 *  @param type               RAL texture type of the texture object to return.
 *  @param base_mipmap_width  Base mip-map width.
 *  @param base_mipmap_height Base mip-map height. (if meaningful for the requested texture target)
 *  @param base_mipmap_depth  Base mip-map depth   (if meaningful for the requested texture target)
 *
 *  @return An ogl_texture instance wrapping the texture object or NULL, if the request
 *          failed for whatever reason.
 **/
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API ogl_texture ogl_context_textures_get_texture_from_pool(ogl_context        context,
                                                                                                 ral_texture_type   type,
                                                                                                 ral_texture_format format,
                                                                                                 bool               use_full_mipmap_chain,
                                                                                                 unsigned int       base_mipmap_width,
                                                                                                 unsigned int       base_mipmap_height,
                                                                                                 unsigned int       base_mipmap_depth,
                                                                                                 unsigned int       n_samples,
                                                                                                 bool               fixed_sample_locations);

/** TODO */
PUBLIC EMERALD_API void ogl_context_textures_return_reusable(ogl_context context,
                                                             ogl_texture released_texture);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC void ogl_context_textures_release(ogl_context_textures materials);

#endif /* OGL_CONTEXT_TEXTURES_H */