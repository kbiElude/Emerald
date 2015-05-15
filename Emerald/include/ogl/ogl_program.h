/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef OGL_PROGRAM_H
#define OGL_PROGRAM_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"

typedef enum
{
    /* ogl_program will not expose ogl_program_ub instances for each uniform block
     * determined active for the owned program object.
     */
    OGL_PROGRAM_SYNCABLE_UBS_MODE_DISABLE,

    /** Uniform block data will be synchronized by ogl_program_ub instances,
     *  each created for corresponding uniform blocks. When enabled, you should NOT
     *  manually asign values via UBOs, but rather assign values to corresponding
     *  uniform block members via ogl_program_ub_set_*() functions. Then, prior
     *  to a draw call, call ogl_program_ub_sync() to update dirty uniform block
     *  regions.
     *
     *  The same ogl_program_ub instance will be exposed to all rendering contexts.
     */
    OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL,

    /** General behavior is the same as for OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL.
     *
     *  Each rendering context will be returned a different ogl_program_ub instance.
     */
    OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_PER_CONTEXT,

} ogl_program_syncable_ubs_mode;

REFCOUNT_INSERT_DECLARATIONS(ogl_program,
                             ogl_program)

/** Attaches a fragment/geometry/vertex shader to a program object.
 *
 *  @param ogl_program Program to attach the shader to.
 *  @param ogl_shader  Shader to attach to the program.
 *
 *  Note: this function calls into rendering thread, so performance drop can be expected
 *        if the renderer is being used at the time of call.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool ogl_program_attach_shader(__in __notnull ogl_program,
                                                  __in __notnull ogl_shader);

/** Creates a new program instance.
 *
 *  Note: this function calls into rendering thread, so performance drop can be expected
 *        if the renderer is being used at the time of call.
 *
 *  @param use_syncable_ubs If true, uniform block data will be synchronized by ogl_program_ub instances,
 *                          each created for corresponding uniform blocks. When enabled, you should NOT
 *                          use gl*Uniform*() API, but rather assign values to the uniform block members via
 *                          ogl_program_ub_set_*() functions. Then, prior to a draw call, call ogl_program_ub_sync()
 *                          to update dirty uniform block regions.
 *
 *  @return New ogl_program instance.
 **/
PUBLIC EMERALD_API ogl_program ogl_program_create(__in __notnull ogl_context                   context,
                                                  __in __notnull system_hashed_ansi_string     name,
                                                  __in           ogl_program_syncable_ubs_mode syncable_ubs_mode = OGL_PROGRAM_SYNCABLE_UBS_MODE_DISABLE);

/** Detaches a fragment/geometry/vertex shader from a program object.
 *
 *  @param ogl_program Program to detach the shader from.
 *  @param ogl_shader  Shader to detach from the program.
 *
 *  Note: this function calls into rendering thread, so performance drop can be expected
 *        if the renderer is being used at the time of call.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool ogl_program_detach_shader(__in __notnull ogl_program,
                                                  __in __notnull ogl_shader);

/** Retrieves a shader that has been successfully attached to the program. Note the indexes
 *  may differ from the ones reported by GL.
 *
 *  @param ogl_program Program to retrieve the attached shader for.
 *  @param uint32_t    Index of the shader to retrieve.
 *  @param ogl_shader* Deref will be used to store the result, if successful.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool ogl_program_get_attached_shader(__in __notnull  ogl_program,
                                                        __in            uint32_t,
                                                        __out __notnull ogl_shader*);

/** Retrieves attribute descriptor for a particular index of user-provided program object.
 *
 *  Note: You need to call ogl_program_link() before using this function.
 *
 *  @param ogl_program                        Program to retrieve the attribute descriptor for.
 *  @param size_t                             Index of the attribute to retrieve.
 *  @param ogl_program_attribute_descriptor** Deref will be used to store the result, if successful.
 *
 *  @return true if successful, false otherwise
 **/
PUBLIC EMERALD_API bool ogl_program_get_attribute_by_index(__in  __notnull ogl_program,
                                                           __in            size_t,
                                                           __out __notnull const ogl_program_attribute_descriptor**);

/** TODO */
PUBLIC EMERALD_API bool ogl_program_get_attribute_by_name(__in  __notnull ogl_program,
                                                          __in  __notnull system_hashed_ansi_string,
                                                          __out __notnull const ogl_program_attribute_descriptor**);

/** Retrieves program's GL id.
 *
 *  @param ogl_program Program to retrieve the id for.
 *
 *  @return ID of the program.
 **/
PUBLIC EMERALD_API GLuint ogl_program_get_id(__in __notnull ogl_program);

/** TODO */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_program_get_name(__in __notnull ogl_program);

/** Retrieves info log for the program object.
 *
 *  Note: You need to call ogl_program_link() before using this function.
 *
 *  @param ogl_program Program object to retrieve info log for.
 *
 *  @return Info log stored as a hashed ansi string.
 */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_program_get_program_info_log(__in __notnull ogl_program);

/** Retrieves uniform descriptor for a particular index of user-provided program object.
 *
 *  Note: You need to call ogl_program_link() before using this function.
 *
 *  @param ogl_program            Program to retrieve the uniform descriptor for.
 *  @param size_t                 Index of the uniform to retrieve the descriptor for.
 *  @param ogl_program_variable** Deref will be used to store the result, if successful.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool ogl_program_get_uniform_by_index(__in  __notnull ogl_program,
                                                         __in            size_t,
                                                         __out __notnull const ogl_program_variable**);

/** TODO */
PUBLIC EMERALD_API bool ogl_program_get_uniform_by_name(__in  __notnull ogl_program,
                                                        __in  __notnull system_hashed_ansi_string,
                                                        __out __notnull const ogl_program_variable**);

/** TODO */
PUBLIC EMERALD_API bool ogl_program_get_uniform_block_by_index(__in  __notnull ogl_program     program,
                                                               __in            unsigned int    index,
                                                               __out __notnull ogl_program_ub* out_ub_ptr);

/** TODO */
PUBLIC EMERALD_API bool ogl_program_get_uniform_block_by_name(__in  __notnull ogl_program               program,
                                                              __in  __notnull system_hashed_ansi_string name,
                                                              __out __notnull ogl_program_ub*           out_ub_ptr);

/** Links a given GL program. After calling this function, you can retrieve attributes/uniform descriptors and program info log.
 *
 *  @param ogl_program Program to link.
 *
 *  @return true if link status for the program is now true, false otherwise.
 **/
PUBLIC EMERALD_API bool ogl_program_link(__in __notnull ogl_program);

/** Releases GL objects associated with a specific context: eg. per-context ogl_program_ub
 *  instances.
 *
 *  Internal usage only.
 */
PUBLIC RENDERING_CONTEXT_CALL void ogl_program_release_context_objects(__in __notnull ogl_program program,
                                                                       __in __notnull ogl_context context);

/** TODO */
PUBLIC EMERALD_API void ogl_program_set_tf_varyings(__in __notnull ogl_program    program,
                                                    __in           unsigned int   n_varyings,
                                                    __in __notnull const GLchar** varying_names,
                                                    __in           GLenum         tf_mode);

#endif /* OGL_PROGRAM_H */
