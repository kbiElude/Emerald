/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"

#include "demo/demo_timeline_segment_node.h"
#include "ogl/ogl_texture.h"
#include "ral/ral_types.h"
#include "ral/ral_utils.h"
#include "system/system_callback_manager.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

typedef struct _demo_timeline_segment_node_texture_input
{
    ral_texture_format                  format;
    bool                                is_attachment_required;
    system_hashed_ansi_string           name;
    unsigned int                        n_components;
    unsigned int                        n_layers;
    unsigned int                        n_samples;
    struct _demo_timeline_segment_node* parent_node_ptr;
    ral_texture_type                    type;

    ogl_texture bound_texture;

    explicit _demo_timeline_segment_node_texture_input(_demo_timeline_segment_node* in_parent_node_ptr,
                                                       system_hashed_ansi_string    in_name,
                                                       ral_texture_format           in_format,
                                                       unsigned int                 in_n_components,
                                                       unsigned int                 in_n_layers,
                                                       unsigned int                 in_n_samples,
                                                       ral_texture_type             in_type,
                                                       bool                         in_is_attachment_required_to_render)
    {
        bound_texture          = NULL;
        format                 = in_format;
        is_attachment_required = in_is_attachment_required_to_render;
        name                   = in_name;
        n_components           = in_n_components;
        n_layers               = in_n_layers;
        n_samples              = in_n_samples;
        parent_node_ptr        = in_parent_node_ptr;
        type                   = in_type;
    }

    ~_demo_timeline_segment_node_texture_input()
    {
        ASSERT_DEBUG_SYNC(bound_texture == NULL,
                          "Bound texture should've been released before the destructor was hit.");
    }
} _demo_timeline_segment_node_texture_input;

typedef struct _demo_timeline_segment_node_texture_output
{
    ral_texture_component               exposed_components[4];
    ral_texture_format                  format;
    system_hashed_ansi_string           name;
    bool                                needs_full_mipmap_chain;
    unsigned int                        n_components;
    unsigned int                        n_layers;
    unsigned int                        n_samples;
    unsigned int                        n_start_layer_index;
    struct _demo_timeline_segment_node* parent_node_ptr;
    uint32_t                            size_absolute[3];
    demo_texture_size_mode              size_mode;
    float                               size_proportional[2];
    ral_texture_type                    type;

    ogl_texture bound_texture;

    explicit _demo_timeline_segment_node_texture_output(_demo_timeline_segment_node*         in_parent_node_ptr,
                                                        system_hashed_ansi_string            in_name,
                                                        ral_texture_format                   in_format,
                                                        unsigned int                         in_n_start_layer_index,
                                                        unsigned int                         in_n_layers,
                                                        unsigned int                         in_texture_n_components,
                                                        const ral_texture_component*         in_texture_components,
                                                        unsigned int                         in_n_samples,
                                                        ral_texture_type                     in_type,
                                                        const demo_texture_size_declaration& in_texture_size_declaration,
                                                        bool                                 in_needs_full_mipmap_chain)
    {
        bound_texture           = NULL;
        format                  = in_format;
        name                    = in_name;
        needs_full_mipmap_chain = in_needs_full_mipmap_chain;
        n_components            = in_texture_n_components;
        n_layers                = in_n_layers;
        n_samples               = in_n_samples;
        n_start_layer_index     = in_n_start_layer_index;
        parent_node_ptr         = in_parent_node_ptr;
        size_mode               = in_texture_size_declaration.mode;
        type                    = in_type;

        ASSERT_DEBUG_SYNC(sizeof(in_texture_size_declaration.absolute_size)     == sizeof(size_absolute) &&
                          sizeof(in_texture_size_declaration.proportional_size) == sizeof(size_proportional),
                          "Buffer size mismatch");

        memcpy(size_absolute,
               in_texture_size_declaration.absolute_size,
               sizeof(in_texture_size_declaration.absolute_size) );
        memcpy(size_proportional,
               in_texture_size_declaration.proportional_size,
               sizeof(in_texture_size_declaration.proportional_size) );

        memcpy(exposed_components,
               in_texture_components,
               in_texture_n_components * sizeof(ral_texture_component) );
    }

    ~_demo_timeline_segment_node_texture_output()
    {
        ASSERT_DEBUG_SYNC(bound_texture == NULL,
                          "Bound texture should've been released before the destructor was hit.");
    }
} _demo_timeline_segment_node_texture_output;

