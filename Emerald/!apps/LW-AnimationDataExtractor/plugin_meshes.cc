#define _MSWIN

#include "shared.h"
#include "lw/lw_mesh_dataset.h"

#include "plugin.h"
#include "plugin_meshes.h"

#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_resizable_vector.h"


typedef struct _mesh_instance
{
    system_hashed_ansi_string filename;
    bool                      is_shadow_caster;
    bool                      is_shadow_receiver;
    system_hashed_ansi_string name;

    explicit _mesh_instance()
    {
        filename           = NULL;
        is_shadow_caster   = false;
        is_shadow_receiver = false;
        name               = NULL;
    }
} _mesh_instance;

/** TODO */
void FillMeshDataset(lw_dataset dataset)
{
    lw_mesh_dataset mesh_dataset = NULL;

    lw_dataset_get_property(dataset,
                            LW_DATASET_PROPERTY_MESH_DATASET,
                           &mesh_dataset);

    /* Iterate over all objects */
    LWItemID                object_id = item_info_ptr->first          (LWI_OBJECT, LWITEM_NULL);
    system_resizable_vector objects   = system_resizable_vector_create(4, /* capacity */
                                                                       sizeof(_mesh_instance*) );

    while (object_id != 0)
    {
        _mesh_instance* new_instance = new (std::nothrow) _mesh_instance;

        new_instance->filename           = system_hashed_ansi_string_create(object_info_ptr->filename  (object_id) );
        new_instance->is_shadow_caster   =                                 (object_info_ptr->shadowOpts(object_id) & LWOSHAD_CAST)    != 0;
        new_instance->is_shadow_receiver =                                 (object_info_ptr->shadowOpts(object_id) & LWOSHAD_RECEIVE) != 0;
        new_instance->name               = system_hashed_ansi_string_create(item_info_ptr->name        (object_id) );

        system_resizable_vector_push(objects,
                                     new_instance);

        /* Move to the next object */
        object_id = item_info_ptr->next(object_id);
    } /* while (object_id != 0) */

    /* NOTE: The objects we've created above are an over-kill for the time being, but they
     *       will be necessary once we introduce mesh data exporting.
     */
    const uint32_t n_objects = system_resizable_vector_get_amount_of_elements(objects);

    for (uint32_t n_object = 0;
                  n_object < n_objects;
                ++n_object)
    {
        _mesh_instance* instance_ptr = NULL;

        if (!system_resizable_vector_get_element_at(objects,
                                                    n_object,
                                                   &instance_ptr) )
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not retrieve object descriptor");

            continue;
        }

        /* Add the instance to the data-set */
        lw_mesh_dataset_mesh_id mesh_instance_dataset_id = lw_mesh_dataset_add_mesh(mesh_dataset,
                                                                                    instance_ptr->name);

        lw_mesh_dataset_set_mesh_property(mesh_dataset,
                                          mesh_instance_dataset_id,
                                          LW_MESH_DATASET_MESH_PROPERTY_FILENAME,
                                         &instance_ptr->filename);
        lw_mesh_dataset_set_mesh_property(mesh_dataset,
                                          mesh_instance_dataset_id,
                                          LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_CASTER,
                                         &instance_ptr->is_shadow_caster);
        lw_mesh_dataset_set_mesh_property(mesh_dataset,
                                          mesh_instance_dataset_id,
                                          LW_MESH_DATASET_MESH_PROPERTY_IS_SHADOW_RECEIVER,
                                         &instance_ptr->is_shadow_receiver);
    } /* for (all objects) */

    /* Release all object descriptors */
    _mesh_instance* object_ptr = NULL;

    while (system_resizable_vector_pop(objects,
                                      &object_ptr) )
    {
        delete object_ptr;

        object_ptr = NULL;
    }

    system_resizable_vector_release(objects);
    objects = NULL;

    /* All done! */
}

