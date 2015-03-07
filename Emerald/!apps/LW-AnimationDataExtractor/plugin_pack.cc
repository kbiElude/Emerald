#define _MSWIN

#include "shared.h"

#include "plugin.h"
#include "plugin_common.h"
#include "plugin_pack.h"
#include "system/system_assertions.h"
#include "system/system_file_packer.h"
#include "system/system_resizable_vector.h"


/* Global data */
system_resizable_vector enqueued_files_vector = NULL;


/** TODO */
PUBLIC void AddFileToFinalBlob(__in __notnull system_hashed_ansi_string filename)
{
    ASSERT_DEBUG_SYNC(enqueued_files_vector != NULL,
                      "Enqueued files vector is NULL");

    if (system_resizable_vector_find(enqueued_files_vector,
                                     filename) == ITEM_NOT_FOUND)
    {
        system_resizable_vector_push(enqueued_files_vector,
                                     filename);
    }
}

/** TODO */
PUBLIC void DeinitPackData()
{
    if (enqueued_files_vector != NULL)
    {
        system_resizable_vector_release(enqueued_files_vector);

        enqueued_files_vector = NULL;
    }
}

/* Please see header for spec */
PUBLIC void InitPackData()
{
    ASSERT_DEBUG_SYNC(enqueued_files_vector == NULL,
                      "Enqueued files vector already initialized");

    enqueued_files_vector = system_resizable_vector_create(4, /* capacity */
                                                           sizeof(system_hashed_ansi_string) );
}

/* Please see header for spec */
PUBLIC void SaveFinalBlob(__in __notnull system_hashed_ansi_string packed_scene_filename)
{
    const uint32_t n_enqueued_files = system_resizable_vector_get_amount_of_elements(enqueued_files_vector);

    ASSERT_DEBUG_SYNC(enqueued_files_vector != NULL,
                      "Enqueued files vector is NULL");

    if (n_enqueued_files > 0)
    {
        message_funcs_ptr->info("Setting up for packed blob baking..",
                                NULL);

        /* Use Emerald's packer to do the dirty job */
        system_file_packer packer = system_file_packer_create();

        for (uint32_t n_file = 0;
                      n_file < n_enqueued_files;
                    ++n_file)
        {
            system_hashed_ansi_string enqueued_filename = NULL;

            system_resizable_vector_get_element_at(enqueued_files_vector,
                                                   n_file,
                                                  &enqueued_filename);

            if (!system_file_packer_add_file(packer,
                                             enqueued_filename) )
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Could not register file [%s] for packing.",
                                   system_hashed_ansi_string_get_buffer(enqueued_filename) );
            }
        } /* for (all enqueued files) */

        message_funcs_ptr->info("Saving the packed blob..",
                                NULL);

        if (!system_file_packer_save(packer,
                                     packed_scene_filename) )
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not save a final packed scene blob.");
        }

        system_file_packer_release(packer);
        packer = NULL;

        message_funcs_ptr->info("Process completed.",
                                NULL);
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "No files stored in the enqueued files vector!");
    }
}
