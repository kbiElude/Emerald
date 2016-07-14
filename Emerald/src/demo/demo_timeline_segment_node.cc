/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"

#include "demo/demo_timeline_segment_node.h"
#include "ral/ral_context.h"
#include "ral/ral_texture.h"
#include "ral/ral_types.h"
#include "ral/ral_utils.h"
#include "system/system_callback_manager.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

typedef struct _demo_timeline_segment_node_texture_io
{
    ral_format                          format;
    uint32_t                            id;
    bool                                is_attachment_required;
    system_hashed_ansi_string           name;
    bool                                needs_full_mipmap_chain;
    unsigned int                        n_components;
    unsigned int                        n_layers;
    unsigned int                        n_samples;
    struct _demo_timeline_segment_node* parent_node_ptr;
    ral_texture_type                    type;

    ral_texture bound_texture; /* Owned by output nodes only! */

    explicit _demo_timeline_segment_node_texture_io(_demo_timeline_segment_node* in_parent_node_ptr,
                                                    uint32_t                     in_id,
                                                    system_hashed_ansi_string    in_name,
                                                    ral_format                   in_format,
                                                    unsigned int                 in_n_layers,
                                                    unsigned int                 in_texture_n_components,
                                                    unsigned int                 in_n_samples,
                                                    ral_texture_type             in_type,
                                                    bool                         in_is_attachment_required)
    {
        bound_texture          = nullptr;
        format                 = in_format;
        id                     = in_id;
        is_attachment_required = in_is_attachment_required;
        name                   = in_name;
        n_components           = in_texture_n_components;
        n_layers               = in_n_layers;
        n_samples              = in_n_samples;
        parent_node_ptr        = in_parent_node_ptr;
        type                   = in_type;
    }
} _demo_timeline_segment_node_texture_io;

typedef struct _demo_timeline_segment_node
{
    system_callback_manager                             callback_manager;
    ral_context                                         context;
    demo_timeline_segment_node_id                       id;
    PFNSEGMENTNODEDEINITCALLBACKPROC                    pfn_deinit_proc;
    PFNSEGMENTNODEGETPROPERTYCALLBACKPROC               pfn_get_property_proc;
    PFNSEGMENTNODEGETTEXTUREMEMORYALLOCATIONDETAILSPROC pfn_get_texture_memory_allocation_details_proc;
    PFNSEGMENTNODEINITCALLBACKPROC                      pfn_init_proc;
    PFNSEGMENTNODERENDERCALLBACKPROC                    pfn_render_proc;
    PFNSEGMENTNODESETPROPERTYCALLBACKPROC               pfn_set_property_proc;
    PFNSEGMENTNODESETTEXTUREMEMORYALLOCATIONPROC        pfn_set_texture_memory_allocation_proc;
    demo_timeline_segment_type                          segment_type;
    demo_timeline_segment_node_type                     type;

    system_resizable_vector   texture_inputs;   /* Own the stored _demo_timeline_segment_node_texture_io instances.
                                                 * NOT sorted relative to the ID; use texture_input_id_to_texture_input_descriptor map for this.
                                                 */
    system_resizable_vector   texture_outputs;  /* Own the stored _demo_timeline_segment_node_texture_io instances.
                                                 * NOT sorted relative to the ID; use texture_output_id_to_texture_output_descriptor map for this.
                                                 */

    system_hash64map          texture_input_id_to_texture_io_descriptor_map;
    system_hash64map          texture_output_id_to_texture_io_descriptor_map;

    _demo_timeline_segment_node(ral_context                                         in_context,
                                demo_timeline_segment_type                          in_segment_type,
                                demo_timeline_segment_node_id                       in_id,
                                PFNSEGMENTNODEDEINITCALLBACKPROC                    in_pfn_deinit_proc,
                                PFNSEGMENTNODEGETPROPERTYCALLBACKPROC               in_pfn_get_property_proc,
                                PFNSEGMENTNODEGETTEXTUREMEMORYALLOCATIONDETAILSPROC in_pfn_get_texture_memory_allocation_details_proc,
                                PFNSEGMENTNODEINITCALLBACKPROC                      in_pfn_init_proc,
                                PFNSEGMENTNODERENDERCALLBACKPROC                    in_pfn_render_proc,
                                PFNSEGMENTNODESETPROPERTYCALLBACKPROC               in_pfn_set_property_proc,
                                PFNSEGMENTNODESETTEXTUREMEMORYALLOCATIONPROC        in_pfn_set_texture_memory_allocation_proc,
                                demo_timeline_segment_node_type                     in_type)
    {
        callback_manager                               = system_callback_manager_create( (_callback_id) DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_COUNT);
        context                                        = in_context;
        id                                             = in_id;
        pfn_deinit_proc                                = in_pfn_deinit_proc;
        pfn_get_property_proc                          = in_pfn_get_property_proc;
        pfn_get_texture_memory_allocation_details_proc = in_pfn_get_texture_memory_allocation_details_proc;
        pfn_init_proc                                  = in_pfn_init_proc;
        pfn_render_proc                                = in_pfn_render_proc;
        pfn_set_property_proc                          = in_pfn_set_property_proc;
        pfn_set_texture_memory_allocation_proc         = in_pfn_set_texture_memory_allocation_proc;
        segment_type                                   = in_segment_type;
        type                                           = in_type;

        texture_inputs                                 = system_resizable_vector_create(4 /* capacity */);
        texture_outputs                                = system_resizable_vector_create(4 /* capacity */);
        texture_input_id_to_texture_io_descriptor_map  = system_hash64map_create(sizeof(_demo_timeline_segment_node_texture_io*)  );
        texture_output_id_to_texture_io_descriptor_map = system_hash64map_create(sizeof(_demo_timeline_segment_node_texture_io*) );

        ASSERT_ALWAYS_SYNC(callback_manager != nullptr,
                           "Could not spawn the callback manager");
        ASSERT_ALWAYS_SYNC(texture_inputs != nullptr,
                           "Could not spawn the video segment texture inputs vector");
        ASSERT_ALWAYS_SYNC(texture_input_id_to_texture_io_descriptor_map != nullptr,
                           "Could not spawn the texture input ID->texture IO descriptor hash map");
        ASSERT_ALWAYS_SYNC(texture_outputs != nullptr,
                           "Could not spawn the video segment texture outputs vector");
        ASSERT_ALWAYS_SYNC(texture_output_id_to_texture_io_descriptor_map != nullptr,
                           "Could not spawn the texture output ID->texture IO descriptor hash map");
    }

    ~_demo_timeline_segment_node()
    {
        /* Release the owned objects */
        if (texture_input_id_to_texture_io_descriptor_map != nullptr)
        {
            system_hash64map_release(texture_input_id_to_texture_io_descriptor_map);

            texture_input_id_to_texture_io_descriptor_map = nullptr;
        }

        if (texture_inputs != nullptr)
        {
            _demo_timeline_segment_node_texture_io* texture_io_ptr = nullptr;

            while (system_resizable_vector_pop(texture_inputs,
                                              &texture_io_ptr) )
            {
                delete texture_io_ptr;
                texture_io_ptr = nullptr;
            }

            system_resizable_vector_release(texture_inputs);
            texture_inputs = nullptr;
        }

        if (texture_output_id_to_texture_io_descriptor_map != nullptr)
        {
            system_hash64map_release(texture_output_id_to_texture_io_descriptor_map);

            texture_output_id_to_texture_io_descriptor_map = nullptr;
        }

        if (texture_outputs != nullptr)
        {
            _demo_timeline_segment_node_texture_io* texture_output_ptr = nullptr;

            while (system_resizable_vector_pop(texture_outputs,
                                              &texture_output_ptr) )
            {
                delete texture_output_ptr;

                texture_output_ptr = nullptr;
            }

            system_resizable_vector_release(texture_outputs);
            texture_outputs = nullptr;
        }

        if (callback_manager != nullptr)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = nullptr;
        }
    }
} _demo_timeline_segment_node;


