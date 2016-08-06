/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef RAGL_PROGRAM_H
#define RAGL_PROGRAM_H

#include "ogl/ogl_types.h"
#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"

typedef struct raGL_program_attribute
{
    int32_t                   location;
    system_hashed_ansi_string name;

    raGL_program_attribute()
    {
        location = -1;
        name     = nullptr;
    }
} raGL_program_attribute;

typedef enum
{
    /* settable; uint32_t */
    RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP
} raGL_program_block_property;

typedef struct raGL_program_variable
{
    /* Buffer variables, uniforms (default & regular uniform block): */
    uint32_t block_index;

    /* Image uniforms (default uniform block): */
    uint32_t image_unit;

    /* Common: */
    system_hashed_ansi_string name;

    /* Sampler uniforms (default uniform block): */
    uint32_t texture_unit;


    raGL_program_variable()
    {
        block_index  = -1;
        image_unit   = -1;
        name         = nullptr;
        texture_unit = -1;
    }
} raGL_program_variable;

typedef enum
{
    /* not settable; GLuint */
    RAGL_PROGRAM_PROPERTY_ID,

    /* not settable; system_hashed_ansi_string.
     *
     * This query will BLOCK until the program instance enters a "linked" state.
     **/
    RAGL_PROGRAM_PROPERTY_INFO_LOG,

    /* not settable; ral_program */
    RAGL_PROGRAM_PROPERTY_PARENT_RAL_PROGRAM
} raGL_program_property;


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
PUBLIC raGL_program raGL_program_create(ral_context context,
                                        ral_program program_ral);

/** TODO */
PUBLIC void raGL_program_get_block_property(const raGL_program          program,
                                            ral_program_block_type      block_type,
                                            uint32_t                    index,
                                            raGL_program_block_property property,
                                            void*                       out_result_ptr);

/** TODO */
PUBLIC void raGL_program_get_block_property_by_name(const raGL_program          program,
                                                    system_hashed_ansi_string   block_name,
                                                    raGL_program_block_property property,
                                                    void*                       out_result_ptr);

/** TODO */
PUBLIC void raGL_program_get_property(const raGL_program    program,
                                      raGL_program_property property,
                                      void*                 out_result_ptr);

/** TODO */
PUBLIC bool raGL_program_get_uniform_by_location(const raGL_program            program,
                                                 uint32_t                      location,
                                                 const raGL_program_variable** out_uniform_ptr);

/** TODO */
PUBLIC bool raGL_program_get_uniform_by_name(const raGL_program            program,
                                             system_hashed_ansi_string     name,
                                             const raGL_program_variable** out_uniform_ptr);

/** TODO */
PUBLIC bool raGL_program_get_vertex_attribute_by_name(const raGL_program             program,
                                                      system_hashed_ansi_string      name,
                                                      const raGL_program_attribute** out_attribute_ptr);

/** Links a given GL program. After calling this function, you can retrieve attributes/uniform descriptors and program info log.
 *
 *  @param ogl_program Program to link.
 *
 *  @return true if link status for the program is now true, false otherwise.
 **/
PUBLIC bool raGL_program_link(raGL_program program);

/** Puts raGL_program into a locked mode, in which only link requests will be handled. All other invocations will be blocked until
 *  the instance is unlocked with a raGL_program_unlock() call.
 *
 *  @param program Program to lock.
 **/
PUBLIC void raGL_program_lock(raGL_program program);

/** TODO */
PUBLIC void raGL_program_release(raGL_program program);

/** TODO */
PUBLIC void raGL_program_set_block_property(raGL_program                program,
                                            ral_program_block_type      block_type,
                                            uint32_t                    index,
                                            raGL_program_block_property property,
                                            const void*                 value_ptr);

/** TODO */
PUBLIC void raGL_program_set_block_property_by_name(raGL_program                program,
                                                    system_hashed_ansi_string   block_name,
                                                    raGL_program_block_property property,
                                                    const void*                 value_ptr);

/** See documentation for raGL_program_lock() for more details.
 *
 *  @param program Program to unlock.
 **/
PUBLIC void raGL_program_unlock(raGL_program program);

#endif /* OGL_PROGRAM_H */