typedef struct _demo_timeline_segment_node
{
    system_callback_manager               callback_manager;
    demo_timeline_segment_node_id         id;
    void*                                 init_user_args[3];
    PFNSEGMENTNODEDEINITCALLBACKPROC      pfn_deinit_proc;
    PFNSEGMENTNODEGETPROPERTYCALLBACKPROC pfn_get_property_proc;
    PFNSEGMENTNODEINITCALLBACKPROC        pfn_init_proc;
    PFNSEGMENTNODERENDERCALLBACKPROC      pfn_render_proc;
    PFNSEGMENTNODESETPROPERTYCALLBACKPROC pfn_set_property_proc;
    demo_timeline_segment_type            segment_type;
    int                                   type;

    system_resizable_vector   texture_inputs;   /* Own the stored _demo_timeline_video_segment_texture_input instances.
                                                 * NOT sorted relative to the ID; use texture_input_id_to_texture_input_descriptor map for this.
                                                 */
    system_resizable_vector   texture_outputs;  /* Own the stored _demo_timeline_video_segment_texture_output instances.
                                                 * NOT sorted relative to the ID; use texture_output_id_to_texture_output_descriptor map for this.
                                                 */

    system_hash64map          texture_input_id_to_texture_input_descriptor_map;
    system_hash64map          texture_output_id_to_texture_output_descriptor_map;

    _demo_timeline_segment_node(demo_timeline_segment_type            in_segment_type,
                                demo_timeline_segment_node_id         in_id,
                                const void*                           in_init_user_args_void3,
                                PFNSEGMENTNODEDEINITCALLBACKPROC      in_pfn_deinit_proc,
                                PFNSEGMENTNODEGETPROPERTYCALLBACKPROC in_pfn_get_property_proc,
                                PFNSEGMENTNODEINITCALLBACKPROC        in_pfn_init_proc,
                                PFNSEGMENTNODERENDERCALLBACKPROC      in_pfn_render_proc,
                                PFNSEGMENTNODESETPROPERTYCALLBACKPROC in_pfn_set_property_proc,
                                int                                   in_type)
    {
        callback_manager      = system_callback_manager_create( (_callback_id) DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_COUNT);
        id                    = in_id;
        pfn_deinit_proc       = in_pfn_deinit_proc;
        pfn_get_property_proc = in_pfn_get_property_proc;
        pfn_init_proc         = in_pfn_init_proc;
        pfn_render_proc       = in_pfn_render_proc;
        pfn_set_property_proc = in_pfn_set_property_proc;
        segment_type          = in_segment_type;
        type                  = in_type;

        memcpy(init_user_args,
               in_init_user_args_void3,
               sizeof(init_user_args) );

        texture_inputs                                     = system_resizable_vector_create(4 /* capacity */);
        texture_outputs                                    = system_resizable_vector_create(4 /* capacity */);
        texture_input_id_to_texture_input_descriptor_map   = system_hash64map_create(sizeof(_demo_timeline_segment_node_texture_input*)  );
        texture_output_id_to_texture_output_descriptor_map = system_hash64map_create(sizeof(_demo_timeline_segment_node_texture_output*) );

        ASSERT_ALWAYS_SYNC(callback_manager != NULL,
                           "Could not spawn the callback manager");
        ASSERT_ALWAYS_SYNC(texture_inputs != NULL,
                           "Could not spawn the video segment texture inputs vector");
        ASSERT_ALWAYS_SYNC(texture_input_id_to_texture_input_descriptor_map != NULL,
                           "Could not spawn the texture input ID->texture input descriptor hash map");
        ASSERT_ALWAYS_SYNC(texture_outputs != NULL,
                           "Could not spawn the video segment texture outputs vector");
        ASSERT_ALWAYS_SYNC(texture_output_id_to_texture_output_descriptor_map != NULL,
                           "Could not spawn the texture output ID->texture out6put descriptor hash map");
    }

    ~_demo_timeline_segment_node()
    {
        /* Release the owned objects */
        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        } /* if (callback_manager != NULL) */

        if (texture_input_id_to_texture_input_descriptor_map != NULL)
        {
            system_hash64map_release(texture_input_id_to_texture_input_descriptor_map);

            texture_input_id_to_texture_input_descriptor_map = NULL;
        } /* if (texture_input_id_to_texture_input_descriptor_map != NULL) */

        if (texture_inputs != NULL)
        {
            _demo_timeline_segment_node_texture_input* texture_input_ptr = NULL;

            while (system_resizable_vector_pop(texture_inputs,
                                              &texture_input_ptr) )
            {
                delete texture_input_ptr;

                texture_input_ptr = NULL;
            }

            system_resizable_vector_release(texture_inputs);
            texture_inputs = NULL;
        } /* if (texture_inputs != NULL) */

        if (texture_output_id_to_texture_output_descriptor_map != NULL)
        {
            system_hash64map_release(texture_output_id_to_texture_output_descriptor_map);

            texture_output_id_to_texture_output_descriptor_map = NULL;
        } /* if (textureo_output_id_to_texture_output_descriptor_map != NULL) */

        if (texture_outputs != NULL)
        {
            _demo_timeline_segment_node_texture_output* texture_output_ptr = NULL;

            while (system_resizable_vector_pop(texture_outputs,
                                              &texture_output_ptr) )
            {
                delete texture_output_ptr;

                texture_output_ptr = NULL;
            }

            system_resizable_vector_release(texture_outputs);
            texture_outputs = NULL;
        } /* if (texture_outputs != NULL) */
    }
} _demo_timeline_segment_node;


