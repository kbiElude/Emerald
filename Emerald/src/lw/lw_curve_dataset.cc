/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "lw/lw_curve_dataset.h"
#include "scene/scene.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/** Internal type definition */
typedef struct _lw_curve_dataset_item
{
    curve_container           curve;
    system_hashed_ansi_string name;
    system_hashed_ansi_string object_name;

    _lw_curve_dataset_item()
    {
        curve       = NULL;
        name        = NULL;
        object_name = NULL;
    }

    ~_lw_curve_dataset_item()
    {
        if (curve != NULL)
        {
            curve_container_release(curve);

            curve = NULL;
        }
    }
} _lw_curve_dataset_item;

typedef struct _lw_curve_dataset_object
{
    lw_curve_dataset_object_type object_type;

    _lw_curve_dataset_object()
    {
        object_type = LW_CURVE_DATASET_OBJECT_TYPE_UNDEFINED;
    }
} _lw_curve_dataset_object;

typedef enum
{
    LW_CURVE_TYPE_POSITION_X,
    LW_CURVE_TYPE_POSITION_Y,
    LW_CURVE_TYPE_POSITION_Z,
    LW_CURVE_TYPE_ROTATION_B,
    LW_CURVE_TYPE_ROTATION_H,
    LW_CURVE_TYPE_ROTATION_P,
    LW_CURVE_TYPE_SCALE_X,
    LW_CURVE_TYPE_SCALE_Y,
    LW_CURVE_TYPE_SCALE_Z,

    LW_CURVE_TYPE_UNDEFINED
} _lw_curve_type;


typedef struct
{
    system_hash64map object_to_item_vector_map; /* "object name" hashed_ansi_string -> vector of _lw_curve_dataset_item* items */
    system_hash64map object_to_properties_map;  /* "object name" hashed ansi string -> _lw_curve_dataset_object* */

    REFCOUNT_INSERT_VARIABLES
} _lw_curve_dataset;


/** Internal variables */ 

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(lw_curve_dataset, lw_curve_dataset, _lw_curve_dataset);


/** TODO */
PRIVATE void _lw_curve_dataset_apply_to_camera(__in           _lw_curve_type  curve_type,
                                               __in __notnull curve_container new_curve,
                                               __in __notnull scene_camera    camera)
{
}

/** TODO */
PRIVATE void _lw_curve_dataset_apply_to_light(__in            _lw_curve_type  curve_type,
                                               __in __notnull curve_container new_curve,
                                               __in __notnull scene_light     light)
{
    // TODO
}

/** TODO */
PRIVATE void _lw_curve_dataset_apply_to_mesh(__in           _lw_curve_type  curve_type,
                                             __in __notnull curve_container new_curve,
                                             __in __notnull scene_mesh      mesh)
{
    // TODO
}

