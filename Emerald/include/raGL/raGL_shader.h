/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef RAGL_SHADER_H
#define RAGL_SHADER_H

#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"

typedef enum
{
    /* not settable; bool */
    RAGL_SHADER_PROPERTY_COMPILE_STATUS,

    /* not settable; GLuint */
    RAGL_SHADER_PROPERTY_ID,

    /* not settable; const char* */
    RAGL_SHADER_PROPERTY_INFO_LOG
} raGL_shader_property;


/** Compiles a shader. After this function returns, an info log is exposed via the
 *  RAGL_SHADER_PROPERTY_INFO_LOG property.
 *
 *  This function calls back into rendering handler's rendering loop so you can expect a frame drop
 *  when calling.
 *
 *  @param ogl_shader Shader to compile.
 *
 *  @return True if the shader compiled successfully, false otherwise
 */
PUBLIC bool raGL_shader_compile(raGL_shader shader);

/** Creates a new shader instance of given type.
 *
 *  This function calls back into rendering handler's rendering loop so you can expect a frame drop
 *  when calling.
 *
 *  @param context_ral TODO
 *  @param context_ogl TODO
 *  @param shader_ral  TODO
 *
 *  @return A new raGL_shader instance
 **/
PUBLIC raGL_shader raGL_shader_create(ral_context context_ral,
                                      ogl_context context_ogl,
                                      ral_shader  shader_ral);

/** TODO */
PUBLIC void raGL_shader_get_property(raGL_shader          shader,
                                     raGL_shader_property property,
                                     void*                out_result_ptr);

/** TODO */
PUBLIC void raGL_shader_release(raGL_shader shader);

#endif /* RAGL_SHADER_H */
