/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef OGL_SHADER_H
#define OGL_SHADER_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_shader, ogl_shader)


/** Compiles a shader. After callnig this function you can check the log provided by the underlying
 *  GL implementation by calling ogl_shader_get_shader_info_log().
 *
 *  This function calls back into rendering handler's rendering loop so you can expect a frame drop
 *  when calling.
 *
 *  @param ogl_shader Shader to compile.
 *
 *  @return True if the shader compiled successfully, false otherwise
 */
PUBLIC EMERALD_API bool ogl_shader_compile(__in __notnull ogl_shader);

/** Creates a new shader instance of given type. 
 *
 *  This function calls back into rendering handler's rendering loop so you can expect a frame drop
 *  when calling.
 *
 *  @param ogl_context               Context to create the shader in.
 *  @param ogl_shader_type           Type of the shader to create.
 *  @param system_hashed_ansi_string TODO
 *
 *  @return GL shader instance.
 **/
PUBLIC EMERALD_API ogl_shader ogl_shader_create(__in __notnull ogl_context, 
                                                __in           ogl_shader_type,
                                                __in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_shader_get_body(__in __notnull ogl_shader);

/** TODO */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_shader_get_name(__in __notnull ogl_shader);

/** TODO */
PUBLIC GLuint ogl_shader_get_id(__in __notnull ogl_shader);

/** Retrieves a shader info log. Only call this function after ogl_shader_compile() call.
 *
 *  @param ogl_shader GL shader to retrieve the log for.
 *
 *  @return Info log or NULL if unavailable.
 **/
PUBLIC EMERALD_API const char* ogl_shader_get_shader_info_log(__in __notnull ogl_shader);

/** Retrieves shader type.
 *
 *  @param ogl_shader GL shader to query for type.
 *
 *  @return Shader type.
 **/
PUBLIC EMERALD_API ogl_shader_type ogl_shader_get_type(__in __notnull ogl_shader);

/** Sets new shader's body.
 *
 *  This function calls back into rendering handler's rendering loop so you can expect a frame drop
 *  when calling.
 *  
 *  @param ogl_shader                GL shader to update body for.
 *  @param system_hashed_ansi_string Body to use for the shader.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool ogl_shader_set_body(__in __notnull ogl_shader, 
                                            __in __notnull system_hashed_ansi_string);

#endif /* OGL_SHADER_H */