/** TODO */
PRIVATE void _demo_timeline_segment_node_get_input_output_ids(demo_timeline_segment_node           node,
                                                              bool                                 should_return_input_data,
                                                              uint32_t                             n_ids,
                                                              uint32_t*                            out_opt_n_ids_ptr,
                                                              demo_timeline_segment_node_input_id* out_opt_ids_ptr)
{
    _demo_timeline_segment_node* node_ptr          = (_demo_timeline_segment_node*) node;
    uint32_t                     n_vector_elements = 0;
    system_resizable_vector      data_vector       = (should_return_input_data) ? node_ptr->texture_inputs
                                                                                : node_ptr->texture_outputs;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node != NULL,
                      "Input node is NULL");

    /* Determine the number of inputs the specified video segment has to offer */
    system_resizable_vector_get_property(data_vector,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_vector_elements);

    if (out_opt_n_ids_ptr != NULL)
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
        void* raw_ids = NULL;

        system_resizable_vector_get_property(data_vector,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                            &raw_ids);

        ASSERT_DEBUG_SYNC(sizeof(void*) == sizeof(demo_timeline_segment_node_input_id),
                          "TODO");

        memcpy(out_opt_ids_ptr,
               raw_ids,
               sizeof(demo_timeline_segment_node_input_id) * n_ids);
    } /* if (there's space to copy input IDs to) */
}