/* Forward declarations */
PRIVATE void _demo_timeline_segment_node_get_input_output_ids       (demo_timeline_segment_node           node,
                                                                     bool                                 should_return_input_data,
                                                                     uint32_t                             n_ids,
                                                                     uint32_t*                            out_opt_n_ids_ptr,
                                                                     demo_timeline_segment_node_input_id* out_opt_ids_ptr);
PRIVATE void _demo_timeline_segment_node_subscribe_for_notifications(_demo_timeline_segment_node*         node_ptr,
                                                                     bool                                 should_subscribe);


/** TODO */
PRIVATE void _demo_timeline_segment_node_get_input_output_ids(demo_timeline_segment_node           node,
                                                              bool                                 should_return_input_data,
                                                              uint32_t                             n_ids,
                                                              uint32_t*                            out_opt_n_ids_ptr,
                                                              demo_timeline_segment_node_input_id* out_opt_ids_ptr)
{
    _demo_timeline_segment_node* node_ptr          = reinterpret_cast<_demo_timeline_segment_node*>(node);
    uint32_t                     n_vector_elements = 0;
    system_resizable_vector      data_vector       = (should_return_input_data) ? node_ptr->texture_inputs
                                                                                : node_ptr->texture_outputs;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node != nullptr,
                      "Input node is NULL");

    /* Determine the number of inputs the specified video segment has to offer */
    system_resizable_vector_get_property(data_vector,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_vector_elements);

    if (out_opt_n_ids_ptr != nullptr)
    {
        *out_opt_n_ids_ptr = n_vector_elements;
    }

    if (n_ids > n_vector_elements)
    {
        n_ids = n_vector_elements;
    }

    /* Store up to n_ids IDs, if possible */
    if (n_ids != 0         &&
        n_ids <= n_vector_elements)
    {
        for (uint32_t n_id = 0;
                      n_id < n_ids;
                    ++n_id)
        {
            void* io = nullptr;

            if (!system_resizable_vector_get_element_at(data_vector,
                                                        n_id,
                                                        &io) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve IO descriptor at index [%d]",
                                  n_id);

                continue;
            }

            out_opt_ids_ptr[n_id] = (reinterpret_cast<_demo_timeline_segment_node_texture_io*>(io) )->id;
        }
    }
}

