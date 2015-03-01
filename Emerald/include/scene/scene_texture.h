/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SCENE_TEXTURE_H
#define SCENE_TEXTURE_H

#include "scene/scene_types.h"

typedef enum
{
    SCENE_TEXTURE_PROPERTY_NAME,       /* system_hashed_ansi_string */
    SCENE_TEXTURE_PROPERTY_FILENAME,   /* system_hashed_ansi_string */
    SCENE_TEXTURE_PROPERTY_OGL_TEXTURE
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
 *  NOTE: Texture ID is configured exclusively by scene instance which owns the texture. */
PUBLIC EMERALD_API scene_texture scene_texture_create(__in     __notnull system_hashed_ansi_string name,
                                                      __in_opt           system_hashed_ansi_string object_manager_path,
                                                      __in     __notnull system_hashed_ansi_string filename);

/** TODO */
PUBLIC EMERALD_API void scene_texture_get(__in  __notnull scene_texture          instance,
                                          __in            scene_texture_property property,
                                          __out __notnull void*                  result);

/** TODO */
PUBLIC EMERALD_API scene_texture scene_texture_load_with_serializer(__in     __notnull system_file_serializer      serializer,
                                                                    __in_opt           system_hashed_ansi_string   object_manager_path,
                                                                    __in     __notnull ogl_context                 context,
                                                                    __in_opt           PFNSETOGLTEXTUREBACKINGPROC pGLSetOGLTextureBacking_callback,
                                                                    __in_opt           void*                       callback_user_data = NULL);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *  NOTE: This function does not spawn ogl_texture instances. This should be handled by the
 *        caller. The SCENE_TEXTURE_PROPERTY_OGL_TEXTURE property should be updated accordingly.
 */
PUBLIC bool scene_texture_save(__in __notnull system_file_serializer serializer,
                               __in __notnull scene_texture          instance);

/** TODO */
PUBLIC EMERALD_API void scene_texture_set(__in __notnull scene_texture          instance,
                                          __in           scene_texture_property property,
                                          __in __notnull void*                  value);

#endif /* SCENE_TEXTURE_H */
