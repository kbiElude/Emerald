/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef OGL_TEXTURES_H
#define OGL_TEXTURES_H

#include "ogl/ogl_types.h"

/** TODO.
 *
 *  This function is not exported */
PUBLIC void ogl_textures_add_texture(__in __notnull ogl_textures textures,
                                     __in __notnull ogl_texture  texture);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC ogl_textures ogl_textures_create(__in __notnull ogl_context context);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC void ogl_textures_delete_texture(__in __notnull ogl_textures              textures,
                                        __in __notnull system_hashed_ansi_string texture_name);

/** TODO */
PUBLIC EMERALD_API ogl_texture ogl_textures_get_texture_by_filename(__in __notnull ogl_textures              textures,
                                                                    __in __notnull system_hashed_ansi_string texture_filename);

/** TODO */
PUBLIC EMERALD_API ogl_texture ogl_textures_get_texture_by_name(__in __notnull ogl_textures              textures,
                                                                __in __notnull system_hashed_ansi_string texture_name);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC void ogl_textures_release(__in __notnull ogl_textures materials);

#endif /* OGL_TEXTURES_H */