/** TODO */
PRIVATE void _demo_timeline_segment_on_textures_deleted(const void* callback_arg,
                                                              void* node)
{
    const ral_context_callback_objects_deleted_callback_arg* callback_arg_ptr = reinterpret_cast<const ral_context_callback_objects_deleted_callback_arg*>(callback_arg);
    _demo_timeline_segment_node*                             node_ptr         = reinterpret_cast<_demo_timeline_segment_node*>                            (node);

    ASSERT_DEBUG_SYNC(callback_arg_ptr->object_type == RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                      "Invalid callback object type");

    /* Check if any of the inputs & outputs which use texture interface refer to the deleted texture.
     * If so, detach it */
    for (uint32_t n_io_type = 0;
                  n_io_type < 2; /* input, output */
                ++n_io_type)
    {
        const system_resizable_vector current_io_vector = (n_io_type == 0) ? node_ptr->texture_inputs
                                                                           : node_ptr->texture_outputs;
        uint32_t                      n_texture_ios     = 0;

        system_resizable_vector_get_property(current_io_vector,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_texture_ios);

        if (n_texture_ios == 0)
        {
            continue;
        }

        for (uint32_t n_io = 0;
                      n_io < n_texture_ios;
                    ++n_io)
        {
            _demo_timeline_segment_node_texture_io* current_io_ptr = nullptr;

            if (!system_resizable_vector_get_element_at(current_io_vector,
                                                        n_io,
                                                       &current_io_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve IO descriptor at index [%d]",
                                  n_io);

                continue;
            }

            for (uint32_t n_deleted_object = 0;
                          n_deleted_object < callback_arg_ptr->n_objects;
                        ++n_deleted_object)
            {
                if (current_io_ptr->bound_texture == callback_arg_ptr->deleted_objects[n_deleted_object])
                {
                    system_hashed_ansi_string texture_name = nullptr;

                    ral_texture_get_property(current_io_ptr->bound_texture,
                                             RAL_TEXTURE_PROPERTY_NAME,
                                            &texture_name);

                    /* Got a match! */
                    LOG_INFO("Auto-detached a released texture [%s] from a segment node",
                             system_hashed_ansi_string_get_buffer(texture_name) );

                    current_io_ptr->bound_texture = nullptr;
                }
            }
        }
    }
}

/** TODO */
PRIVATE void _demo_timeline_segment_node_subscribe_for_notifications(_demo_timeline_segment_node* node_ptr,
                                                                     bool                         should_subscribe)
{
    system_callback_manager context_callback_manager = nullptr;

    ral_context_get_property(node_ptr->context,
                             RAL_CONTEXT_PROPERTY_CALLBACK_MANAGER,
                            &context_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_TEXTURES_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                       &_demo_timeline_segment_on_textures_deleted,
                                                        node_ptr);
    }
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_TEXTURES_DELETED,
                                                          &_demo_timeline_segment_on_textures_deleted,
                                                           node_ptr);
    }
}