/** TODO */
PRIVATE void _lw_curve_dataset_release(__in __notnull __deallocate(mem) void* ptr)
{
    _lw_curve_dataset* dataset_ptr = (_lw_curve_dataset*) ptr;

    if (dataset_ptr->object_to_item_vector_map != NULL)
    {
        const uint32_t n_entries = system_hash64map_get_amount_of_elements(dataset_ptr->object_to_item_vector_map);

        for (uint32_t n_entry = 0;
                      n_entry < n_entries;
                    ++n_entry)
        {
            system_resizable_vector curve_vector = NULL;
            _lw_curve_dataset_item* item_ptr     = NULL;
            system_hash64           vector_hash  = 0;

            if (!system_hash64map_get_element_at(dataset_ptr->object_to_item_vector_map,
                                                 n_entry,
                                                &curve_vector,
                                                &vector_hash) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Hash map getter failed.");
            }
            else
            {
                while (system_resizable_vector_pop(curve_vector, &item_ptr) )
                {
                    delete item_ptr;

                    item_ptr = NULL;
                }

                system_resizable_vector_release(curve_vector);
                curve_vector = NULL;
            }
        } /* while (the object hash-map contains vectors) */

        system_hash64map_release(dataset_ptr->object_to_item_vector_map);
        dataset_ptr->object_to_item_vector_map = NULL;
    }

    if (dataset_ptr->object_to_properties_map != NULL)
    {
        const uint32_t n_entries = system_hash64map_get_amount_of_elements(dataset_ptr->object_to_properties_map);

        for (uint32_t n_entry = 0;
                      n_entry < n_entries;
                    ++n_entry)
        {
            _lw_curve_dataset_object* item_ptr = NULL;

            if (!system_hash64map_get_element_at(dataset_ptr->object_to_properties_map,
                                                 n_entry,
                                                &item_ptr,
                                                 NULL) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Hash map getter failed.");
            }
            else
            {
                delete item_ptr;
                item_ptr = NULL;
            }
        } /* for (all entries) */

        system_hash64map_release(dataset_ptr->object_to_properties_map);
        dataset_ptr->object_to_properties_map = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API void lw_curve_dataset_add_curve(__in __notnull lw_curve_dataset             dataset,
                                                   __in __notnull system_hashed_ansi_string    object_name,
                                                   __in           lw_curve_dataset_object_type object_type,
                                                   __in __notnull system_hashed_ansi_string    curve_name,
                                                   __in __notnull curve_container              curve)
{
    _lw_curve_dataset*      dataset_ptr      = (_lw_curve_dataset*) dataset;
    system_resizable_vector item_vector      = NULL;
    const system_hash64     object_name_hash = system_hashed_ansi_string_get_hash(object_name);

    if (!system_hash64map_get(dataset_ptr->object_to_item_vector_map,
                              system_hashed_ansi_string_get_hash(object_name),
                             &item_vector) )
    {
        /* Create a vector entry */
        item_vector = system_resizable_vector_create(4, /* capacity */
                                                     sizeof(_lw_curve_dataset_item*) );

        ASSERT_DEBUG_SYNC(item_vector != NULL,
                          "Could not create a resizable vector");

        system_hash64map_insert(dataset_ptr->object_to_item_vector_map,
                                object_name_hash,
                                item_vector,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */

        /* Spawn a new object properties descriptor */
        _lw_curve_dataset_object* object_ptr = new (std::nothrow) _lw_curve_dataset_object;

        ASSERT_DEBUG_SYNC(object_ptr != NULL,
                          "Out of memory");
        if (object_ptr == NULL)
        {
            goto end;
        }

        object_ptr->object_type = object_type;

        /* Store it */
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(dataset_ptr->object_to_properties_map,
                                                     object_name_hash),
                          "The same name is likely used for objects of different types."
                          " This is unsupported at the moment.");

        system_hash64map_insert(dataset_ptr->object_to_properties_map,
                                object_name_hash,
                                object_ptr,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */
    }
    else
    {
        _lw_curve_dataset_object* object_ptr = NULL;

        ASSERT_DEBUG_SYNC(system_hash64map_contains(dataset_ptr->object_to_properties_map,
                                                    object_name_hash),
                          "Object has not been recognized.");

        system_hash64map_get(dataset_ptr->object_to_properties_map,
                             object_name_hash,
                            &object_ptr);

        ASSERT_DEBUG_SYNC(object_ptr->object_type == object_type,
                          "Object types do not match");
    }

    /* Spawn a new descriptor */
    _lw_curve_dataset_item* item_ptr = new (std::nothrow) _lw_curve_dataset_item;

    ASSERT_ALWAYS_SYNC(item_ptr != NULL, "Out of memory");
    if (item_ptr == NULL)
    {
        goto end;
    }

    item_ptr->curve       = curve;
    item_ptr->name        = curve_name;
    item_ptr->object_name = object_name;

    curve_container_retain(curve);

    /* Add the new item */
    system_resizable_vector_push(item_vector, item_ptr);

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void lw_curve_dataset_apply_to_scene(__in __notnull lw_curve_dataset dataset,
                                                        __in           scene            scene)
{
    _lw_curve_dataset* dataset_ptr = (_lw_curve_dataset*) dataset;
    const uint32_t     n_objects   = system_hash64map_get_amount_of_elements(dataset_ptr->object_to_item_vector_map);

    ASSERT_DEBUG_SYNC(n_objects == system_hash64map_get_amount_of_elements(dataset_ptr->object_to_properties_map),
                      "Map corruption detected");

    /* Iterate over all objects we have curves for. */
    for (uint32_t n_object = 0;
                  n_object < n_objects;
                ++n_object)
    {
        _lw_curve_dataset_item*   item_ptr   = NULL;
        system_hash64             item_hash  = -1;
        _lw_curve_dataset_object* object_ptr = NULL;

        /* Retrieve descriptors */
        if (!system_hash64map_get_element_at(dataset_ptr->object_to_item_vector_map,
                                             n_object,
                                            &item_ptr,
                                            &item_hash) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve item descriptor for object at index [%d]",
                              n_object);

            goto end;
        }

        if (!system_hash64map_get(dataset_ptr->object_to_properties_map,
                                  item_hash,
                                 &object_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve object descriptor.");

            goto end;
        }

        /* Match the curve name to one of the predefined curve types */
        static const system_hashed_ansi_string position_x_curve_name = system_hashed_ansi_string_create("Position.X");
        static const system_hashed_ansi_string position_y_curve_name = system_hashed_ansi_string_create("Position.Y");
        static const system_hashed_ansi_string position_z_curve_name = system_hashed_ansi_string_create("Position.Z");
        static const system_hashed_ansi_string rotation_b_curve_name = system_hashed_ansi_string_create("Rotation.B");
        static const system_hashed_ansi_string rotation_h_curve_name = system_hashed_ansi_string_create("Rotation.H");
        static const system_hashed_ansi_string rotation_p_curve_name = system_hashed_ansi_string_create("Rotation.P");
        static const system_hashed_ansi_string scale_x_curve_name    = system_hashed_ansi_string_create("Scale.X");
        static const system_hashed_ansi_string scale_y_curve_name    = system_hashed_ansi_string_create("Scale.Y");
        static const system_hashed_ansi_string scale_z_curve_name    = system_hashed_ansi_string_create("Scale.Z");

        system_hash64  curve_name_hash = system_hashed_ansi_string_get_hash(item_ptr->name);
        _lw_curve_type curve_type      = LW_CURVE_TYPE_UNDEFINED;

        if (curve_name_hash == system_hashed_ansi_string_get_hash(position_x_curve_name) ) curve_type = LW_CURVE_TYPE_POSITION_X;else
        if (curve_name_hash == system_hashed_ansi_string_get_hash(position_y_curve_name) ) curve_type = LW_CURVE_TYPE_POSITION_Y;else
        if (curve_name_hash == system_hashed_ansi_string_get_hash(position_z_curve_name) ) curve_type = LW_CURVE_TYPE_POSITION_Z;else
        if (curve_name_hash == system_hashed_ansi_string_get_hash(rotation_b_curve_name) ) curve_type = LW_CURVE_TYPE_ROTATION_B;else
        if (curve_name_hash == system_hashed_ansi_string_get_hash(rotation_h_curve_name) ) curve_type = LW_CURVE_TYPE_ROTATION_H;else
        if (curve_name_hash == system_hashed_ansi_string_get_hash(rotation_p_curve_name) ) curve_type = LW_CURVE_TYPE_ROTATION_P;else
        if (curve_name_hash == system_hashed_ansi_string_get_hash(scale_x_curve_name) )    curve_type = LW_CURVE_TYPE_SCALE_X;   else
        if (curve_name_hash == system_hashed_ansi_string_get_hash(scale_y_curve_name) )    curve_type = LW_CURVE_TYPE_SCALE_Y;   else
        if (curve_name_hash == system_hashed_ansi_string_get_hash(scale_z_curve_name) )    curve_type = LW_CURVE_TYPE_SCALE_Z;   else
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unrecognized curve type");
        }

        /* Let the object-specific handler take care of the request */
        switch (object_ptr->object_type)
        {
            case LW_CURVE_DATASET_OBJECT_TYPE_CAMERA:
            {
                scene_camera found_camera = scene_get_camera_by_name(scene,
                                                                     item_ptr->object_name);

                ASSERT_ALWAYS_SYNC(found_camera != NULL,
                                   "Could not find a camera named [%s]",
                                   system_hashed_ansi_string_get_buffer(item_ptr->object_name) );

                if (found_camera != NULL)
                {
                    _lw_curve_dataset_apply_to_camera(curve_type,
                                                      item_ptr->curve,
                                                      found_camera);
                }

                break;
            }

            case LW_CURVE_DATASET_OBJECT_TYPE_LIGHT:
            {
                scene_light found_light = scene_get_light_by_name(scene,
                                                                  item_ptr->object_name);

                ASSERT_ALWAYS_SYNC(found_light != NULL,
                                   "Could not find a light named [%s]",
                                   system_hashed_ansi_string_get_buffer(item_ptr->object_name) );

                if (found_light != NULL)
                {
                    _lw_curve_dataset_apply_to_light(curve_type,
                                                     item_ptr->curve,
                                                     found_light);
                }

                break;
            }

            case LW_CURVE_DATASET_OBJECT_TYPE_MESH:
            {
                scene_mesh found_mesh = scene_get_mesh_instance_by_name(scene,
                                                                        item_ptr->object_name);

                ASSERT_ALWAYS_SYNC(found_mesh != NULL,
                                   "Could not find a mesh named [%s]",
                                   system_hashed_ansi_string_get_buffer(item_ptr->object_name) );

                if (found_mesh != NULL)
                {
                    _lw_curve_dataset_apply_to_mesh(curve_type,
                                                    item_ptr->curve,
                                                    found_mesh);
                }

                break;
            }

            default:
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Unrecognized LW curve data-set object type");
            }
        } /* switch (object_ptr->object_type) */
    } /* for (all objects) */

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API lw_curve_dataset lw_curve_dataset_create(__in __notnull system_hashed_ansi_string name)
{
    /* Instantiate the object */
    _lw_curve_dataset* result_instance = new (std::nothrow) _lw_curve_dataset;

    ASSERT_DEBUG_SYNC(result_instance != NULL, "Out of memory");
    if (result_instance == NULL)
    {
        LOG_ERROR("Out of memory");

        goto end;
    }

    /* Set up the fields */
    memset(result_instance,
           0,
           sizeof(*result_instance) );

    result_instance->object_to_item_vector_map = system_hash64map_create(sizeof(_lw_curve_dataset_item*) );
    result_instance->object_to_properties_map  = system_hash64map_create(sizeof(_lw_curve_dataset_object*) );

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_instance,
                                                   _lw_curve_dataset_release,
                                                   OBJECT_TYPE_LW_CURVE_DATASET,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Lightwave curve data-sets\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (lw_curve_dataset) result_instance;

end:
    if (result_instance != NULL)
    {
        _lw_curve_dataset_release(result_instance);

        delete result_instance;
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API lw_curve_dataset lw_curve_dataset_load(__in __notnull system_hashed_ansi_string name,
                                                          __in __notnull system_file_serializer    serializer)
{
    lw_curve_dataset result = lw_curve_dataset_create(name);

    ASSERT_DEBUG_SYNC(result != NULL,
                      "lw_curve_dataset_create() failed.");
    if (result == NULL)
    {
        goto end;
    }

    /* Load the objects */
    _lw_curve_dataset* result_ptr   = (_lw_curve_dataset*) result;
    uint32_t           n_objects    = 0;
    system_hash64map   object_props = system_hash64map_create(sizeof(_lw_curve_dataset_object*) );

    system_file_serializer_read(serializer,
                                sizeof(n_objects),
                               &n_objects);

    for (uint32_t n_object = 0;
                  n_object < n_objects;
                ++n_object)
    {
        uint32_t                  n_items      = 0;
        system_resizable_vector   object_items = system_resizable_vector_create(4, /* capacity */
                                                                                sizeof(_lw_curve_dataset_item*) );
        system_hashed_ansi_string object_name  = NULL; /* this gets set when loading data of the first defined item */

        system_file_serializer_read(serializer,
                                    sizeof(n_items),
                                   &n_items);

        ASSERT_DEBUG_SYNC(n_items != 0,
                          "Zero items defined for a LW curve data set object");

        for (uint32_t n_item = 0;
                      n_item < n_items;
                    ++n_item)
        {
            if (n_item == 0)
            {
                lw_curve_dataset_object_type object_type = LW_CURVE_DATASET_OBJECT_TYPE_UNDEFINED;

                system_file_serializer_read_hashed_ansi_string(serializer,
                                                              &object_name);
                system_file_serializer_read                   (serializer,
                                                               sizeof(object_type),
                                                              &object_type);

                ASSERT_DEBUG_SYNC(object_name != NULL,
                                  "Object name remains undefined");

                /* Create a prop descriptor */
                system_hash64             object_name_hash = system_hashed_ansi_string_get_hash(object_name);
                _lw_curve_dataset_object* object_ptr       = NULL;

                ASSERT_DEBUG_SYNC(!system_hash64map_contains(object_props,
                                                             object_name_hash),
                                  "Props hash-map already stores properties for the object");

                object_ptr = new (std::nothrow) _lw_curve_dataset_object;

                ASSERT_ALWAYS_SYNC(object_ptr != NULL,
                                   "Out of memory");
                if (object_ptr == NULL)
                {
                    goto end_error;
                }

                object_ptr->object_type = object_type;

                /* Store it */
                system_hash64map_insert(object_props,
                                        object_name_hash,
                                        object_ptr,
                                        NULL,  /* on_remove_callback */
                                        NULL); /* on_remove_callback_user_arg */
            }

            /* Read the item data */
            curve_container           item_curve = NULL;
            system_hashed_ansi_string item_name  = NULL;

            system_file_serializer_read_hashed_ansi_string(serializer,
                                                          &item_name);
            system_file_serializer_read_curve_container   (serializer,
                                                          &item_curve);

            /* Spawn the descriptor */
            _lw_curve_dataset_item* item_ptr = new (std::nothrow) _lw_curve_dataset_item;

            ASSERT_ALWAYS_SYNC(item_ptr != NULL,
                               "Out of memory");
            if (item_ptr == NULL)
            {
                goto end_error;
            }

            item_ptr->curve       = item_curve;
            item_ptr->name        = item_name;
            item_ptr->object_name = object_name;

            /* Store the item */
            system_resizable_vector_push(object_items, item_ptr);
        } /* for (all items) */

        /* Store the vector */
        const system_hash64 object_name_hash = system_hashed_ansi_string_get_hash(object_name);

        system_hash64map_insert(result_ptr->object_to_item_vector_map,
                                object_name_hash,
                                object_items,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */
    } /* for (all objects) */

    result_ptr->object_to_properties_map = object_props;

    goto end;

end_error:
    if (result != NULL)
    {
        lw_curve_dataset_release(result);

        result = NULL;
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void lw_curve_dataset_save(__in __notnull lw_curve_dataset       dataset,
                                              __in __notnull system_file_serializer serializer)
{
    _lw_curve_dataset* dataset_ptr = (_lw_curve_dataset*) dataset;

    /* Store the map entries */
    const uint32_t n_objects = system_hash64map_get_amount_of_elements(dataset_ptr->object_to_item_vector_map);

    system_file_serializer_write(serializer,
                                 sizeof(n_objects),
                                &n_objects);

    for (uint32_t n_object = 0;
                  n_object < n_objects;
                ++n_object)
    {
        system_resizable_vector items   = NULL;
        uint32_t                n_items = 0;

        if (!system_hash64map_get_element_at(dataset_ptr->object_to_item_vector_map,
                                             n_object,
                                            &items,
                                             NULL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve object vector");

            goto end;
        }

        n_items = system_resizable_vector_get_amount_of_elements(items);

        system_file_serializer_write(serializer,
                                     sizeof(uint32_t),
                                    &n_items);

        for (uint32_t n_item = 0;
                      n_item < n_items;
                    ++n_item)
        {
            _lw_curve_dataset_item* item_ptr = NULL;

            if (!system_resizable_vector_get_element_at(items,
                                                        n_item,
                                                       &item_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve item descriptor");

                goto end;
            }

            /* Write the object name & its type, if this is the first item */
            if (n_item == 0)
            {
                _lw_curve_dataset_object* object_ptr = NULL;

                system_hash64map_get(dataset_ptr->object_to_properties_map,
                                     system_hashed_ansi_string_get_hash(item_ptr->object_name),
                                    &object_ptr);

                ASSERT_DEBUG_SYNC(object_ptr != NULL,
                                  "Could not find object descriptor");

                system_file_serializer_write_hashed_ansi_string(serializer,
                                                                item_ptr->object_name);
                system_file_serializer_write                   (serializer,
                                                                sizeof(object_ptr->object_type),
                                                               &object_ptr->object_type);
            }

            /* Write the item data */
            system_file_serializer_write_hashed_ansi_string(serializer,
                                                            item_ptr->name);
            system_file_serializer_write_curve_container   (serializer,
                                                            item_ptr->curve);
        } /* for (all items) */
    } /* for (all objects) */

end:
    ;
}

