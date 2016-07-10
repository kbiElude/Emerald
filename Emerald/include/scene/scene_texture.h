/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef SCENE_TEXTURE_H
#define SCENE_TEXTURE_H

#include "scene/scene_types.h"

typedef enum
{
    /* system_hashed_ansi_string */
    SCENE_TEXTURE_PROPERTY_NAME,

    /* system_hashed_ansi_string */
    SCENE_TEXTURE_PROPERTY_FILENAME,

    SCENE_TEXTURE_PROPERTY_TEXTURE_RAL
} scene_texture_property;

typedef void (*PFNSETOGLTEXTUREBACKINGPROC)(scene_texture             texture,
                                            system_hashed_ansi_string file_name,
                                            system_hashed_ansi_string texture_name,
                                            bool                      uses_mipmaps,
                                            void*                     callback_user_data);


REFCOUNT_INSERT_DECLARATIONS(scene_texture,
                             scene_texture)


/** TODO.
 *
 *  NOTE: RAL texture instance is owned exclusively by scene instanc. */
PUBLIC EMERALD_API scene_texture scene_texture_create(system_hashed_ansi_string name,
                                                      system_hashed_ansi_string object_manager_path,
                                                      system_hashed_ansi_string filename,
                                                      ral_context               context);

/** TODO */
PUBLIC EMERALD_API void scene_texture_get(scene_texture          instance,
                                          scene_texture_property property,
                                          void*                  out_result_ptr);

/** TODO */
PUBLIC EMERALD_API scene_texture scene_texture_load_with_serializer(system_file_serializer      serializer,
                                                                    system_hashed_ansi_string   object_manager_path,
                                                                    ral_context                 context,
                                                                    PFNSETOGLTEXTUREBACKINGPROC pGLSetOGLTextureBacking_callback,
                                                                    void*                       callback_user_data = NULL);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *  NOTE: This function does not spawn ogl_texture instances. This should be handled by the
 *        caller. The SCENE_TEXTURE_PROPERTY_OGL_TEXTURE property should be updated accordingly.
 */
PUBLIC bool scene_texture_save(system_file_serializer serializer,
                               scene_texture          instance);

/** TODO */
PUBLIC EMERALD_API void scene_texture_set(scene_texture          instance,
                                          scene_texture_property property,
                                          const void*            value);

#endif /* SCENE_TEXTURE_H */
