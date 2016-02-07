/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef RAGL_PROGRAM_H
#define RAGL_PROGRAM_H

#include "ogl/ogl_types.h"
#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"

typedef enum
{
    /* not settable; GLuint */
    RAGL_PROGRAM_PROPERTY_ID,

    /* not settable; system_hashed_ansi_string */
    RAGL_PROGRAM_PROPERTY_INFO_LOG

} raGL_program_property;

typedef enum
{
    /* raGL_program will not expose raGL_program_ub instances for each uniform block
     * determined active for the owned program object.
     */
    RAGL_PROGRAM_SYNCABLE_UBS_MODE_DISABLE,

    /** Uniform block data will be synchronized by ogl_program_ub instances,
     *  each created for corresponding uniform blocks. When enabled, you should NOT
     *  manually assign values via UBOs, but rather assign values to corresponding
     *  uniform block members via raGL_program_ub_set_*() functions. Then, prior
     *  to a draw call, call raGL_program_ub_sync() to update dirty uniform block
     *  regions.
     *
     *  The same raGL_program_ub instance will be exposed to all rendering contexts.
     */
    RAGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL,

    /** General behavior is the same as for OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL.
     *
     *  Each rendering context will be returned a different raGL_program_ub instance.
     */
    RAGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_PER_CONTEXT,

} raGL_program_syncable_ubs_mode;


/** TODO */
PUBLIC bool raGL_program_attach_shader(raGL_program program_raGL,
                                       raGL_shader  shader_raGL);

/** Creates a new program instance.
 *
 *  Note: this function calls into rendering thread, so performance drop can be expected
 *        if the renderer is being used at the time of call.
 *
 *  @return New ogl_program instance.
 **/
PUBLIC raGL_program raGL_program_create(ral_context                    context,
                                        ral_program                    program_ral,
                                        raGL_program_syncable_ubs_mode syncable_ubs_mode = RAGL_PROGRAM_SYNCABLE_UBS_MODE_DISABLE);

/** TODO */
PUBLIC void raGL_program_fill_ral_program_variable(raGL_program          program,
                                                   unsigned int          temp_variable_name_storage_size,
                                                   char*                 temp_variable_name_storage,
                                                   ral_program_variable* variable_ptr,
                                                   GLenum                variable_interface_type,
                                                   unsigned int          n_variable);

/** TODO */
PUBLIC EMERALD_API void raGL_program_get_property(raGL_program          program,
                                                  raGL_program_property property,
                                                  void*                 out_result_ptr);

/** TODO */
PUBLIC EMERALD_API bool raGL_program_get_shader_storage_block_by_sb_index(raGL_program     program,
                                                                          unsigned int     index,
                                                                          ogl_program_ssb* out_ssb_ptr);

/** TODO */
PUBLIC EMERALD_API bool raGL_program_get_shader_storage_block_by_name(raGL_program              program,
                                                                      system_hashed_ansi_string name,
                                                                      ogl_program_ssb*          out_ssb_ptr);

/** Retrieves uniform descriptor for a particular index of user-provided program object.
 *
 *  Note: You need to call ogl_program_link() before using this function.
 *
 *  @param program         Program to retrieve the uniform descriptor for.
 *  @param index           Index of the uniform to retrieve the descriptor for.
 *  @param out_uniform_ptr Deref will be used to store the result, if successful.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool raGL_program_get_uniform_by_index(raGL_program                 program,
                                                          size_t                       index,
                                                          const ral_program_variable** out_uniform_ptr);

/** TODO */
PUBLIC EMERALD_API bool raGL_program_get_uniform_by_name(raGL_program                 program,
                                                         system_hashed_ansi_string    index,
                                                         const ral_program_variable** out_uniform_ptr);

/** TODO */
PUBLIC EMERALD_API bool raGL_program_get_uniform_block_by_ub_index(raGL_program    program,
                                                                   unsigned int    index,
                                                                   ogl_program_ub* out_ub_ptr);

/** TODO */
PUBLIC EMERALD_API bool raGL_program_get_uniform_block_by_name(raGL_program              program,
                                                               system_hashed_ansi_string name,
                                                               ogl_program_ub*           out_ub_ptr);

/** Retrieves vertex attribute descriptor for a particular index of user-provided program object.
 *
 *  Note: You need to call raGL_program_link() before using this function.
 *
 *  @param program                  Program to retrieve the attribute descriptor for.
 *  @param index                    Index of the attribute to retrieve.
 *  @param out_vertex_attribute_ptr Deref will be used to store the result, if successful.
 *
 *  @return true if successful, false otherwise
 **/
PUBLIC EMERALD_API bool raGL_program_get_vertex_attribute_by_index(raGL_program                  program,
                                                                   size_t                        index,
                                                                   const ral_program_attribute** out_vertex_attribute_ptr);

/** TODO */
PUBLIC EMERALD_API bool raGL_program_get_vertex_attribute_by_name(raGL_program                  program,
                                                                  system_hashed_ansi_string     index,
                                                                  const ral_program_attribute** out_vertex_attribute_ptr);

/** Links a given GL program. After calling this function, you can retrieve attributes/uniform descriptors and program info log.
 *
 *  @param ogl_program Program to link.
 *
 *  @return true if link status for the program is now true, false otherwise.
 **/
PUBLIC bool raGL_program_link(raGL_program program);

/** TODO */
PUBLIC void raGL_program_release(raGL_program program);

/** Releases GL objects associated with a specific context: eg. per-context ogl_program_ub
 *  instances.
 *
 *  Internal usage only.
 */
PUBLIC RENDERING_CONTEXT_CALL void raGL_program_release_context_objects(raGL_program program,
                                                                        ogl_context  context);

#endif /* OGL_PROGRAM_H */
