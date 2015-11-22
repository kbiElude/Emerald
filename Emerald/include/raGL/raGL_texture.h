#ifndef RAGL_TEXTURE_H
#define RAGL_TEXTURE_H

#include "ogl/ogl_types.h"
#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"


typedef enum
{
    /* not settable; GLuint.
     *
     * NOTE: This could be either a renderbuffer object ID or a texture object ID. You can determine
     *       this by checking value of the RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER property. */
    RAGL_TEXTURE_PROPERTY_ID,

    /* not settable; bool.
     *
     * Tells whether the instance uses a renderbuffer storage, or a texture storage. */
    RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,

    /* not settable; ral_texture */
    RAGL_TEXTURE_PROPERTY_RAL_TEXTURE,

} raGL_texture_property;


/** Creates a new raGL_texture instance. Depending on the specified usage bits, a renderbuffer object or
 *  a texture object will be created.
 *
 *  NOTE: Rendering context thread recommended.
 *
 *  @param context Rendering context.
 *  @param texture RAL texture the raGL_texture instance is created for.
 *
 *  @return A new raGL_texture instance, or NULL if the function failed.
 **/
PUBLIC RENDERING_CONTEXT_CALL raGL_texture raGL_texture_create(ogl_context context,
                                                               ral_texture texture);

/** TODO */
PUBLIC bool raGL_texture_get_property(raGL_texture          texture,
                                      raGL_texture_property property,
                                      void*                 out_result_ptr);

/** TODO */
PUBLIC void raGL_texture_release(raGL_texture texture);

/** TODO */
PUBLIC bool raGL_texture_update_with_client_sourced_data(raGL_texture                                         texture,
                                                         uint32_t                                             n_updates,
                                                         const ral_texture_mipmap_client_sourced_update_info* updates);

#endif /* RAGL_TEXTURE_H */