/** Please see header for specification */
PUBLIC bool demo_timeline_segment_node_add_texture_input(demo_timeline_segment_node           node,
                                                         const demo_texture_io_declaration*   new_input_properties_ptr,
                                                         demo_timeline_segment_node_input_id* out_opt_input_id_ptr)
{
    _demo_timeline_segment_node* node_ptr = (_demo_timeline_segment_node*) node;
    bool                         result   = false;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node != nullptr,
                      "Input node is NULL");

    ASSERT_DEBUG_SYNC(new_input_properties_ptr->texture_format != RAL_FORMAT_UNKNOWN,
                      "Unknown texture format is not allowed");
    ASSERT_DEBUG_SYNC(new_input_properties_ptr->texture_type != RAL_TEXTURE_TYPE_UNKNOWN,
                      "Unknown texture type is not allowed");
    ASSERT_DEBUG_SYNC(new_input_properties_ptr->texture_n_components >= 1 && new_input_properties_ptr->texture_n_components <= 4,
                      "Number of components expected at a texture input must be a value between 1 and 4 inclusive.");
    ASSERT_DEBUG_SYNC(new_input_properties_ptr->texture_n_layers >= 1,
                      "Number of texture layers must be at least 1.");
    ASSERT_DEBUG_SYNC(new_input_properties_ptr->texture_n_samples >= 1,
                      "Number of texture samples must be at least 1");

    ASSERT_DEBUG_SYNC(!(new_input_properties_ptr->texture_n_layers > 1 && !(ral_utils_is_texture_type_cubemap(new_input_properties_ptr->texture_type) ||
                                                                            ral_utils_is_texture_type_layered(new_input_properties_ptr->texture_type) )),
                      "Specified texture type is neither layered nor a cubemap type, but texture_n_layers is [%d]",
                      new_input_properties_ptr->texture_n_layers);

    ASSERT_DEBUG_SYNC(!(new_input_properties_ptr->texture_n_samples > 1 && !ral_utils_is_texture_type_multisample(new_input_properties_ptr->texture_type) ),
                      "Specified texture type is not multisample but texture_n_samples is [%d]",
                      new_input_properties_ptr->texture_n_samples);

    /* Create a new texture output instance */
    uint32_t                                new_input_id       = -1;
    _demo_timeline_segment_node_texture_io* new_texture_io_ptr = nullptr;

    system_resizable_vector_get_property(node_ptr->texture_inputs,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &new_input_id);

     ASSERT_DEBUG_SYNC(!system_hash64map_contains(node_ptr->texture_input_id_to_texture_io_descriptor_map,
                                                  (system_hash64) new_input_id),
                       "A descriptor is already associated to the new texture input's ID!");

    new_texture_io_ptr = new (std::nothrow) _demo_timeline_segment_node_texture_io(node_ptr,
                                                                                   new_input_id,
                                                                                   new_input_properties_ptr->name,
                                                                                   new_input_properties_ptr->texture_format,
                                                                                   new_input_properties_ptr->texture_n_layers,
                                                                                   new_input_properties_ptr->texture_n_components,
                                                                                   new_input_properties_ptr->texture_n_samples,
                                                                                   new_input_properties_ptr->texture_type,
                                                                                   new_input_properties_ptr->is_attachment_required);

    ASSERT_DEBUG_SYNC(new_texture_io_ptr != nullptr,
                      "Out of memory");

    if (new_texture_io_ptr != nullptr)
    {
        /* Store it */
        if (out_opt_input_id_ptr != nullptr)
        {
            *out_opt_input_id_ptr = new_input_id;
        }

        result = true;

        system_resizable_vector_push(node_ptr->texture_inputs,
                                     new_texture_io_ptr);
        system_hash64map_insert     (node_ptr->texture_input_id_to_texture_io_descriptor_map,
                                     (system_hash64) new_input_id,
                                     new_texture_io_ptr,
                                     nullptr,  /* on_remove_callback */
                                     nullptr); /* callback_argument */

        /* Ping the subscribers about the fact a new texture input has arrived */
        system_callback_manager_call_back(node_ptr->callback_manager,
                                          DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_INPUT_ADDED,
                                          (void*) new_input_id);
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_node_add_texture_output(demo_timeline_segment_node            node,
                                                          const demo_texture_io_declaration*    new_output_properties_ptr,
                                                          demo_timeline_segment_node_output_id* out_opt_output_id_ptr)
{
    _demo_timeline_segment_node* node_ptr = (_demo_timeline_segment_node*) node;
    bool                         result   = false;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node != nullptr,
                      "Input node is NULL");

    ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_format != RAL_FORMAT_UNKNOWN,
                      "Unknown texture format is not allowed");
    ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_type != RAL_TEXTURE_TYPE_UNKNOWN,
                      "Unknown texture type is not allowed");
    ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_n_layers >= 1,
                      "Output texture must not use 0 layers.");
    ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_n_samples >= 1,
                      "Output texture must not use 0 samples.");
    ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_n_components >= 1 && new_output_properties_ptr->texture_n_components <= 4,
                      "Output texture must be use at least 1, but no more than 4 components. ");

    /* Create a new texture output instance */
    uint32_t                                new_output_id          = -1;
    _demo_timeline_segment_node_texture_io* new_texture_output_ptr = nullptr;

    system_resizable_vector_get_property(node_ptr->texture_outputs,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &new_output_id);

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(node_ptr->texture_output_id_to_texture_io_descriptor_map,
                                                 (system_hash64) new_output_id),
                      "A descriptor is already associated to the new texture output's ID!");

    new_texture_output_ptr = new (std::nothrow) _demo_timeline_segment_node_texture_io(node_ptr,
                                                                                       new_output_id,
                                                                                       new_output_properties_ptr->name,
                                                                                       new_output_properties_ptr->texture_format,
                                                                                       new_output_properties_ptr->texture_n_layers,
                                                                                       new_output_properties_ptr->texture_n_components,
                                                                                       new_output_properties_ptr->texture_n_samples,
                                                                                       new_output_properties_ptr->texture_type,
                                                                                       false); /* is_attachment_required */

    ASSERT_DEBUG_SYNC(new_texture_output_ptr != nullptr,
                      "Out of memory");

    if (new_texture_output_ptr != nullptr)
    {
        /* Store it */
        if (out_opt_output_id_ptr != nullptr)
        {
            *out_opt_output_id_ptr = new_output_id;
        }

        result = true;

        system_hash64map_insert     (node_ptr->texture_output_id_to_texture_io_descriptor_map,
                                     (system_hash64) new_output_id,
                                     new_texture_output_ptr,
                                     nullptr,  /* on_remove_callback */
                                     nullptr); /* callback_argument  */

        system_resizable_vector_push(node_ptr->texture_outputs,
                                     new_texture_output_ptr);

        /* Ping the subscribers about the fact a new texture output has arrived */
        demo_timeline_segment_node_callback_texture_output_added_callback_argument callback_arg;

        callback_arg.node      = node;
        callback_arg.output_id = new_output_id;

        system_callback_manager_call_back(node_ptr->callback_manager,
                                          DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_ADDED,
                                         &callback_arg);
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_node_attach_texture_to_texture_io(demo_timeline_segment_node                 node,
                                                                    bool                                       is_input_id,
                                                                    uint32_t                                   id,
                                                                    const demo_texture_attachment_declaration* texture_attachment_ptr)
{
    _demo_timeline_segment_node*            node_ptr       = (_demo_timeline_segment_node*) node;
    bool                                    result         = false;
    _demo_timeline_segment_node_texture_io* texture_io_ptr = nullptr;

    ASSERT_DEBUG_SYNC(node_ptr != nullptr,
                      "Input node is NULL");

    system_hash64map io_map = (is_input_id) ? node_ptr->texture_input_id_to_texture_io_descriptor_map
                                            : node_ptr->texture_output_id_to_texture_io_descriptor_map;

    /* First, retrieve the IO descriptor */
    if (!system_hash64map_get(io_map,
                              id,
                             &texture_io_ptr))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve texture IO with ID [%d]",
                          id);

        goto end;
    }

    if (texture_attachment_ptr != nullptr)
    {
        /* Make sure the texture the caller wants to attach is different from the one that's already bound */
        if (texture_attachment_ptr->texture_ral == texture_io_ptr->bound_texture)
        {
            LOG_ERROR("Redundant demo_timeline_segment_node_attach_texture_to_texture_io() call detected.");

            goto end;
        }

        /* Make sure the texture can actually be bound to the texture input. */
        bool             is_arg_texture_layered      = false;
        ral_format       texture_format              = RAL_FORMAT_UNKNOWN;
        uint32_t         texture_format_n_components = 0;
        uint32_t         texture_n_layers            = 0;
        uint32_t         texture_n_samples           = 0;
        ral_texture_type texture_type                = RAL_TEXTURE_TYPE_UNKNOWN;

        is_arg_texture_layered = ral_utils_is_texture_type_layered(texture_io_ptr->type);

        ral_texture_get_property(texture_attachment_ptr->texture_ral,
                                 RAL_TEXTURE_PROPERTY_FORMAT,
                                &texture_format);
        ral_texture_get_property(texture_attachment_ptr->texture_ral,
                                 RAL_TEXTURE_PROPERTY_N_LAYERS,
                                &texture_n_layers);
        ral_texture_get_property(texture_attachment_ptr->texture_ral,
                                 RAL_TEXTURE_PROPERTY_N_SAMPLES,
                                &texture_n_samples);
        ral_texture_get_property(texture_attachment_ptr->texture_ral,
                                 RAL_TEXTURE_PROPERTY_TYPE,
                                &texture_type);

        ral_utils_get_format_property(texture_format,
                                      RAL_FORMAT_PROPERTY_N_COMPONENTS,
                                     &texture_format_n_components);

        if (texture_format_n_components != texture_io_ptr->n_components)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Number of components of the texture to be attached does not match the number of components required by the specified segment node's IO");

            goto end;
        }

        if (texture_io_ptr->format != texture_format)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Format of the texture to be attached does not match the format required by the specified segment node's IO");

            goto end;
        }

        if (texture_io_ptr->type != texture_type)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Type of the texture to be attached does not match the type required by the specified segment node's IO");

            goto end;
        }

        if (texture_io_ptr->n_samples != texture_n_samples)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Number of samples of the texture to be attached does not match the number of samples expected by the specified segment node's IO");

            goto end;
        }

        if (is_arg_texture_layered)
        {
            if (texture_io_ptr->n_layers < texture_n_layers)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Number of layers defined for the texture to be attached does not match the number of layers expected by the specified segment node's IO");

                goto end;
            }
        }
    }

    /* If there's an existing texture attachment, detach it before continuing */
    if (texture_io_ptr->bound_texture != nullptr)
    {
        demo_timeline_segment_node_callback_texture_detached_callback_argument callback_arg;

        callback_arg.id          = id;
        callback_arg.is_input_id = is_input_id;
        callback_arg.node        = node;
        callback_arg.texture     = texture_io_ptr->bound_texture;

        system_callback_manager_call_back(node_ptr->callback_manager,
                                          DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_DETACHED,
                                         &callback_arg);

        texture_io_ptr->bound_texture = nullptr;
    }

    /* Looks like this texture can be bound to the specified IO! */
    texture_io_ptr->bound_texture = (texture_attachment_ptr != nullptr) ? texture_attachment_ptr->texture_ral : nullptr;

    /* Notify subscribers */
    if (texture_io_ptr->bound_texture != nullptr)
    {
        demo_timeline_segment_node_callback_texture_attached_callback_argument callback_arg;

        callback_arg.id          = id;
        callback_arg.is_input_id = is_input_id;
        callback_arg.node        = node;
        callback_arg.texture     = texture_attachment_ptr->texture_ral;

        system_callback_manager_call_back(texture_io_ptr->parent_node_ptr->callback_manager,
                                          DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_ATTACHED,
                                         &callback_arg);
    }

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for spec */
PUBLIC demo_timeline_segment_node demo_timeline_segment_node_create(ral_context                                         context,
                                                                    demo_timeline_segment_type                          segment_type,
                                                                    demo_timeline_segment_node_type                     node_type,
                                                                    demo_timeline_segment_node_id                       node_id,
                                                                    PFNSEGMENTNODEDEINITCALLBACKPROC                    pfn_deinit_proc,
                                                                    PFNSEGMENTNODEGETPROPERTYCALLBACKPROC               pfn_get_property_proc,
                                                                    PFNSEGMENTNODEGETTEXTUREMEMORYALLOCATIONDETAILSPROC pfn_get_texture_memory_allocation_details_proc,
                                                                    PFNSEGMENTNODEINITCALLBACKPROC                      pfn_init_proc,
                                                                    PFNSEGMENTNODERENDERCALLBACKPROC                    pfn_render_proc,
                                                                    PFNSEGMENTNODESETPROPERTYCALLBACKPROC               pfn_set_property_proc,
                                                                    PFNSEGMENTNODESETTEXTUREMEMORYALLOCATIONPROC        pfn_set_texture_memory_allocation_proc,
                                                                    system_hashed_ansi_string                           node_name)
{
    _demo_timeline_segment_node* new_node_ptr = new (std::nothrow) _demo_timeline_segment_node(context,
                                                                                               segment_type,
                                                                                               node_id,
                                                                                               pfn_deinit_proc,
                                                                                               pfn_get_property_proc,
                                                                                               pfn_get_texture_memory_allocation_details_proc,
                                                                                               pfn_init_proc,
                                                                                               pfn_render_proc,
                                                                                               pfn_set_property_proc,
                                                                                               pfn_set_texture_memory_allocation_proc,
                                                                                               node_type);

    ASSERT_ALWAYS_SYNC(new_node_ptr != nullptr,
                       "Out of memory");

    /* Sign up for RAL context notifications */
    _demo_timeline_segment_node_subscribe_for_notifications(new_node_ptr,
                                                            true /* should_subscribe */);

    return (demo_timeline_segment_node) new_node_ptr;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_node_delete_texture_input(demo_timeline_segment_node          node,
                                                            demo_timeline_segment_node_input_id input_id)
{
    _demo_timeline_segment_node*            node_ptr             = (_demo_timeline_segment_node*) node;
    bool                                    result               = false;
    _demo_timeline_segment_node_texture_io* texture_io_ptr       = nullptr;
    size_t                                  texture_io_vec_index = -1;

    /* Sanity checks */
    if (node_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment node is NULL");

        goto end;
    }

    /* Retrieve the output's descriptor */
    if (!system_hash64map_get(node_ptr->texture_input_id_to_texture_io_descriptor_map,
                              (system_hash64) input_id,
                             &texture_io_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve texture input descriptor");

        goto end;
    }

    if ( (texture_io_vec_index = system_resizable_vector_find(node_ptr->texture_inputs,
                                                              texture_io_ptr) ) == ITEM_NOT_FOUND)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not locate texture input descriptor in texture inputs vector");

        goto end;
    }

    /* Wipe the descriptor out of all containers it is stored in */
    if (!system_hash64map_remove(node_ptr->texture_input_id_to_texture_io_descriptor_map,
                                 (system_hash64) input_id) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not remove texture input descriptor from the texture input ID->texture input descriptor map");
    }

    if (!system_resizable_vector_delete_element_at(node_ptr->texture_inputs,
                                                   texture_io_vec_index) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not remove texture input descriptor from the texture inputs vector");
    }

    /* If there's any texture bound to the input, release it */
    if (texture_io_ptr->bound_texture != nullptr)
    {
        demo_timeline_segment_node_callback_texture_detached_callback_argument callback_arg;

        callback_arg.id          = input_id;
        callback_arg.is_input_id = true;
        callback_arg.node        = node;
        callback_arg.texture     = texture_io_ptr->bound_texture;

        system_callback_manager_call_back(texture_io_ptr->parent_node_ptr->callback_manager,
                                          DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_DETACHED,
                                          texture_io_ptr->bound_texture);

        texture_io_ptr->bound_texture = nullptr;
    }

    /* Release the descriptor. */
    delete texture_io_ptr;

    /* Notify subscribers about the fact the node no longer offers a texture output. */
    system_callback_manager_call_back(node_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_INPUT_DELETED,
                                      (void*) input_id);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_node_delete_texture_output(demo_timeline_segment_node           node,
                                                             demo_timeline_segment_node_output_id output_id)
{
    _demo_timeline_segment_node*            node_ptr             = (_demo_timeline_segment_node*) node;
    bool                                    result               = false;
    _demo_timeline_segment_node_texture_io* texture_io_ptr       = nullptr;
    size_t                                  texture_io_vec_index = -1;

    /* Sanity checks */
    if (node_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment node is NULL");

        goto end;
    }

    /* Retrieve the output's descriptor */
    if (!system_hash64map_get(node_ptr->texture_output_id_to_texture_io_descriptor_map,
                              (system_hash64) output_id,
                             &texture_io_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve texture output descriptor");

        goto end;
    }

    if ( (texture_io_vec_index = system_resizable_vector_find(node_ptr->texture_outputs,
                                                              texture_io_ptr) ) == ITEM_NOT_FOUND)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not locate texture output descriptor in texture outputs vector");

        goto end;
    }

    /* Wipe the descriptor out of all containers it is stored in */
    if (!system_hash64map_remove(node_ptr->texture_output_id_to_texture_io_descriptor_map,
                                 (system_hash64) output_id) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not remove texture output descriptor from the texture output ID->texture output descriptor map");
    }

    if (!system_resizable_vector_delete_element_at(node_ptr->texture_outputs,
                                                   texture_io_vec_index) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not remove texture output descriptor from the texture outputs vector");
    }

    /* If there's any texture bound to the output, release it */
    if (texture_io_ptr->bound_texture != nullptr)
    {
        demo_timeline_segment_node_callback_texture_detached_callback_argument callback_arg;

        callback_arg.id          = output_id;
        callback_arg.is_input_id = false;
        callback_arg.node        = node;
        callback_arg.texture     = texture_io_ptr->bound_texture;

        system_callback_manager_call_back(texture_io_ptr->parent_node_ptr->callback_manager,
                                          DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_DETACHED,
                                         &callback_arg);

        texture_io_ptr->bound_texture = nullptr;
    }

    /* Release the descriptor. */
    delete texture_io_ptr;

    /* Notify subscribers about the fact the node no longer offers a texture output. */
    demo_timeline_segment_node_callback_texture_output_deleted_callback_argument callback_arg;

    callback_arg.node      = node;
    callback_arg.output_id = output_id;

    system_callback_manager_call_back(node_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_DELETED,
                                     &callback_arg);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC void demo_timeline_segment_node_get_inputs(demo_timeline_segment_node           node,
                                                  uint32_t                             n_input_ids,
                                                  uint32_t*                            out_opt_n_input_ids_ptr,
                                                  demo_timeline_segment_node_input_id* out_opt_input_ids_ptr)
{
    _demo_timeline_segment_node_get_input_output_ids(node,
                                                     true, /* should_return_input_data */
                                                     n_input_ids,
                                                     out_opt_n_input_ids_ptr,
                                                     out_opt_input_ids_ptr);
}

/** Please see header for specification */
PUBLIC void demo_timeline_segment_node_get_io_property(demo_timeline_segment_node             node,
                                                       bool                                   is_input_io,
                                                       uint32_t                               io_id,
                                                       demo_timeline_segment_node_io_property property,
                                                       void*                                  out_result_ptr)
{
    _demo_timeline_segment_node*            node_ptr       = (_demo_timeline_segment_node*) node;
    _demo_timeline_segment_node_texture_io* texture_io_ptr = nullptr;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node_ptr != nullptr,
                      "Input node handle is NULL");

    /* Retrieve the output's descriptor.
     *
     * TODO: We currently assume all outputs are texture outputs but that will not hold in the future! */
    if (!system_hash64map_get( (is_input_io) ? node_ptr->texture_input_id_to_texture_io_descriptor_map
                                             : node_ptr->texture_output_id_to_texture_io_descriptor_map,
                              (system_hash64) io_id,
                             &texture_io_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "IO ID was not recognized");

        goto end;
    }

    switch (property)
    {
        case DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_BOUND_TEXTURE_RAL:
        {
            *(ral_texture*) out_result_ptr = texture_io_ptr->bound_texture;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_INTERFACE_TYPE:
        {
            *(demo_timeline_segment_node_interface_type*) out_result_ptr = DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_IS_REQUIRED:
        {
            *(bool*) out_result_ptr = texture_io_ptr->is_attachment_required;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result_ptr = texture_io_ptr->name;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_FORMAT:
        {
            *(ral_format*) out_result_ptr = texture_io_ptr->format;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_N_COMPONENTS:
        {
            *(uint32_t*) out_result_ptr = texture_io_ptr->n_components;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_N_LAYERS:
        {
            *(uint32_t*) out_result_ptr = texture_io_ptr->n_layers;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_N_SAMPLES:
        {
            *(uint32_t*) out_result_ptr = texture_io_ptr->n_samples;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_TYPE:
        {
            *(ral_texture_type*) out_result_ptr = texture_io_ptr->type;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_USES_FULL_MIPMAP_CHAIN:
        {
            *(bool*) out_result_ptr = texture_io_ptr->needs_full_mipmap_chain;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_sement_node_output_property value.");
        }
    }

    /* All done */
end:
    ;
}

/** Please see header for specification */
PUBLIC void demo_timeline_segment_node_get_outputs(demo_timeline_segment_node            node,
                                                   uint32_t                              n_output_ids,
                                                   uint32_t*                             out_opt_n_output_ids_ptr,
                                                   demo_timeline_segment_node_output_id* out_opt_output_ids_ptr)
{
    _demo_timeline_segment_node_get_input_output_ids(node,
                                                     false, /* should_return_input_data */
                                                     n_output_ids,
                                                     out_opt_n_output_ids_ptr,
                                                     out_opt_output_ids_ptr);
}

/** Please see header for spec */
PUBLIC void demo_timeline_segment_node_get_property(demo_timeline_segment_node          node,
                                                    demo_timeline_segment_node_property property,
                                                    void*                               out_result_ptr)
{
    _demo_timeline_segment_node* node_ptr = (_demo_timeline_segment_node*) node;

    ASSERT_DEBUG_SYNC(node_ptr != nullptr,
                      "Input demo_timeline_segment_node instance is NULL");

    switch (property)
    {
        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result_ptr = node_ptr->callback_manager;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_ID:
        {
            *(demo_timeline_segment_node_id*) out_result_ptr = node_ptr->id;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_DEINIT_FUNC_PTR:
        {
            *(PFNSEGMENTNODEDEINITCALLBACKPROC*) out_result_ptr = node_ptr->pfn_deinit_proc;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_GET_PROPERTY_FUNC_PTR:
        {
            *(PFNSEGMENTNODEGETPROPERTYCALLBACKPROC*) out_result_ptr = node_ptr->pfn_get_property_proc;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_GET_TEXTURE_MEMORY_ALLOCATION_DETAILS_FUNC_PTR:
        {
            *(PFNSEGMENTNODEGETTEXTUREMEMORYALLOCATIONDETAILSPROC*) out_result_ptr = node_ptr->pfn_get_texture_memory_allocation_details_proc;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_INIT_FUNC_PTR:
        {
            *(PFNSEGMENTNODEINITCALLBACKPROC*) out_result_ptr = node_ptr->pfn_init_proc;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_RENDER_FUNC_PTR:
        {
            *(PFNSEGMENTNODERENDERCALLBACKPROC*) out_result_ptr = node_ptr->pfn_render_proc;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_SET_PROPERTY_FUNC_PTR:
        {
            *(PFNSEGMENTNODESETPROPERTYCALLBACKPROC*) out_result_ptr = node_ptr->pfn_set_property_proc;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_SET_TEXTURE_MEMORY_ALLOCATION_FUNC_PTR:
        {
            *(PFNSEGMENTNODESETTEXTUREMEMORYALLOCATIONPROC*) out_result_ptr = node_ptr->pfn_set_texture_memory_allocation_proc;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_TYPE:
        {
            *(int*) out_result_ptr = node_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Input demo_timeline_segment_node_property value was not recognized");
        }
    }
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_node_get_texture_attachment(demo_timeline_segment_node node,
                                                              bool                       is_input_id,
                                                              uint32_t                   id,
                                                              ral_texture*               out_attached_texture_ptr)
{
    _demo_timeline_segment_node*            node_ptr       = (_demo_timeline_segment_node*) node;
    bool                                    result         = false;
    _demo_timeline_segment_node_texture_io* texture_io_ptr = nullptr;

    system_hash64map item_map = is_input_id ? node_ptr->texture_input_id_to_texture_io_descriptor_map
                                            : node_ptr->texture_output_id_to_texture_io_descriptor_map;

    /* Sanity checks */
    if (out_attached_texture_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input out_attached_texture_ptr argument is NULL");

        goto end;
    }

    if (node == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment node is NULL");

        goto end;
    }

    /* Retrieve the bound texture instance. */
    if (!system_hash64map_get(item_map,
                              id,
                             &texture_io_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid input segment node IO ID");

        goto end;
    }

    ASSERT_DEBUG_SYNC(texture_io_ptr != nullptr,
                      "Texture IO descriptor is NULL");

    *out_attached_texture_ptr = texture_io_ptr->bound_texture;

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for spec */
PUBLIC bool demo_timeline_segment_node_is_node_output_compatible_with_node_input(demo_timeline_segment_node           src_node,
                                                                                 demo_timeline_segment_node_output_id src_node_output_id,
                                                                                 demo_timeline_segment_node           dst_node,
                                                                                 demo_timeline_segment_node_input_id  dst_node_input_id)
{
    _demo_timeline_segment_node_texture_io* dst_node_input_ptr  = nullptr;
    _demo_timeline_segment_node*            dst_node_ptr        = (_demo_timeline_segment_node*) dst_node;
    bool                                    result              = false;
    _demo_timeline_segment_node_texture_io* src_node_output_ptr = nullptr;
    _demo_timeline_segment_node*            src_node_ptr        = (_demo_timeline_segment_node*) src_node;

    /* Sanity checks */
    if (dst_node_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Destination node instance is NULL");

        goto end;
    }

    if (src_node_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Source node instance is NULL");

        goto end;
    }

    /* Retrieve input & output descriptors */
    if (!system_hash64map_get(dst_node_ptr->texture_input_id_to_texture_io_descriptor_map,
                              (system_hash64) dst_node_input_id,
                             &dst_node_input_ptr) ||
        !system_hash64map_get(src_node_ptr->texture_output_id_to_texture_io_descriptor_map,
                              (system_hash64) src_node_output_id,
                             &src_node_output_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Source node's output ID or destination node's input ID was not recognized");

        goto end;
    }

    /* Perform compatibility checks. */
    if (src_node_output_ptr->format != dst_node_input_ptr->format)
    {
        LOG_FATAL("Format exposed by the source node output does not match the destination node input's.");

        goto end;
    }

    if (src_node_output_ptr->n_components != dst_node_input_ptr->n_components)
    {
        LOG_FATAL("Number of texture components exposed by the source node output does not match the destination node input's.");

        goto end;
    }

    if (src_node_output_ptr->n_layers != dst_node_input_ptr->n_layers)
    {
        LOG_FATAL("Number of texture layers exposed by the source node output does not match the destination node input's.");

        goto end;
    }

    if (src_node_output_ptr->n_samples != dst_node_input_ptr->n_samples)
    {
        LOG_FATAL("Number of samples exposed by the source node output does not match the destination node input's.");

        goto end;
    }

    if (src_node_output_ptr->type != dst_node_input_ptr->type)
    {
        LOG_FATAL("Source node output's texture type does not match destination node input's.");

        goto end;
    }

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for spec */
PUBLIC void demo_timeline_segment_node_release(demo_timeline_segment_node node)
{
    _demo_timeline_segment_node* node_ptr = (_demo_timeline_segment_node*) node;

    ASSERT_DEBUG_SYNC(node != nullptr,
                      "Input demo_timeline_segment_node instance is NULL");

    if (node_ptr != nullptr)
    {
        _demo_timeline_segment_node_subscribe_for_notifications(node_ptr,
                                                                false /* should_subscribe */);

        delete node_ptr;
    }
}