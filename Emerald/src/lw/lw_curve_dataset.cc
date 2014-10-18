/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "lw/lw_curve_dataset.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
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

typedef struct
{
    system_hash64map object_to_item_vector_map; /* "object name" hashed_ansi_string -> vector of _lw_curve_dataset_item* items */

    REFCOUNT_INSERT_VARIABLES
} _lw_curve_dataset;


/** Internal variables */ 

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(lw_curve_dataset, lw_curve_dataset, _lw_curve_dataset);


/** TODO */
PRIVATE void _lw_curve_dataset_release(__in __notnull __deallocate(mem) void* ptr)
{
    system_resizable_vector curve_vector = NULL;
    _lw_curve_dataset*      data_ptr     = (_lw_curve_dataset*) ptr;
    system_hash64           vector_hash  = 0;

    if (data_ptr->object_to_item_vector_map != NULL)
    {
        while (system_hash64map_get_element_at(data_ptr->object_to_item_vector_map,
                                               0,
                                               &curve_vector,
                                               &vector_hash) )
        {
            _lw_curve_dataset_item* item_ptr = NULL;

            while (system_resizable_vector_pop(curve_vector, &item_ptr) )
            {
                delete item_ptr;

                item_ptr = NULL;
            }

            system_resizable_vector_release(curve_vector);
            curve_vector = NULL;
        } /* while (the object hash-map contains vectors) */

        system_hash64map_release(data_ptr->object_to_item_vector_map);
        data_ptr->object_to_item_vector_map = NULL;
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API void lw_curve_dataset_add_curve(__in __notnull lw_curve_dataset          dataset,
                                                   __in __notnull system_hashed_ansi_string object_name,
                                                   __in __notnull system_hashed_ansi_string curve_name,
                                                   __in __notnull curve_container           curve)
{
    _lw_curve_dataset*      dataset_ptr = (_lw_curve_dataset*) dataset;
    system_resizable_vector item_vector = NULL;

    if (!system_hash64map_get(dataset_ptr->object_to_item_vector_map,
                              system_hashed_ansi_string_get_hash(object_name),
                             &item_vector) )
    {
        item_vector = system_resizable_vector_create(4, /* capacity */
                                                     sizeof(_lw_curve_dataset_item*) );

        ASSERT_DEBUG_SYNC(item_vector != NULL,
                          "Could not create a resizable vector");

        system_hash64map_insert(dataset_ptr->object_to_item_vector_map,
                                system_hashed_ansi_string_get_hash(object_name),
                                item_vector,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */
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
    _lw_curve_dataset* result_ptr = (_lw_curve_dataset*) result;
    uint32_t           n_objects  = 0;

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
                system_file_serializer_read_hashed_ansi_string(serializer,
                                                              &object_name);

                ASSERT_DEBUG_SYNC(object_name != NULL,
                                  "Object name remains undefined");
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

            /* Write the object name, if this is the first item */
            if (n_item == 0)
            {
                system_file_serializer_write_hashed_ansi_string(serializer,
                                                                item_ptr->object_name);
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

