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
    system_hash64map program_id_to_program_map;
    system_hash64map program_name_to_program_map;

    REFCOUNT_INSERT_VARIABLES

    _ogl_programs()
    {
        program_id_to_program_map   = NULL;
        program_name_to_program_map = NULL;
    }

    ~_ogl_programs()
    {
        if (program_id_to_program_map != NULL)
        {
            system_hash64map_release(program_id_to_program_map);

            program_id_to_program_map = NULL;
        } /* if (program_id_to_program_map != NULL) */

        if (program_name_to_program_map != NULL)
        {
            /* No need to release the hosted ogl_shader instances */
            system_hash64map_release(program_name_to_program_map);

            program_name_to_program_map = NULL;
        } /* if (program_name_to_program_map != NULL) */
    }
} _ogl_programs;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_programs,
                               ogl_programs,
                              _ogl_programs);


/** TODO */
PRIVATE void _ogl_programs_release(void* programs)
{
    /* Nothing to do here */
}


/** Please see header for spec */
PUBLIC ogl_programs ogl_programs_create()
{
    _ogl_programs* new_programs = new (std::nothrow) _ogl_programs;

    ASSERT_DEBUG_SYNC(new_programs != NULL,
                      "Out of memory");

    if (new_programs != NULL)
    {
        static unsigned int cnt = 0;
        char                name[32];

        sprintf(name,
                "Instance %d",
                cnt++);

        new_programs->program_id_to_program_map   = system_hash64map_create(sizeof(ogl_program),
                                                                            true); /* should_be_thread_safe */
        new_programs->program_name_to_program_map = system_hash64map_create(sizeof(ogl_program),
                                                                            true); /* should_be_thread_safe */

        ASSERT_DEBUG_SYNC(new_programs->program_id_to_program_map   != NULL &&
                          new_programs->program_name_to_program_map != NULL,
                          "Could not initialize a hash-map");

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_programs,
                                                       _ogl_programs_release,
                                                       OBJECT_TYPE_OGL_PROGRAMS,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Program Managers\\",
                                                                                                               name) );
    }

    return (ogl_programs) new_programs;
}

/** Please see header for spec */
PUBLIC ogl_program ogl_programs_get_program_by_id(__in __notnull ogl_programs programs,
                                                  __in           GLuint       po_id)
{
    ogl_program    result       = NULL;
    _ogl_programs* programs_ptr = (_ogl_programs*) programs;

    system_hash64map_get(programs_ptr->program_id_to_program_map,
                         po_id,
                        &result);

    return result;
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
    GLuint                    program_id   = ogl_program_get_id                (program);
    system_hashed_ansi_string program_name = ogl_program_get_name              (program);
    system_hash64             program_hash = system_hashed_ansi_string_get_hash(program_name);
    _ogl_programs*            programs_ptr = (_ogl_programs*) programs;

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(programs_ptr->program_id_to_program_map,
                                                 program_id),
                      "About to register an already registered program object!");
    ASSERT_DEBUG_SYNC(!system_hash64map_contains(programs_ptr->program_name_to_program_map,
                                                 program_hash),
                      "About to register an already registered program object!");

    system_hash64map_insert(programs_ptr->program_id_to_program_map,
                            program_id,
                            program,
                            NULL,  /* on_remove_callback */
                            NULL); /* on_remove_callback_user_arg */
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
    GLuint                    program_id   = ogl_program_get_id                (program);
    system_hashed_ansi_string program_name = ogl_program_get_name              (program);
    system_hash64             program_hash = system_hashed_ansi_string_get_hash(program_name);
    _ogl_programs*            programs_ptr = (_ogl_programs*) programs;

    ASSERT_DEBUG_SYNC(system_hash64map_contains(programs_ptr->program_id_to_program_map,
                                                program_id),
                      "Cannot unregister a program which has not been registered.");
    ASSERT_DEBUG_SYNC(system_hash64map_contains(programs_ptr->program_name_to_program_map,
                                                program_hash),
                      "Cannot unregister a program which has not been registered.");

    system_hash64map_remove(programs_ptr->program_id_to_program_map,
                            program_id);
    system_hash64map_remove(programs_ptr->program_name_to_program_map,
                            program_hash);
}