/** Please see header for specification */
PUBLIC bool demo_timeline_segment_node_add_texture_input(demo_timeline_segment_node            node,
                                                         const demo_texture_input_declaration* new_input_properties_ptr,
                                                         demo_timeline_segment_node_input_id*  out_input_id_ptr)
{
    _demo_timeline_segment_node* node_ptr = (_demo_timeline_segment_node*) node;
    bool                         result   = false;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node != NULL,
                      "Input node is NULL");

    ASSERT_DEBUG_SYNC(new_input_properties_ptr->texture_format != RAL_TEXTURE_FORMAT_UNKNOWN,
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
    _demo_timeline_segment_node_texture_input* new_texture_input_ptr = new (std::nothrow) _demo_timeline_segment_node_texture_input(node_ptr,
                                                                                                                                    new_input_properties_ptr->input_name,
                                                                                                                                    new_input_properties_ptr->texture_format,
                                                                                                                                    new_input_properties_ptr->texture_n_components,
                                                                                                                                    new_input_properties_ptr->texture_n_layers,
                                                                                                                                    new_input_properties_ptr->texture_n_samples,
                                                                                                                                    new_input_properties_ptr->texture_type,
                                                                                                                                    new_input_properties_ptr->is_attachment_required);

    ASSERT_DEBUG_SYNC(new_texture_input_ptr != NULL,
                      "Out of memory");

    if (new_texture_input_ptr != NULL)
    {
        uint32_t new_input_id = -1;

        system_resizable_vector_get_property(node_ptr->texture_inputs,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &new_input_id);

        /* Sanity checks.. */
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(node_ptr->texture_input_id_to_texture_input_descriptor_map,
                                                     (system_hash64) new_input_id),
                          "A descriptor is already associated to the new texture input's ID!");

        /* Store it */
        *out_input_id_ptr = new_input_id;
        result            = true;

        system_resizable_vector_push(node_ptr->texture_inputs,
                                     new_texture_input_ptr);
        system_hash64map_insert     (node_ptr->texture_input_id_to_texture_input_descriptor_map,
                                     (system_hash64) new_input_id,
                                     new_texture_input_ptr,
                                     NULL,  /* on_remove_callback */
                                     NULL); /* callback_argument */

        /* Ping the subscribers about the fact a new texture input has arrived */
        system_callback_manager_call_back(node_ptr->callback_manager,
                                          DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_INPUT_ADDED,
                                          (void*) new_input_id);
    } /* if (new_texture_input_ptr != NULL) */

    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_node_add_texture_output(demo_timeline_segment_node             node,
                                                          const demo_texture_output_declaration* new_output_properties_ptr,
                                                          demo_timeline_segment_node_output_id*  out_output_id_ptr)
{
    _demo_timeline_segment_node* node_ptr = (_demo_timeline_segment_node*) node;
    bool                         result   = false;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node != NULL,
                      "Input node is NULL");

    ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_format != RAL_TEXTURE_FORMAT_UNKNOWN,
                      "Unknown texture format is not allowed");
    ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_type != RAL_TEXTURE_TYPE_UNKNOWN,
                      "Unknown texture type is not allowed");
    ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_n_layers >= 1,
                      "Output texture must not use 0 layers.");
    ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_n_samples >= 1,
                      "Output texture must not use 0 samples.");
    ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_n_components >= 1 && new_output_properties_ptr->texture_n_components <= 4,
                      "Output texture must be use at least 1, but no more than 4 components. ");
    ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_components != NULL,
                      "Texture components array must not be NULL");

    ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_size.mode == DEMO_TEXTURE_SIZE_MODE_ABSOLUTE     ||
                      new_output_properties_ptr->texture_size.mode == DEMO_TEXTURE_SIZE_MODE_PROPORTIONAL,
                      "Unrecognized texture size mode requested");

    for (unsigned int n_texture_component = 0;
                      n_texture_component < new_output_properties_ptr->texture_n_components;
                    ++n_texture_component)
    {
        ASSERT_DEBUG_SYNC(new_output_properties_ptr->texture_components[n_texture_component] < RAL_TEXTURE_COMPONENT_COUNT,
                          "Invalid texture component specified");
    } /* for (all components to expose) */

    /* Create a new texture output instance */
    _demo_timeline_segment_node_texture_output* new_texture_output_ptr = new (std::nothrow) _demo_timeline_segment_node_texture_output(node_ptr,
                                                                                                                                       new_output_properties_ptr->output_name,
                                                                                                                                       new_output_properties_ptr->texture_format,
                                                                                                                                       new_output_properties_ptr->texture_n_start_layer_index,
                                                                                                                                       new_output_properties_ptr->texture_n_layers,
                                                                                                                                       new_output_properties_ptr->texture_n_components,
                                                                                                                                       new_output_properties_ptr->texture_components,
                                                                                                                                       new_output_properties_ptr->texture_n_samples,
                                                                                                                                       new_output_properties_ptr->texture_type,
                                                                                                                                       new_output_properties_ptr->texture_size,
                                                                                                                                       new_output_properties_ptr->texture_needs_full_mipmap_chain);

    ASSERT_DEBUG_SYNC(new_texture_output_ptr != NULL,
                      "Out of memory");

    if (new_texture_output_ptr != NULL)
    {
        uint32_t new_output_id = -1;

        system_resizable_vector_get_property(node_ptr->texture_outputs,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &new_output_id);

        /* Sanity checks */
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(node_ptr->texture_output_id_to_texture_output_descriptor_map,
                                                     (system_hash64) new_output_id),
                          "A descriptor is already associated to the new texture output's ID!");

        /* Store it */
        *out_output_id_ptr = new_output_id;
        result             = true;

        system_hash64map_insert     (node_ptr->texture_output_id_to_texture_output_descriptor_map,
                                     (system_hash64) new_output_id,
                                     new_texture_output_ptr,
                                     NULL,  /* on_remove_callback */
                                     NULL); /* callback_argument  */

        system_resizable_vector_push(node_ptr->texture_outputs,
                                     new_texture_output_ptr);

        /* Ping the subscribers about the fact a new texture input has arrived */
        system_callback_manager_call_back(node_ptr->callback_manager,
                                          DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_ADDED,
                                          (void*) new_output_id);
    } /* if (new_texture_output_ptr != NULL) */

    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_node_attach_texture_to_texture_io(demo_timeline_segment_node                 node,
                                                                    bool                                       is_input_id,
                                                                    uint32_t                                   id,
                                                                    const demo_texture_attachment_declaration* texture_attachment_ptr)
{
    _demo_timeline_segment_node*                node_ptr              = (_demo_timeline_segment_node*) node;
    bool                                        result                = false;
    _demo_timeline_segment_node_texture_input*  texture_input_ptr     = NULL;
    _demo_timeline_segment_node_texture_output* texture_output_ptr    = NULL;
    ogl_texture*                                texture_to_detach_ptr = NULL;

    ASSERT_DEBUG_SYNC(node_ptr != NULL,
                      "Input node is NULL");

    system_hash64map io_map = (is_input_id) ? node_ptr->texture_input_id_to_texture_input_descriptor_map
                                            : node_ptr->texture_output_id_to_texture_output_descriptor_map;
    void*            io_ptr = (is_input_id) ? (void*) &texture_input_ptr
                                            : (void*) &texture_output_ptr;

    /* First, retrieve the IO descriptor */
    if (!system_hash64map_get(io_map,
                              id,
                              io_ptr))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve texture IO with ID [%d]",
                          id);

        goto end;
    } /* if (IO descriptor could not've been retrieved) */

    if (texture_attachment_ptr != NULL)
    {
        /* Make sure the texture the caller wants to attach is different from the one that's already bound */
        if (is_input_id)
        {
            if (texture_attachment_ptr->texture == texture_input_ptr->bound_texture)
            {
                LOG_ERROR("Redundant demo_timeline_segment_node_attach_texture_to_texture_io() call detected.");

                goto end;
            }
        } /* if (is_input_id) */
        else
        {
            if (texture_attachment_ptr->texture == texture_output_ptr->bound_texture)
            {
                LOG_ERROR("Redundant demo_timeline_segment_node_attach_texture_to_texture_io() call detected.");

                goto end;
            }
        }

        /* Make sure the texture can actually be bound to the texture input. */
        bool               is_arg_texture_layered      = false;
        ral_texture_format texture_format              = RAL_TEXTURE_FORMAT_UNKNOWN;
        uint32_t           texture_format_n_components = 0;
        uint32_t           texture_n_layers            = 0;
        uint32_t           texture_n_samples           = 0;
        ral_texture_type   texture_type                = RAL_TEXTURE_TYPE_UNKNOWN;

        is_arg_texture_layered = ral_utils_is_texture_type_layered(texture_input_ptr->type);

        ogl_texture_get_property(texture_attachment_ptr->texture,
                                 OGL_TEXTURE_PROPERTY_FORMAT_RAL,
                                &texture_format);
        ogl_texture_get_property(texture_attachment_ptr->texture,
                                 OGL_TEXTURE_PROPERTY_N_LAYERS,
                                &texture_n_layers);
        ogl_texture_get_property(texture_attachment_ptr->texture,
                                 OGL_TEXTURE_PROPERTY_N_SAMPLES,
                                &texture_n_samples);
        ogl_texture_get_property(texture_attachment_ptr->texture,
                                 OGL_TEXTURE_PROPERTY_TYPE,
                                &texture_type);

        ral_utils_get_texture_format_property(texture_format,
                                              RAL_TEXTURE_FORMAT_PROPERTY_N_COMPONENTS,
                                             &texture_format_n_components);

        if ( is_input_id && texture_format_n_components != texture_input_ptr->n_components ||
            !is_input_id && texture_format_n_components != texture_output_ptr->n_components)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Number of components of the texture to be attached does not match the number of components required by the specified segment node's IO");

            goto end;
        } /* if (number of components mismatch) */

        if ( is_input_id && texture_input_ptr->format  != texture_format ||
            !is_input_id && texture_output_ptr->format != texture_format)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Format of the texture to be attached does not match the format required by the specified segment node's IO");

            goto end;
        } /* if (format mismatch detected) */

        if ( is_input_id && texture_input_ptr->type  != texture_type ||
            !is_input_id && texture_output_ptr->type != texture_type)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Type of the texture to be attached does not match the type required by the specified segment node's IO");

            goto end;
        } /* if (texture type mismatch) */

        if ( is_input_id && texture_input_ptr->n_samples  != texture_n_samples ||
            !is_input_id && texture_output_ptr->n_samples != texture_n_samples)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Number of samples of the texture to be attached does not match the number of samples expected by the specified segment node's IO");

            goto end;
        } /* if (n_samples mismatch) */

        if (is_arg_texture_layered)
        {
            if ( is_input_id && (texture_input_ptr->n_layers  < texture_n_layers) ||
                !is_input_id && (texture_output_ptr->n_layers < texture_n_layers) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Number of layers defined for the texture to be attached does not match the number of layers expected by the specified segment node's IO");

                goto end;
            }
        } /* if (is_arg_texture_layered) */
    } /* if (texture_attachment_ptr != NULL) */

    /* If there's an existing texture attachment, detach it before continuing */
    if (is_input_id)
    {
        texture_to_detach_ptr = &texture_input_ptr->bound_texture;
    } /* if (is_input_id) */
    else
    {
        texture_to_detach_ptr = &texture_output_ptr->bound_texture;
    }

    if (*texture_to_detach_ptr != NULL)
    {
        system_callback_manager_call_back(node_ptr->callback_manager,
                                          DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_DETACHED,
                                          *texture_to_detach_ptr);

        *texture_to_detach_ptr = NULL;
    } /* if (*texture_to_detach_ptr != NULL) */

    /* Looks like this texture can be bound to the specified IO! */
    if (is_input_id)
    {
        texture_input_ptr->bound_texture = texture_attachment_ptr->texture;
    }
    else
    {
        texture_output_ptr->bound_texture = texture_attachment_ptr->texture;
    }

    /* Notify subscribers */
    system_callback_manager_call_back(texture_output_ptr->parent_node_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_ATTACHED,
                                      texture_attachment_ptr->texture);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for spec */
