/**
 *
 * Emerald (kbi/elude @2014-2015)
 */
#ifndef OGL_CONTEXT_TEXTURE_COMPRESSION_H
#define OGL_CONTEXT_TEXTURE_COMPRESSION_H

#include "ogl/ogl_types.h"

typedef enum
{
    OGL_CONTEXT_TEXTURE_COMPRESSION_PROPERTY_N_COMPRESSED_INTERNALFORMAT, /* not settable, uint32_t */
} ogl_context_texture_compression_property;

typedef enum
{
    OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_FILE_EXTENSION, /* not settable, system_hashed_ansi_string */
    OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_GL_VALUE,       /* not settable, GLenum */
    OGL_CONTEXT_TEXTURE_COMPRESSION_ALGORITHM_PROPERTY_NAME            /* not settable, system_hashed_ansi_string */
} ogl_context_texture_compression_algorithm_property;

#pragma pack(push)
 #pragma pack(1)

 /* Defines a header for compressed blob files */
 typedef struct ogl_context_texture_compression_compressed_blob_header
 {
     unsigned char n_mipmaps;

     ogl_context_texture_compression_compressed_blob_header()
     {
         n_mipmaps = 0;
     }
 } ogl_context_texture_compression_compressed_blob_header;

 /* Defines a header used per each mipmap. This structure is repeated for each mipmap,
  * right after ogl_context_texture_compression_compressed_blob_header.
  */
 typedef struct ogl_context_texture_compression_compressed_blob_mipmap_header
 {
     unsigned short width;
     unsigned short height;
     unsigned short depth;
     unsigned int   data_size;

     ogl_context_texture_compression_compressed_blob_mipmap_header()
     {
         data_size = 0;
         depth     = 0;
         height    = 0;
         width     = 0;
     }
 } ogl_context_texture_compression_compressed_blob_mipmap_header;

#pragma pack(pop)

/* Declare private handle */
DECLARE_HANDLE(ogl_context_texture_compression);


/** TODO */
PUBLIC ogl_context_texture_compression ogl_context_texture_compression_create(ogl_context context);

/** TODO */
PUBLIC EMERALD_API void ogl_context_texture_compression_get_algorithm_property(ogl_context_texture_compression                    texture_compression,
                                                                               uint32_t                                           index,
                                                                               ogl_context_texture_compression_algorithm_property property,
                                                                               void*                                              out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_context_texture_compression_get_property(ogl_context_texture_compression          texture_compression,
                                                                     ogl_context_texture_compression_property property,
                                                                     void*                                    out_result);

/** TODO */
PUBLIC void ogl_context_texture_compression_init(ogl_context_texture_compression           texture_compression,
                                                 const ogl_context_gl_entrypoints_private* entrypoints_private_ptr);

/** TODO */
PUBLIC void ogl_context_texture_compression_release(ogl_context_texture_compression texture_compression);

#endif /* OGL_CONTEXT_TEXTURE_COMPRESSION_H */
