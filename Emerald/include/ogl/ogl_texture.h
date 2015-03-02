/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef OGL_TEXTURE_H
#define OGL_TEXTURE_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_texture, ogl_texture)

typedef enum _ogl_texture_mipmap_property
{
    OGL_TEXTURE_MIPMAP_PROPERTY_DATA_SIZE,      /* not settable, unsigned int; currently only set for loaded compressed texture mipmaps */
    OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH,          /* not settable, unsigned int */
    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,         /* not settable, unsigned int */
    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,          /* not settable, unsigned int */

    /* Always last */
    OGL_TEXTURE_MIPMAP_PROPERTY_COUNT
} ogl_texture_mipmap_property;

typedef enum _ogl_texture_property
{
    OGL_TEXTURE_PROPERTY_DIMENSIONALITY,            /* not settable, ogl_texture_dimensionality */
    OGL_TEXTURE_PROPERTY_FIXED_SAMPLE_LOCATIONS,    /* not settable, bool */
    OGL_TEXTURE_PROPERTY_HAS_BEEN_BOUND,            /* settable,     bool; tells if the texture object has been bound at least once.
                                                     *                     necessary for the state caching layer to work correctly. */
    OGL_TEXTURE_PROPERTY_HAS_HAD_MIPMAPS_GENERATED, /* not settable, bool; toggled on by ogl_texture_generate_mipmaps() call. */
    OGL_TEXTURE_PROPERTY_ID,                        /* not settable, GLuint */
    OGL_TEXTURE_PROPERTY_INTERNALFORMAT,            /* not settable, GLint */
    OGL_TEXTURE_PROPERTY_NAME,                      /* not settable, system_hashed_ansi_string */
    OGL_TEXTURE_PROPERTY_N_MIPMAPS,                 /* not settable, unsigned int */
    OGL_TEXTURE_PROPERTY_N_SAMPLES,                 /* not settable, unsigned int */
    OGL_TEXTURE_PROPERTY_SRC_FILENAME,              /* not settable, system_hashed_ansi_string */
    OGL_TEXTURE_PROPERTY_TARGET,                    /* not settable, GLenum */

    /* Always last */
    OGL_TEXTURE_PROPERTY_COUNT
} ogl_texture_property;

/** TODO */
PUBLIC EMERALD_API ogl_texture ogl_texture_create_empty(__in __notnull ogl_context               context,
                                                        __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API ogl_texture ogl_texture_create_from_file_name(__in __notnull ogl_context               context,
                                                                 __in __notnull system_hashed_ansi_string name,
                                                                 __in __notnull system_hashed_ansi_string src_filename);

/** TODO */
PUBLIC EMERALD_API ogl_texture ogl_texture_create_from_gfx_image(__in __notnull ogl_context               context,
                                                                 __in __notnull gfx_image                 image,
                                                                 __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL ogl_texture ogl_texture_create_and_initialize(__in __notnull ogl_context                context,
                                                                                        __in __notnull system_hashed_ansi_string  name,
                                                                                        __in           ogl_texture_dimensionality dimensionality,
                                                                                        __in           unsigned int               n_mipmaps,
                                                                                        __in           GLenum                     internalformat,
                                                                                        __in           unsigned int               base_mipmap_width,
                                                                                        __in           unsigned int               base_mipmap_height,
                                                                                        __in           unsigned int               base_mipmap_depth,
                                                                                        __in           unsigned int               n_samples,
                                                                                        __in           bool                       fixed_sample_locations);

/** TODO */
PUBLIC EMERALD_API void ogl_texture_generate_mipmaps(__in __notnull ogl_texture texture);

/** TODO */
PUBLIC EMERALD_API bool ogl_texture_get_mipmap_property(__in  __notnull ogl_texture                 texture,
                                                        __in            unsigned int                mipmap_level,
                                                        __in            ogl_texture_mipmap_property property_value,
                                                        __out __notnull void*                       out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_texture_get_property(__in  __notnull const ogl_texture    texture,
                                                 __in            ogl_texture_property property,
                                                 __out           void*                out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_texture_set_mipmap_property(__in __notnull ogl_texture                 texture,
                                                        __in           unsigned int                n_mipmap,
                                                        __in           ogl_texture_mipmap_property property_value,
                                                        __in           void*                       value_ptr);

/** TODO */
PUBLIC EMERALD_API void ogl_texture_set_property(__in  __notnull ogl_texture          texture,
                                                 __in            ogl_texture_property property,
                                                 __in            void*                data);

#endif /* OGL_TEXTURE_H */