PUBLIC demo_timeline_segment_node demo_timeline_segment_node_create(demo_timeline_segment_type            segment_type,
                                                                    int                                   node_type,
                                                                    demo_timeline_segment_node_id         node_id,
                                                                    PFNSEGMENTNODEDEINITCALLBACKPROC      pfn_deinit_proc,
                                                                    PFNSEGMENTNODEGETPROPERTYCALLBACKPROC pfn_get_property_proc,
                                                                    PFNSEGMENTNODEINITCALLBACKPROC        pfn_init_proc,
                                                                    const void*                           init_user_args_void3,
                                                                    PFNSEGMENTNODERENDERCALLBACKPROC      pfn_render_proc,
                                                                    PFNSEGMENTNODESETPROPERTYCALLBACKPROC pfn_set_property_proc,
                                                                    system_hashed_ansi_string             node_name)
{
    _demo_timeline_segment_node* new_node_ptr = new (std::nothrow) _demo_timeline_segment_node(segment_type,
                                                                                               node_id,
                                                                                               init_user_args_void3,
                                                                                               pfn_deinit_proc,
                                                                                               pfn_get_property_proc,
                                                                                               pfn_init_proc,
                                                                                               pfn_render_proc,
                                                                                               pfn_set_property_proc,
                                                                                               node_type);

    ASSERT_ALWAYS_SYNC(new_node_ptr != NULL,
                       "Out of memory");

    return (demo_timeline_segment_node) new_node_ptr;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_node_delete_texture_input(demo_timeline_segment_node          node,
                                                            demo_timeline_segment_node_input_id input_id)
{
    _demo_timeline_segment_node*                node_ptr                = (_demo_timeline_segment_node*) node;
    bool                                        result                  = false;
    _demo_timeline_segment_node_texture_output* texture_input_ptr       = NULL;
    size_t                                      texture_input_vec_index = -1;

    /* Sanity checks */
    if (node_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment node is NULL");

        goto end;
    }

    /* Retrieve the output's descriptor */
    if (!system_hash64map_get(node_ptr->texture_input_id_to_texture_input_descriptor_map,
                              (system_hash64) input_id,
                             &texture_input_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve texture input descriptor");

        goto end;
    }

    if ( (texture_input_vec_index = system_resizable_vector_find(node_ptr->texture_inputs,
                                                                 texture_input_ptr) ) == ITEM_NOT_FOUND)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not locate texture input descriptor in texture inputs vector");

        goto end;
    }

    /* Wipe the descriptor out of all containers it is stored in */
    if (!system_hash64map_remove(node_ptr->texture_input_id_to_texture_input_descriptor_map,
                                 (system_hash64) input_id) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not remove texture input descriptor from the texture input ID->texture input descriptor map");
    }

    if (!system_resizable_vector_delete_element_at(node_ptr->texture_inputs,
                                                   texture_input_vec_index) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not remove texture input descriptor from the texture inputs vector");
    }

    /* If there's any texture bound to the input, release it */
    if (texture_input_ptr->bound_texture != NULL)
    {
        system_callback_manager_call_back(texture_input_ptr->parent_node_ptr->callback_manager,
                                          DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_DETACHED,
                                          texture_input_ptr->bound_texture);

        texture_input_ptr->bound_texture = NULL;
    } /* if (texture_input_ptr->bound_texture != NULL) */

    /* Release the descriptor. */
    delete texture_input_ptr;

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
    _demo_timeline_segment_node*                node_ptr                 = (_demo_timeline_segment_node*) node;
    bool                                        result                   = false;
    _demo_timeline_segment_node_texture_output* texture_output_ptr       = NULL;
    size_t                                      texture_output_vec_index = -1;

    /* Sanity checks */
    if (node_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment node is NULL");

        goto end;
    }

    /* Retrieve the output's descriptor */
    if (!system_hash64map_get(node_ptr->texture_output_id_to_texture_output_descriptor_map,
                              (system_hash64) output_id,
                             &texture_output_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve texture output descriptor");

        goto end;
    }

    if ( (texture_output_vec_index = system_resizable_vector_find(node_ptr->texture_outputs,
                                                                  texture_output_ptr) ) == ITEM_NOT_FOUND)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not locate texture output descriptor in texture outputs vector");

        goto end;
    }

    /* Wipe the descriptor out of all containers it is stored in */
    if (!system_hash64map_remove(node_ptr->texture_output_id_to_texture_output_descriptor_map,
                                 (system_hash64) output_id) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not remove texture output descriptor from the texture output ID->texture output descriptor map");
    }

    if (!system_resizable_vector_delete_element_at(node_ptr->texture_outputs,
                                                   texture_output_vec_index) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not remove texture output descriptor from the texture outputs vector");
    }

    /* If there's any texture bound to the output, release it */
    if (texture_output_ptr->bound_texture != NULL)
    {
        system_callback_manager_call_back(texture_output_ptr->parent_node_ptr->callback_manager,
                                          DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_DETACHED,
                                          texture_output_ptr->bound_texture);

        texture_output_ptr->bound_texture = NULL;
    } /* if (texture_output_ptr->bound_texture != NULL) */

    /* Release the descriptor. */
    delete texture_output_ptr;

    /* Notify subscribers about the fact the node no longer offers a texture output. */
    system_callback_manager_call_back(node_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_DELETED,
                                      (void*) output_id);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC void demo_timeline_segment_get_input_property(demo_timeline_segment_node                node,
                                                     demo_timeline_segment_node_input_id       input_id,
                                                     demo_timeline_segment_node_input_property property,
                                                     void*                                     out_result_ptr)
{
    _demo_timeline_segment_node*               node_ptr          = (_demo_timeline_segment_node*) node;
    _demo_timeline_segment_node_texture_input* texture_input_ptr = NULL;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node_ptr != NULL,
                      "Input node is NULL");

    /* Retrieve the input's descriptor.
     *
     * TODO: We currently assume all inputs are texture inputs but that will not hold in the future! */
    if (!system_hash64map_get(node_ptr->texture_input_id_to_texture_input_descriptor_map,
                              (system_hash64) input_id,
                             &texture_input_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Texture input ID was not recognized");

        goto end;
    } /* if (input ID->descriptor hash map does not hold the requested descriptor) */

    switch (property)
    {
        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_BOUND_TEXTURE:
        {
            *(ogl_texture*) out_result_ptr = texture_input_ptr->bound_texture;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_INTERFACE_TYPE:
        {
            *(demo_timeline_segment_node_interface_type*) out_result_ptr = DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_IS_REQUIRED:
        {
            *(bool*) out_result_ptr = texture_input_ptr->is_attachment_required;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result_ptr = texture_input_ptr->name;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_FORMAT:
        {
            *(ral_texture_format*) out_result_ptr = texture_input_ptr->format;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_N_COMPONENTS:
        {
            *(uint32_t*) out_result_ptr = texture_input_ptr->n_components;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_N_LAYERS:
        {
            *(uint32_t*) out_result_ptr = texture_input_ptr->n_layers;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_N_SAMPLES:
        {
            *(uint32_t*) out_result_ptr = texture_input_ptr->n_samples;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_TYPE:
        {
            *(ral_texture_type*) out_result_ptr = texture_input_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_video_segment_input_property value.");
        }
    } /* switch (property) */
end:
    ;
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
PUBLIC void demo_timeline_segment_node_get_output_property(demo_timeline_segment_node                 node,
                                                           demo_timeline_segment_node_output_id       output_id,
                                                           demo_timeline_segment_node_output_property property,
                                                           void*                                      out_result_ptr)
{
    _demo_timeline_segment_node*                node_ptr           = (_demo_timeline_segment_node*) node;
    _demo_timeline_segment_node_texture_output* texture_output_ptr = NULL;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node_ptr != NULL,
                      "Input node handle is NULL");

    /* Retrieve the output's descriptor.
     *
     * TODO: We currently assume all outputs are texture outputs but that will not hold in the future! */
    if (!system_hash64map_get(node_ptr->texture_input_id_to_texture_input_descriptor_map,
                              (system_hash64) output_id,
                             &texture_output_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Texture outnput ID was not recognized");

        goto end;
    } /* if (output ID->descriptor hash map does not hold the requested descriptor) */

    switch (property)
    {
        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_BOUND_TEXTURE:
        {
            *(ogl_texture*) out_result_ptr = texture_output_ptr->bound_texture;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_INTERFACE_TYPE:
        {
            *(demo_timeline_segment_node_interface_type*) out_result_ptr = DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_COMPONENTS:
        {
            memcpy(out_result_ptr,
                   texture_output_ptr->exposed_components,
                   sizeof(ral_texture_component) * texture_output_ptr->n_components);

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_FORMAT:
        {
            *(ral_texture_format*) out_result_ptr = texture_output_ptr->format;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_N_COMPONENTS:
        {
            *(uint32_t*) out_result_ptr = texture_output_ptr->n_components;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_N_LAYERS:
        {
            *(uint32_t*) out_result_ptr = texture_output_ptr->n_layers;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_N_SAMPLES:
        {
            *(uint32_t*) out_result_ptr = texture_output_ptr->n_samples;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_N_START_LAYER_INDEX:
        {
            *(uint32_t*) out_result_ptr = texture_output_ptr->n_start_layer_index;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_SIZE_ABSOLUTE:
        {
            ASSERT_DEBUG_SYNC(texture_output_ptr->size_mode == DEMO_TEXTURE_SIZE_MODE_ABSOLUTE,
                              "DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_SIZE_ABSOLUTE query is invalid, texture size mode is not ABSOLUTE.");

            switch (texture_output_ptr->type)
            {
                case RAL_TEXTURE_TYPE_1D:
                {
                    ASSERT_DEBUG_SYNC(texture_output_ptr->size_absolute[0] > 0,
                                      "Invalid texture size detected.");

                    *(uint32_t*) out_result_ptr = texture_output_ptr->size_absolute[0];

                    break;
                }

                case RAL_TEXTURE_TYPE_1D_ARRAY:
                case RAL_TEXTURE_TYPE_2D:
                case RAL_TEXTURE_TYPE_CUBE_MAP:
                case RAL_TEXTURE_TYPE_MULTISAMPLE_2D:
                {
                    ASSERT_DEBUG_SYNC(texture_output_ptr->size_absolute[0] > 0 &&
                                      texture_output_ptr->size_absolute[1] > 0,
                                      "Invalid texture size detected.");

                    memcpy(out_result_ptr,
                           texture_output_ptr->size_absolute,
                           sizeof(uint32_t) * 2);

                    break;
                }

                case RAL_TEXTURE_TYPE_2D_ARRAY:
                case RAL_TEXTURE_TYPE_3D:
                case RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY:
                case RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY:
                {
                    ASSERT_DEBUG_SYNC(texture_output_ptr->size_absolute[0] > 0 &&
                                      texture_output_ptr->size_absolute[1] > 0 &&
                                      texture_output_ptr->size_absolute[2] > 0,
                                      "Invalid texture size detected.");

                    memcpy(out_result_ptr,
                           texture_output_ptr->size_absolute,
                           sizeof(uint32_t) * 3);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Texture type is not supported for the TEXTURE_SIZE_ABSOLUTE query");
                }
            } /* switch (texture_output_ptr->type) */

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_SIZE_MODE:
        {
            *(demo_texture_size_mode*) out_result_ptr = texture_output_ptr->size_mode;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_SIZE_PROPORTIONAL:
        {
            ASSERT_DEBUG_SYNC(texture_output_ptr->size_proportional[0] > 0.0f &&
                              texture_output_ptr->size_proportional[1] > 0.0f,
                              "Invalid texture size detected.");
            ASSERT_DEBUG_SYNC(texture_output_ptr->type == RAL_TEXTURE_TYPE_2D,
                              "TEXTURE_SIZE_PROPORTIONAL query is only valid for 2D texture types");

            memcpy(out_result_ptr,
                   texture_output_ptr->size_proportional,
                   sizeof(texture_output_ptr->size_proportional) );

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_TYPE:
        {
            *(ral_texture_type*) out_result_ptr = texture_output_ptr->type;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_USES_FULL_MIPMAP_CHAIN:
        {
            *(bool*) out_result_ptr = texture_output_ptr->needs_full_mipmap_chain;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_sement_node_output_property value.");
        }
    } /* switch (property) */
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

    ASSERT_DEBUG_SYNC(node_ptr != NULL,
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

        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_INIT_FUNC_PTR:
        {
            *(PFNSEGMENTNODEINITCALLBACKPROC*) out_result_ptr = node_ptr->pfn_init_proc;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_INIT_USER_ARGS:
        {
            *(void**) out_result_ptr = node_ptr->init_user_args;

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
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_node_get_texture_attachment(demo_timeline_segment_node node,
                                                              bool                       is_input_id,
                                                              uint32_t                   id,
                                                              ogl_texture*               out_attached_texture_ptr)
{
    _demo_timeline_segment_node*                node_ptr           = (_demo_timeline_segment_node*) node;
    bool                                        result             = false;
    _demo_timeline_segment_node_texture_input*  texture_input_ptr  = NULL;
    _demo_timeline_segment_node_texture_output* texture_output_ptr = NULL;

    void*                                       dst_ptr            = is_input_id ? (void*) &texture_input_ptr
                                                                                 : (void*) &texture_output_ptr;
    system_hash64map                            item_map           = is_input_id ? node_ptr->texture_input_id_to_texture_input_descriptor_map
                                                                                 : node_ptr->texture_output_id_to_texture_output_descriptor_map;

    /* Sanity checks */
    if (out_attached_texture_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input out_attached_texture_ptr argument is NULL");

        goto end;
    } /* if (out_attached_texture_ptr != NULL) */

    if (node == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment node is NULL");

        goto end;
    } /* if (segment_ptr == NULL) */

    /* Retrieve the bound texture instance. */
    if (!system_hash64map_get(item_map,
                              id,
                              dst_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid input segment node IO ID");

        goto end;
    }

    if (is_input_id)
    {
        ASSERT_DEBUG_SYNC(texture_input_ptr != NULL,
                          "Texture input descriptor is NULL");

        *out_attached_texture_ptr = texture_input_ptr->bound_texture;
    } /* if (is_input_id) */
    else
    {
        ASSERT_DEBUG_SYNC(texture_output_ptr != NULL,
                          "Texture output descriptor is NULL");

        *out_attached_texture_ptr = texture_output_ptr->bound_texture;
    }

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
    _demo_timeline_segment_node_texture_input*  dst_node_input_ptr  = NULL;
    _demo_timeline_segment_node*                dst_node_ptr        = (_demo_timeline_segment_node*) dst_node;
    bool                                        result              = false;
    _demo_timeline_segment_node_texture_output* src_node_output_ptr = NULL;
    _demo_timeline_segment_node*                src_node_ptr        = (_demo_timeline_segment_node*) src_node;

    /* Sanity checks */
    if (dst_node_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Destination node instance is NULL");

        goto end;
    } /* if (dst_node_ptr == NULL) */

    if (src_node_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Source node instance is NULL");

        goto end;
    }

    /* Retrieve input & output descriptors */
    if (!system_hash64map_get(dst_node_ptr->texture_input_id_to_texture_input_descriptor_map,
                              (system_hash64) dst_node_input_id,
                             &dst_node_input_ptr) ||
        !system_hash64map_get(src_node_ptr->texture_output_id_to_texture_output_descriptor_map,
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
    ASSERT_DEBUG_SYNC(node != NULL,
                      "Input demo_timeline_segment_node instance is NULL");

    delete (_demo_timeline_segment_node*) node;
}