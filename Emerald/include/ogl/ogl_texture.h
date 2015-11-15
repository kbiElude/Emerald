/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * OpenGL-specific texture object support.
 *
 * NOTE: We are in the process of porting existing OGL_ code to an OpenGL back-end. During the process, this header
 *       will be moved to raGL/ directory and a new RAL object will be introduced to provide an abstracted way of
 *       accessing the functionality. You will still be allowed to use ogl_texture module, but for some use cases the
 *       RAL object may suit your needs better.
 */
#ifndef OGL_TEXTURE_H
#define OGL_TEXTURE_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"
#include "ral/ral_types.h"


REFCOUNT_INSERT_DECLARATIONS(ogl_texture,
                             ogl_texture)

typedef enum _ogl_texture_mipmap_property
{
    /* not settable, unsigned int; currently only set for loaded compressed texture mipmaps */
    OGL_TEXTURE_MIPMAP_PROPERTY_DATA_SIZE,

    /* not settable, unsigned int */
    OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH,

    /* not settable, unsigned int */
    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,

    /* not settable, unsigned int */
    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,

    /* Always last */
    OGL_TEXTURE_MIPMAP_PROPERTY_COUNT
} ogl_texture_mipmap_property;

typedef enum _ogl_texture_property
{
    /* not settable, bool */
    OGL_TEXTURE_PROPERTY_FIXED_SAMPLE_LOCATIONS,

    /* settable (only once), ral_texture_format */
    OGL_TEXTURE_PROPERTY_FORMAT_RAL,

    /* settable, bool;
     *
     * tells if the texture object has been bound at least once.
     * Necessary for the state caching layer to work correctly.
     */
    OGL_TEXTURE_PROPERTY_HAS_BEEN_BOUND,

    /* not settable, bool;
     *
     * toggled on by ogl_texture_generate_mipmaps() call. */
    OGL_TEXTURE_PROPERTY_HAS_HAD_MIPMAPS_GENERATED,

    /* not settable, GLuint */
    OGL_TEXTURE_PROPERTY_ID,

    /* not settable, system_hashed_ansi_string */
    OGL_TEXTURE_PROPERTY_NAME,

    /* settabe (only once), unsigned int */
    OGL_TEXTURE_PROPERTY_N_LAYERS,

    /* not settable, unsigned int */
    OGL_TEXTURE_PROPERTY_N_MIPMAPS,

    /* settable (only once), unsigned int */
    OGL_TEXTURE_PROPERTY_N_SAMPLES,

    /* not settable, system_hashed_ansi_string */
    OGL_TEXTURE_PROPERTY_SRC_FILENAME,

    /* not settable, GLenum */
    OGL_TEXTURE_PROPERTY_TARGET_GL,

    /* settable (only once), ral_texture_type */
    OGL_TEXTURE_PROPERTY_TYPE,

    /* Always last */
    OGL_TEXTURE_PROPERTY_COUNT
} ogl_texture_property;

/** TODO */
PUBLIC EMERALD_API ogl_texture ogl_texture_create_from_file_name(ogl_context               context,
                                                                 system_hashed_ansi_string name,
                                                                 system_hashed_ansi_string src_filename);

/** TODO */
PUBLIC EMERALD_API ogl_texture ogl_texture_create_from_gfx_image(ogl_context               context,
                                                                 gfx_image                 image,
                                                                 system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL ogl_texture ogl_texture_create_and_initialize(ogl_context               context,
                                                                                        system_hashed_ansi_string name,
                                                                                        ral_texture_type          type,
                                                                                        ral_texture_format        format,
                                                                                        bool                      use_full_mipmap_chain,
                                                                                        unsigned int              base_mipmap_width,
                                                                                        unsigned int              base_mipmap_height,
                                                                                        unsigned int              base_mipmap_depth,
                                                                                        unsigned int              n_samples,
                                                                                        bool                      fixed_sample_locations);

/** TODO */
PUBLIC EMERALD_API void ogl_texture_generate_mipmaps(ogl_texture texture);

/** TODO */
PUBLIC EMERALD_API bool ogl_texture_get_mipmap_property(ogl_texture                 texture,
                                                        unsigned int                mipmap_level,
                                                        ogl_texture_mipmap_property property_value,
                                                        void*                       out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_texture_get_property(const ogl_texture    texture,
                                                 ogl_texture_property property,
                                                 void*                out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_texture_set_mipmap_property(ogl_texture                 texture,
                                                        unsigned int                n_mipmap,
                                                        ogl_texture_mipmap_property property_value,
                                                        void*                       value_ptr);

/** TODO */
PUBLIC EMERALD_API void ogl_texture_set_property(ogl_texture          texture,
                                                 ogl_texture_property property,
                                                 const void*          data);

#endif /* OGL_TEXTURE_H */
