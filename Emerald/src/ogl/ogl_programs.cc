/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_programs.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"

typedef struct _ogl_programs
{
    system_hash64map program_name_to_program_map;

    _ogl_programs()
    {
        program_name_to_program_map = NULL;
    }

    ~_ogl_programs()
    {
        if (program_name_to_program_map != NULL)
        {
            /* No need to release the hosted ogl_shader instances */
            system_hash64map_release(program_name_to_program_map);

            program_name_to_program_map = NULL;
        } /* if (shader_name_to_shader_map != NULL) */
    }
} _ogl_programs;

/** Please see header for spec */
PUBLIC ogl_programs ogl_programs_create()
{
    _ogl_programs* new_programs = new (std::nothrow) _ogl_programs;

    ASSERT_DEBUG_SYNC(new_programs != NULL,
                      "Out of memory");

    if (new_programs != NULL)
    {
        new_programs->program_name_to_program_map = system_hash64map_create(sizeof(ogl_program),
                                                                            true); /* should_be_thread_safe */

        ASSERT_DEBUG_SYNC(new_programs->program_name_to_program_map != NULL,
                          "Could not initialize a hash-map");
    }

    return (ogl_programs) new_programs;
}

/** Please see header for spec */
PUBLIC ogl_program ogl_programs_get_program_by_name(__in __notnull ogl_programs              programs,
                                                    __in __notnull system_hashed_ansi_string program_has)
{
    ogl_program    result       = NULL;
    _ogl_programs* programs_ptr = (_ogl_programs*) programs;

    system_hash64map_get(programs_ptr->program_name_to_program_map,
                         system_hashed_ansi_string_get_hash(program_has),
                        &result);

    return result;
}

/** Please see header for spec */
PUBLIC void ogl_programs_release(__in __notnull ogl_programs programs)
{
    delete (_ogl_programs*) programs;

    programs = NULL;
}

/** Please see header for spec */
PUBLIC void ogl_programs_register_program(__in __notnull ogl_programs programs,
                                          __in __notnull ogl_program  program)
{
    system_hashed_ansi_string program_name = ogl_program_get_name              (program);
    system_hash64             program_hash = system_hashed_ansi_string_get_hash(program_name);
    _ogl_programs*            programs_ptr = (_ogl_programs*) programs;

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(programs_ptr->program_name_to_program_map,
                                                 program_hash),
                      "About to register an already registered program object!");

    system_hash64map_insert(programs_ptr->program_name_to_program_map,
                            program_hash,
                            program,
                            NULL,  /* on_remove_callback */
                            NULL); /* on_remove_callback_user_arg */
}

/** Please see header for spec */
PUBLIC void ogl_programs_unregister_program(__in __notnull ogl_programs programs,
                                            __in __notnull ogl_program  program)
{
    system_hashed_ansi_string program_name = ogl_program_get_name              (program);
    system_hash64             program_hash = system_hashed_ansi_string_get_hash(program_name);
    _ogl_programs*            programs_ptr = (_ogl_programs*) programs;

    ASSERT_DEBUG_SYNC(system_hash64map_contains(programs_ptr->program_name_to_program_map,
                                                program_hash),
                      "Cannot unregister a program which has not been registered.");

    system_hash64map_remove(programs_ptr->program_name_to_program_map,
                            program_hash);
}
