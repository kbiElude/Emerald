/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data.h"
#include "collada/collada_data_geometry_material_binding.h"
#include "collada/collada_data_geometry_mesh.h"
#include "collada/collada_data_image.h"
#include "collada/collada_data_input.h"
#include "collada/collada_data_material.h"
#include "collada/collada_data_polylist.h"
#include "collada/collada_data_scene_graph_node_material_instance.h"
#include "collada/collada_data_source.h"
#include "collada/collada_mesh_generator.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_textures.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_resizable_vector.h"

/** TODO
 *
 *  Note: Texcoord data can be described by multiple streams. Each stream starts
 *        at specified offset and takes described amount of bytes. The binding
 *        between each stream and corresponding vertex attribute array is configured
 *        on mesh instance level.
 *
 *        For maintenance reasons, streams are used to build array/index data for
 *        all input types. While perhaps not the most optimal, this should save
 *        the teeth-grinding, should need arise to expand this approach to other
 *        input types.
 **/
typedef struct _collada_mesh_generator_input_data_stream
{
    /* texcoord only: tells which shading property the texture data described
     *                by the UV set is to be associated with (ambient/diffuse/etc.)
     */
    mesh_material_shading_property texcoord_shading_property;

    const float*  array_data; /* collada_data-owned, DO NOT release */
    unsigned int  array_data_size;
    unsigned int* index_data;
    unsigned int  index_delta;
    unsigned int  max_index;
    unsigned int  min_index;

    _collada_mesh_generator_input_data_stream()
    {
        array_data                = NULL;
        array_data_size           = -1;
        index_data                = NULL;
        index_delta               = -1;
        max_index                 = -1;
        min_index                 = -1;
        texcoord_shading_property = MESH_MATERIAL_SHADING_PROPERTY_UNKNOWN;
    }

    ~_collada_mesh_generator_input_data_stream()
    {
        if (index_data != NULL)
        {
            delete [] index_data;

            index_data = NULL;
        }
    }
} _collada_mesh_generator_input_data_stream;

typedef struct _collada_mesh_generator_input_data
{
    unsigned char*           array_data;
    unsigned int             array_data_size;
    unsigned int             n_indices;
    system_resizable_vector  streams;
    _collada_data_input_type type;

    _collada_mesh_generator_input_data()
    {
        array_data      = NULL;
        array_data_size = 0;
        n_indices       = 0;
        streams         = system_resizable_vector_create(4 /* capacity */, sizeof(_collada_mesh_generator_input_data_stream*) );
        type            = COLLADA_DATA_INPUT_TYPE_UNDEFINED;
    }

    ~_collada_mesh_generator_input_data()
    {
        if (array_data != NULL)
        {
            delete [] array_data;

            array_data = NULL;
        }

        if (streams != NULL)
        {
            _collada_mesh_generator_input_data_stream* stream_ptr = NULL;

            while (system_resizable_vector_pop(streams, &stream_ptr) )
            {
                delete stream_ptr;

                stream_ptr = NULL;
            }

            system_resizable_vector_release(streams);
            streams = NULL;
        }
    }
} _collada_mesh_generator_input_data;

/* Forward declarations */
PRIVATE mesh_material_shading_property      _collada_mesh_generator_get_mesh_material_shading_property_from_collada_data_shading_factor_item(               collada_data_shading_factor_item item);
PRIVATE mesh_material_texture_filtering     _collada_mesh_generator_get_mesh_material_texture_filtering_from_collada_data_sampler_filter    (               _collada_data_sampler_filter     filter);
PRIVATE void                                _collada_mesh_generator_configure_mesh_material_from_effect                                     (__in __notnull ogl_context                      context,
                                                                                                                                             __in __notnull collada_data_effect              effect,
                                                                                                                                             __in __notnull mesh_material                    material);
PRIVATE _collada_mesh_generator_input_data* _collada_mesh_generator_generate_input_data                                                     (__in __notnull ogl_context                      context,
                                                                                                                                             __in __notnull collada_data                     data,
                                                                                                                                             __in __notnull collada_data_geometry            geometry,
                                                                                                                                             __in           _collada_data_input_type         requested_input_type,
                                                                                                                                             __in           unsigned int                     n_polylist);
PRIVATE collada_data_effect                 _collada_mesh_generator_get_geometry_effect                                                     (__in __notnull collada_data_geometry            geometry,
                                                                                                                                             __in           collada_data_polylist            polylist);
PRIVATE mesh_material                       _collada_mesh_generator_get_geometry_material                                                   (__in __notnull ogl_context                      context,
                                                                                                                                             __in __notnull collada_data_geometry            geometry,
                                                                                                                                             __in           unsigned int                     n_polylist);
PRIVATE collada_data_polylist               _collada_mesh_generator_get_geometry_polylist                                                   (__in __notnull collada_data_geometry            geometry,
                                                                                                                                             __in           unsigned int                     n_polylist);
PRIVATE void                                _collada_mesh_generator_get_input_type_properties                                               (__in           _collada_data_input_type         input_type,
                                                                                                                                             __out_opt      unsigned int*                    out_component_size,
                                                                                                                                             __out_opt      unsigned int*                    out_n_components);
PRIVATE mesh_layer_data_stream_type         _collada_mesh_generator_get_mesh_layer_data_stream_type_for_collada_data_input_type             (__in           _collada_data_input_type         input_type);


/** TODO */
PRIVATE void _collada_mesh_generator_configure_mesh_material_from_effect(__in __notnull ogl_context         context,
                                                                         __in __notnull collada_data_effect effect,
                                                                         __in __notnull mesh_material       material)
{
    _collada_data_shading       effect_shading              = COLLADA_DATA_SHADING_UNKNOWN;
    collada_data_shading_factor effect_shading_factor       = COLLADA_DATA_SHADING_FACTOR_UNKNOWN;
    int                         effect_shading_item_bitmask = 0;

    collada_data_effect_get_properties(effect,
                                       &effect_shading,
                                       &effect_shading_item_bitmask);

    switch (effect_shading)
    {
        case COLLADA_DATA_SHADING_LAMBERT:
        {
            const mesh_material_shading shading_type = MESH_MATERIAL_SHADING_LAMBERT;

            mesh_material_set_property(material,
                                       MESH_MATERIAL_PROPERTY_SHADING,
                                      &shading_type);

            break;
        }

        case COLLADA_DATA_SHADING_PHONG:
        {
            const mesh_material_shading shading_type = MESH_MATERIAL_SHADING_PHONG;

            mesh_material_set_property(material,
                                       MESH_MATERIAL_PROPERTY_SHADING,
                                      &shading_type);

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unrecognized effect shading [%d]", effect_shading);
        }
    } /* switch (effect_shading) */

    /* Iterate over items */
    for (int n_shading_item = 0;
             n_shading_item < COLLADA_DATA_SHADING_FACTOR_ITEM_COUNT;
           ++n_shading_item)
    {
        /* Is this item toggled on? */
        const int effect_shading_item_value = 1 << n_shading_item;

        if (effect_shading_item_bitmask & effect_shading_item_value)
        {
            /* How does it correspond to mesh_material representation? */
            bool                           can_continue  = true;
            mesh_material_shading_property item_property = _collada_mesh_generator_get_mesh_material_shading_property_from_collada_data_shading_factor_item( (collada_data_shading_factor_item) effect_shading_item_value);

            can_continue = (item_property != MESH_MATERIAL_SHADING_PROPERTY_UNKNOWN);

            if (can_continue)
            {
                collada_data_effect_get_shading_factor_item_properties(effect,
                                                                       (collada_data_shading_factor_item) effect_shading_item_value,
                                                                       &effect_shading_factor);

                switch (effect_shading_factor)
                {
                    case COLLADA_DATA_SHADING_FACTOR_FLOAT:
                    {
                        float shading_factor_float;

                        collada_data_effect_get_shading_factor_item_float_properties(effect,
                                                                                     (collada_data_shading_factor_item) effect_shading_item_value,
                                                                                    &shading_factor_float);

                        mesh_material_set_shading_property_to_float(material,
                                                                    item_property,
                                                                    shading_factor_float);

                        break;
                    }

                    case COLLADA_DATA_SHADING_FACTOR_TEXTURE:
                    {
                        collada_data_image           shading_factor_image           = NULL;
                        system_hashed_ansi_string    shading_factor_image_file_name = NULL;
                        system_hashed_ansi_string    shading_factor_image_name      = NULL;
                        ogl_texture                  shading_factor_texture         = NULL;
                        _collada_data_sampler_filter shading_factor_mag_filter      = COLLADA_DATA_SAMPLER_FILTER_UNKNOWN;
                        _collada_data_sampler_filter shading_factor_min_filter      = COLLADA_DATA_SAMPLER_FILTER_UNKNOWN;
                        system_hashed_ansi_string    texcoord_name                  = NULL;

                        collada_data_effect_get_shading_factor_item_texture_properties(effect,
                                                                                       (collada_data_shading_factor_item) effect_shading_item_value,
                                                                                      &shading_factor_image,
                                                                                      &shading_factor_mag_filter,
                                                                                      &shading_factor_min_filter,
                                                                                      &texcoord_name);

                        collada_data_image_get_properties(shading_factor_image,
                                                         &shading_factor_image_name,
                                                         &shading_factor_image_file_name,
                                                          NULL,  /* out_file_name_with_path */
                                                          NULL); /* out_requires_mipmaps */

                        /* Identify ogl_texture with the specified name */
                        ogl_context_textures current_context_textures = NULL;

                        ogl_context_get_property(context,
                                                 OGL_CONTEXT_PROPERTY_TEXTURES,
                                                &current_context_textures);

                        ASSERT_ALWAYS_SYNC(current_context_textures != NULL, "Texture manager is NULL");

                        shading_factor_texture = ogl_context_textures_get_texture_by_name(current_context_textures,
                                                                                          shading_factor_image_file_name);
                        ASSERT_ALWAYS_SYNC(shading_factor_texture != NULL,
                                           "Could not retrieve ogl_texture instance for image [%s]",
                                           system_hashed_ansi_string_get_buffer(shading_factor_image_name) );

                        /* Texcoord does not necessarily have to be TEX0 - this will be especially true for meshes where
                         * there are more than just a single UV set (eg. for different shading items).
                         *
                         * The test mesh I'm using at the moment does not feature this so I'm leaving this as homework
                         * when the need arises.
                         */
                        ASSERT_ALWAYS_SYNC(system_hashed_ansi_string_is_equal_to_raw_string(texcoord_name, "TEX0"), "TODO");

                        /* We now know everything we need to configure the material */
                        mesh_material_set_shading_property_to_texture(material,
                                                                      item_property,
                                                                      shading_factor_texture,
                                                                      0, /* always use base mip-map */
                                                                      _collada_mesh_generator_get_mesh_material_texture_filtering_from_collada_data_sampler_filter(shading_factor_mag_filter),
                                                                      _collada_mesh_generator_get_mesh_material_texture_filtering_from_collada_data_sampler_filter(shading_factor_min_filter) );

                        break;
                    }

                    case COLLADA_DATA_SHADING_FACTOR_VEC4:
                    {
                        float shading_factor_float4[4];

                        collada_data_effect_get_shading_factor_item_float4_properties(effect,
                                                                                      (collada_data_shading_factor_item) effect_shading_item_value,
                                                                                      shading_factor_float4);

                        mesh_material_set_shading_property_to_vec4(material,
                                                                   item_property,
                                                                   shading_factor_float4);

                        break;
                    }

                    default:
                    {
                        ASSERT_ALWAYS_SYNC(false,
                                           "Unrecognized COLLADA effect shading factor [%d]",
                                           effect_shading_factor);
                    }
                } /* switch (effect_shading_factor) */
            }
        } /* if (effect_shading_item_bitmask & (1 << n_shading_item) ) */
    } /* for (all shading items) */
}

/** TODO */
PRIVATE _collada_mesh_generator_input_data* _collada_mesh_generator_generate_input_data(__in __notnull ogl_context              context,
                                                                                        __in __notnull collada_data             data,
                                                                                        __in __notnull collada_data_geometry    geometry,
                                                                                        __in           _collada_data_input_type requested_input_type,
                                                                                        __in           unsigned int             n_polylist)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(geometry != NULL, "Geometry is NULL");

    /* Retrieve geometry properties */
    collada_data_geometry_mesh          geometry_mesh        = NULL;
    unsigned int                        n_material_instances = 0;
    unsigned int                        n_polylists          = 0;
    _collada_mesh_generator_input_data* result               = NULL;

    collada_data_geometry_get_properties(geometry,
                                         NULL, /* out_name */
                                         &geometry_mesh,
                                         &n_material_instances);

    collada_data_geometry_mesh_get_property(geometry_mesh,
                                            COLLADA_DATA_GEOMETRY_MESH_PROPERTY_N_POLYLISTS,
                                           &n_polylists);

    ASSERT_DEBUG_SYNC(n_polylists != 0, "No polylists defined for requested geometry");
    if (n_polylists != 0)
    {
        /* Look for polylist's input that defines offset and source data for the requested input type */
        collada_data_polylist current_polylist = NULL;

        collada_data_geometry_mesh_get_polylist(geometry_mesh,
                                                n_polylist,
                                               &current_polylist);

        if (current_polylist == NULL)
        {
            ASSERT_DEBUG_SYNC(false, "Could not find zeroth polylist");

            goto end;
        }

        /* Retrieve polylist properties */
        system_hashed_ansi_string material_symbol_name    = NULL;
              unsigned int        n_polylist_indices      = 0;
              unsigned int        n_polylist_inputs       = 0;
        const unsigned int*       polylist_index_data     = NULL;
              unsigned int        polylist_index_data_max = -1;
              unsigned int        polylist_index_data_min = -1;

        collada_data_polylist_get_property(current_polylist,
                                           COLLADA_DATA_POLYLIST_PROPERTY_MATERIAL_SYMBOL_NAME,
                                          &material_symbol_name);
        collada_data_polylist_get_property(current_polylist,
                                           COLLADA_DATA_POLYLIST_PROPERTY_N_INDICES,
                                          &n_polylist_indices);
        collada_data_polylist_get_property(current_polylist,
                                           COLLADA_DATA_POLYLIST_PROPERTY_N_INPUTS,
                                          &n_polylist_inputs);
        collada_data_polylist_get_property(current_polylist,
                                           COLLADA_DATA_POLYLIST_PROPERTY_INDEX_DATA,
                                          &polylist_index_data);
        collada_data_polylist_get_property(current_polylist,
                                           COLLADA_DATA_POLYLIST_PROPERTY_INDEX_MAXIMUM_VALUE,
                                          &polylist_index_data_max);
        collada_data_polylist_get_property(current_polylist,
                                           COLLADA_DATA_POLYLIST_PROPERTY_INDEX_MINIMUM_VALUE,
                                          &polylist_index_data_min);

        /* Identify material instance */
        collada_data_material                           material             = NULL;
        collada_data_effect                             material_effect      = _collada_mesh_generator_get_geometry_effect               (geometry, current_polylist);
        collada_data_scene_graph_node_material_instance material_instance    = collada_data_geometry_get_material_instance_by_symbol_name(geometry, material_symbol_name);

        /* Identify source of requested input type */
        collada_data_input requested_input = NULL;

        collada_data_polylist_get_input(current_polylist, requested_input_type, &requested_input);

        if (requested_input == NULL)
        {
            /* Nope */
            goto end;
        }

        /* Looks good! Allocate the descriptor before we start */
        result = new (std::nothrow) _collada_mesh_generator_input_data;

        ASSERT_DEBUG_SYNC(result != NULL, "Out of memory");
        if (result == NULL)
        {
            goto end;
        }

        /* Allocate enough space to hold array & index data */
        result->n_indices = n_polylist_indices / n_polylist_inputs;
        result->type      = requested_input_type;

        /* Retrieve input properties we'll need to construct array & index data */
        unsigned int n_sets = 0;

        collada_data_input_get_properties(requested_input,
                                          0, /* n_set */
                                         &n_sets,
                                          NULL,  /* out_offset */
                                          NULL,  /* out_source */
                                          NULL); /* out_type */

        /* More than a single set is only expected for texcoords at the moment .. */
        unsigned int index_counter         = 0;
        unsigned int total_array_data_size = 0;

        ASSERT_DEBUG_SYNC(requested_input_type == COLLADA_DATA_INPUT_TYPE_TEXCOORD && n_sets >= 1 ||
                                                                                      n_sets == 1,
                          "Unsupported number of sets used for non-texcoord input type.");

        for (unsigned int input_set = 0; input_set < n_sets; ++input_set)
        {
            collada_data_source               input_source                 = NULL;
            collada_data_float_array          source_data_float_array      = NULL;
            collada_data_shading_factor_item  texcoord_shading_factor_item = COLLADA_DATA_SHADING_FACTOR_ITEM_UNKNOWN;

            collada_data_input_get_properties(requested_input,
                                              input_set,
                                              NULL,         /* out_n_sets */
                                              NULL,         /* out_offset */
                                             &input_source,
                                              NULL);        /* out_type */

            collada_data_source_get_source_float_data(input_source,
                                                     &source_data_float_array);

            /* Make sure n_components makes sense */
            unsigned int expected_source_n_components = 0;

            switch (requested_input_type)
            {
                case COLLADA_DATA_INPUT_TYPE_NORMAL:   expected_source_n_components = 3; break;
                case COLLADA_DATA_INPUT_TYPE_TEXCOORD: expected_source_n_components = 2; break;
                case COLLADA_DATA_INPUT_TYPE_VERTEX:   expected_source_n_components = 3; break;

                default:
                {
                    ASSERT_DEBUG_SYNC(false, "Input type not supported");
                }
            } /* switch (requested_input_type) */

            /* Retrieve source data float array properties */
            const float* source_data_float_array_data         = NULL;
            unsigned int source_data_float_array_n_components = 0;
            unsigned int source_data_float_array_n_values     = 0;

            collada_data_float_array_get_property(source_data_float_array,
                                                  COLLADA_DATA_FLOAT_ARRAY_PROPERTY_DATA,
                                                 &source_data_float_array_data);
            collada_data_float_array_get_property(source_data_float_array,
                                                  COLLADA_DATA_FLOAT_ARRAY_PROPERTY_N_COMPONENTS,
                                                 &source_data_float_array_n_components);
            collada_data_float_array_get_property(source_data_float_array,
                                                  COLLADA_DATA_FLOAT_ARRAY_PROPERTY_N_VALUES,
                                                 &source_data_float_array_n_values);

            ASSERT_DEBUG_SYNC(expected_source_n_components == source_data_float_array_n_components,
                              "Unsupported amount of components used for input");

            /* If this is a texcoord input type, it could be describing multiple texture inputs
             * (ambient, diffuse, etc). Check the vertex bindings */
            if (requested_input_type == COLLADA_DATA_INPUT_TYPE_TEXCOORD)
            {
                unsigned int material_binding_index = -1;

                collada_data_geometry_get_material_binding_index_by_input_semantic_name(material_instance,
                                                                                        input_set,
                                                                                        system_hashed_ansi_string_create("TEXCOORD"),
                                                                                        &material_binding_index);

                if (material_binding_index == -1)
                {
                    ASSERT_DEBUG_SYNC(false, "Could not retrieve material binding index");
                } /* if (material_binding_index != -1) */
                else
                {
                    collada_data_geometry_material_binding material_binding        = NULL;
                    system_hashed_ansi_string              material_input_semantic = NULL;
                    system_hashed_ansi_string              material_semantic_name  = NULL;

                    collada_data_scene_graph_node_material_instance_get_material_binding(material_instance,
                                                                                         material_binding_index,
                                                                                        &material_binding);

                    collada_data_geometry_material_binding_get_properties(material_binding,
                                                                         &material_input_semantic,
                                                                          NULL, /* out_input_set */
                                                                         &material_semantic_name);

                    /* Find a texture that uses the texture coordinate set */
                    texcoord_shading_factor_item = collada_data_effect_get_shading_factor_item_by_texcoord(material_effect,
                                                                                                           material_semantic_name);

                    if (texcoord_shading_factor_item == COLLADA_DATA_SHADING_FACTOR_ITEM_UNKNOWN)
                    {
                        ASSERT_DEBUG_SYNC(false, "Could not associate an UV set with actual effect");
                    } /* if (shading_factor_item == COLLADA_DATA_SHADING_FACTOR_ITEM_UNKNOWN) */
                }
            } /* if (requested_input_type == COLLADA_DATA_INPUT_TYPE_TEXCOORD) */

            /* Form the stream descriptor */
            _collada_mesh_generator_input_data_stream* stream_ptr = new (std::nothrow) _collada_mesh_generator_input_data_stream;

            ASSERT_DEBUG_SYNC(stream_ptr != NULL, "Out of memory");
            if (stream_ptr == NULL)
            {
                goto end;
            }

            stream_ptr->array_data      = source_data_float_array_data;
            stream_ptr->array_data_size = source_data_float_array_n_values * sizeof(float);
            stream_ptr->index_delta     = index_counter;

            if (requested_input_type == COLLADA_DATA_INPUT_TYPE_TEXCOORD)
            {
                stream_ptr->texcoord_shading_property = _collada_mesh_generator_get_mesh_material_shading_property_from_collada_data_shading_factor_item(texcoord_shading_factor_item);
            }

            /* Extract the index data */
            int current_index_offset = 0;

            collada_data_input_get_properties(requested_input,
                                              input_set,
                                              NULL,        /* out_n_sets */
                                              &current_index_offset,
                                              NULL,         /* out_source */
                                              NULL);        /* out_type */

            /* Allocate space for index data */
            stream_ptr->index_data = new (std::nothrow) unsigned int[result->n_indices];

            if (stream_ptr->index_data == NULL)
            {
                ASSERT_DEBUG_SYNC(false, "Out of memory");

                goto end;
            }

            /* Retrieve index data */
            for (unsigned int n_index = 0;
                              n_index < result->n_indices;
                            ++n_index, current_index_offset += n_polylist_inputs)
            {
                stream_ptr->index_data[n_index] = polylist_index_data[current_index_offset];
            } /* for (all indices) */

            stream_ptr->max_index = polylist_index_data_max;
            stream_ptr->min_index = polylist_index_data_min;

            /* Store the stream */
            system_resizable_vector_push(result->streams, stream_ptr);

            /* Update offset monitors */
            total_array_data_size += stream_ptr->array_data_size;
            index_counter         += result->n_indices;
        } /* for (unsigned int input_set = 0; input_set < n_sets; ++input_set) */

        /* Combine data from all streams into a single array data */
        unsigned char* combined_array_data = new unsigned char[total_array_data_size];
        unsigned char* traveller_ptr       = combined_array_data;

        ASSERT_DEBUG_SYNC(combined_array_data != NULL, "Out of memory");
        if (combined_array_data == NULL)
        {
            goto end;
        }

        for (unsigned int input_set = 0;
                          input_set < n_sets;
                        ++input_set)
        {
            _collada_mesh_generator_input_data_stream* stream_ptr = NULL;

            if (system_resizable_vector_get_element_at(result->streams,
                                                       input_set,
                                                      &stream_ptr) )
            {
                memcpy(traveller_ptr,
                       stream_ptr->array_data,
                       stream_ptr->array_data_size);

                traveller_ptr += stream_ptr->array_data_size;
            }
            else
            {
                ASSERT_DEBUG_SYNC(false, "Could not retrieve stream descriptor");
            }
        }

        result->array_data      = combined_array_data;
        result->array_data_size = total_array_data_size;
    } /* if (n_polylists != 0) */

end:
    return result;
}

/** TODO */
PRIVATE collada_data_effect _collada_mesh_generator_get_geometry_effect(__in __notnull collada_data_geometry geometry,
                                                                        __in           collada_data_polylist polylist)
{
    /* Retrieve polylist properties */
    collada_data_effect material_effect      = NULL;
    mesh_material       result_mesh_material = NULL;

    system_hashed_ansi_string material_symbol_name = NULL;
          unsigned int        n_polylist_indices   = 0;
          unsigned int        n_polylist_inputs    = 0;
    const unsigned int*       polylist_index_data  = NULL;

    collada_data_polylist_get_property(polylist,
                                       COLLADA_DATA_POLYLIST_PROPERTY_MATERIAL_SYMBOL_NAME,
                                      &material_symbol_name);
    collada_data_polylist_get_property(polylist,
                                       COLLADA_DATA_POLYLIST_PROPERTY_N_INDICES,
                                      &n_polylist_indices);
    collada_data_polylist_get_property(polylist,
                                       COLLADA_DATA_POLYLIST_PROPERTY_N_INPUTS,
                                      &n_polylist_inputs);
    collada_data_polylist_get_property(polylist,
                                       COLLADA_DATA_POLYLIST_PROPERTY_INDEX_DATA,
                                      &polylist_index_data);

    /* Retrieve the effect associated with the material bound to the polylist */
    collada_data_material                           material             = NULL;
    collada_data_scene_graph_node_material_instance material_instance    = collada_data_geometry_get_material_instance_by_symbol_name(geometry, material_symbol_name);

    if (material_instance == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not find descriptor of material instance with symbol [%s]",
                           system_hashed_ansi_string_get_buffer(material_symbol_name) );
    } /* if (material_instance == NULL) */
    else
    {
        /* Retrieve effect assigned to the material. Effect tells us how the shading should
         * actually be performed.
         */
        collada_data_scene_graph_node_material_instance_get_properties(material_instance,
                                                                      &material,
                                                                       NULL,  /* out_symbol_name */
                                                                       NULL); /* out_n_bindings */

        if (material == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not retrieve material associated with material instance [%s]",
                               system_hashed_ansi_string_get_buffer(material_symbol_name) );
        } /* if (material == NULL) */
        else
        {
            collada_data_material_get_properties(material,
                                                 &material_effect,
                                                 NULL,  /* out_id */
                                                 NULL); /* out_name */

            if (material_effect == NULL)
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Could not retrieve effect associated with material instance [%s]",
                                   system_hashed_ansi_string_get_buffer(material_symbol_name) );
            } /* if (material_effect == NULL) */
        }
    }

    return material_effect;
}

/** TODO.
 *
 *  It is caller's responsibility to release the mesh_material object
 *  when no longer needed.
 **/
PRIVATE mesh_material _collada_mesh_generator_get_geometry_material(__in __notnull ogl_context           context,
                                                                    __in __notnull collada_data_geometry geometry,
                                                                    __in           unsigned int          n_polylist)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(geometry != NULL, "Geometry is NULL");

    /* Retrieve geometry properties */
    collada_data_polylist current_polylist     = NULL;
    mesh_material         result_mesh_material = NULL;

    current_polylist = _collada_mesh_generator_get_geometry_polylist(geometry,
                                                                     n_polylist);

    ASSERT_DEBUG_SYNC(current_polylist != NULL, "Could not retrievfe requested polylist");
    if (current_polylist != NULL)
    {
        /* Retrieve polylist properties */
        system_hashed_ansi_string material_symbol_name = NULL;

        collada_data_polylist_get_property(current_polylist,
                                           COLLADA_DATA_POLYLIST_PROPERTY_MATERIAL_SYMBOL_NAME,
                                          &material_symbol_name);

        /* Identify material instance */
        collada_data_material     material                    = NULL;
        collada_data_effect       material_effect             = _collada_mesh_generator_get_geometry_effect(geometry,
                                                                                                            current_polylist);
        system_hashed_ansi_string material_effect_uv_map_name = NULL;

        collada_data_effect_get_property(material_effect,
                                         COLLADA_DATA_EFFECT_PROPERTY_UV_MAP_NAME,
                                        &material_effect_uv_map_name);

        /* Create a mesh_material instance out of the effect */
        result_mesh_material = mesh_material_create(material_symbol_name,
                                                    context,
                                                    NULL); /* object_manager_path */

        mesh_material_set_property(result_mesh_material,
                                   MESH_MATERIAL_PROPERTY_UV_MAP_NAME,
                                  &material_effect_uv_map_name);

        _collada_mesh_generator_configure_mesh_material_from_effect(context,
                                                                    material_effect,
                                                                    result_mesh_material);
    } /* if (n_polylists != 0) */

    return result_mesh_material;
}

/** TODO */
PRIVATE collada_data_polylist _collada_mesh_generator_get_geometry_polylist(__in __notnull collada_data_geometry geometry,
                                                                            __in           unsigned int          n_polylist)
{
    /* Retrieve geometry properties */
    collada_data_geometry_mesh geometry_mesh   = NULL;

    collada_data_geometry_get_properties(geometry,
                                         NULL, /* out_name */
                                        &geometry_mesh,
                                         NULL); /* out_n_material_instances */

    /* Look for requested polylist */
    collada_data_polylist result_polylist = NULL;

    collada_data_geometry_mesh_get_polylist(geometry_mesh,
                                            n_polylist,
                                           &result_polylist);

    if (result_polylist == NULL)
    {
        ASSERT_DEBUG_SYNC(false, "Could not find requested polylist");
    }

    return result_polylist;
}

/** TODO */
PRIVATE void _collada_mesh_generator_get_input_type_properties(__in      _collada_data_input_type input_type,
                                                               __out_opt unsigned int*            out_component_size,
                                                               __out_opt unsigned int*            out_n_components)
{
    unsigned int result_component_size = 0;
    unsigned int result_n_components   = 0;

    switch(input_type)
    {
        case COLLADA_DATA_INPUT_TYPE_NORMAL:
        case COLLADA_DATA_INPUT_TYPE_VERTEX:
        {
            result_component_size = sizeof(float);
            result_n_components   = 3;

            break;
        }

        case COLLADA_DATA_INPUT_TYPE_TEXCOORD:
        {
            result_component_size = sizeof(float);
            result_n_components   = 2;

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unrecognized COLLADA data input type");
        }
    } /* switch (input_type) */

    if (out_component_size != NULL)
    {
        *out_component_size = result_component_size;
    }

    if (out_n_components != NULL)
    {
        *out_n_components = result_n_components;
    }
}

/** TODO */
PRIVATE mesh_layer_data_stream_type _collada_mesh_generator_get_mesh_layer_data_stream_type_for_collada_data_input_type(__in _collada_data_input_type input_type)
{
    mesh_layer_data_stream_type result = MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN;

    switch (input_type)
    {
        case COLLADA_DATA_INPUT_TYPE_NORMAL:   result = MESH_LAYER_DATA_STREAM_TYPE_NORMALS;   break;
        case COLLADA_DATA_INPUT_TYPE_TEXCOORD: result = MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS; break;
        case COLLADA_DATA_INPUT_TYPE_VERTEX:   result = MESH_LAYER_DATA_STREAM_TYPE_VERTICES;  break;

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unrecognized input type");
        }
    }

    return result;
}

/** TODO */
PRIVATE mesh_material_shading_property _collada_mesh_generator_get_mesh_material_shading_property_from_collada_data_shading_factor_item(collada_data_shading_factor_item item)
{
    mesh_material_shading_property result = MESH_MATERIAL_SHADING_PROPERTY_UNKNOWN;

    switch (item)
    {
        case COLLADA_DATA_SHADING_FACTOR_ITEM_AMBIENT:
        {
            result = MESH_MATERIAL_SHADING_PROPERTY_AMBIENT;

            break;
        }

        case COLLADA_DATA_SHADING_FACTOR_ITEM_DIFFUSE:
        {
            result = MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE;

            break;
        }

        case COLLADA_DATA_SHADING_FACTOR_ITEM_LUMINOSITY:
        {
            result = MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY;

            break;
        }

        case COLLADA_DATA_SHADING_FACTOR_ITEM_SHININESS:
        {
            result = MESH_MATERIAL_SHADING_PROPERTY_SHININESS;

            break;
        }

        case COLLADA_DATA_SHADING_FACTOR_ITEM_SPECULAR:
        {
            result = MESH_MATERIAL_SHADING_PROPERTY_SPECULAR;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized COLLADA data shading factor item");
        }
    } /* switch (item) */

    return result;
}

/** TODO */
PRIVATE mesh_material_texture_filtering _collada_mesh_generator_get_mesh_material_texture_filtering_from_collada_data_sampler_filter(_collada_data_sampler_filter filter)
{
    mesh_material_texture_filtering result = MESH_MATERIAL_TEXTURE_FILTERING_UNKNOWN;

    switch (filter)
    {
        case COLLADA_DATA_SAMPLER_FILTER_NEAREST:                result = MESH_MATERIAL_TEXTURE_FILTERING_NEAREST;         break;
        case COLLADA_DATA_SAMPLER_FILTER_NEAREST_MIPMAP_LINEAR:  result = MESH_MATERIAL_TEXTURE_FILTERING_NEAREST_LINEAR;  break;
        case COLLADA_DATA_SAMPLER_FILTER_NEAREST_MIPMAP_NEAREST: result = MESH_MATERIAL_TEXTURE_FILTERING_NEAREST_NEAREST; break;
        case COLLADA_DATA_SAMPLER_FILTER_LINEAR:                 result = MESH_MATERIAL_TEXTURE_FILTERING_LINEAR;          break;
        case COLLADA_DATA_SAMPLER_FILTER_LINEAR_MIPMAP_NEAREST:  result = MESH_MATERIAL_TEXTURE_FILTERING_LINEAR_NEAREST;  break;
        case COLLADA_DATA_SAMPLER_FILTER_LINEAR_MIPMAP_LINEAR:   result = MESH_MATERIAL_TEXTURE_FILTERING_LINEAR_LINEAR;   break;

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unrecognized COLLADA data sampler filter");
        }
    } /* switch (filter) */

    return result;
}


/** Please see header for specification */
PUBLIC mesh collada_mesh_generator_create(__in __notnull ogl_context  context,
                                          __in __notnull collada_data data,
                                          __in __notnull unsigned int n_geometry)
{
    collada_data_geometry geometry = NULL;
    mesh                  result   = NULL;

    collada_data_get_geometry(data, n_geometry, &geometry);

    if (geometry == NULL)
    {
        ASSERT_ALWAYS_SYNC(false, "COLLADA geometry at index [%d] not found", n_geometry);

        goto end;
    }

    /* Retrieve geometry properties */
    system_hashed_ansi_string geometry_id = NULL;

    collada_data_geometry_get_property(geometry,
                                       COLLADA_DATA_GEOMETRY_PROPERTY_ID,
                                       &geometry_id);

    /* Name is potentially NULL. If this is the case, come with our own one */
    static int name_counter = 1;

    if (system_hashed_ansi_string_get_length(geometry_id) == 0)
    {
        /* Use enough characters to fit really large numbers */
        char temp[24];

        sprintf_s(temp, "Emerald%d", name_counter++);

        geometry_id = system_hashed_ansi_string_create(temp);
    }

    /* Instantiate new mesh */
    result = mesh_create(MESH_SAVE_SUPPORT, geometry_id);

    if (result == NULL)
    {
        ASSERT_ALWAYS_SYNC(false, "Could not create a new Emerald mesh");

        goto end;
    }

    /* Iterate over all declared polylists. Polylists define index data (referring to
     * array data which can differ between polylists), and bind it to specific
     * material, which more or less corresponds to what mesh layers are in Emerald
     */
    collada_data_geometry_mesh geometry_mesh                 = NULL;
    bool                       has_normals_data_been_defined = false;
    unsigned int               n_polylists                   = 0;

    collada_data_geometry_get_property(geometry,
                                       COLLADA_DATA_GEOMETRY_PROPERTY_MESH,
                                       &geometry_mesh);

    collada_data_geometry_mesh_get_property(geometry_mesh,
                                            COLLADA_DATA_GEOMETRY_MESH_PROPERTY_N_POLYLISTS,
                                           &n_polylists);

    for (unsigned int n_polylist = 0;
                      n_polylist < n_polylists;
                    ++n_polylist)
    {
        /* Add a new layer */
        mesh_layer_id current_layer_id = mesh_add_layer(result);

        /* Extract data only for the input types we're interested in. Note that "mesh" can handle any data streams,
         * so if you need any input type that's not currently implemented, just expand the array.
         */
        const _collada_data_input_type input_types[] =
        {
            /* DO NOT change the order! */
            COLLADA_DATA_INPUT_TYPE_NORMAL,
            COLLADA_DATA_INPUT_TYPE_TEXCOORD,
            COLLADA_DATA_INPUT_TYPE_VERTEX
        };
        const unsigned int                  n_input_types             = sizeof(input_types) / sizeof(input_types[0]);
        const unsigned int                  n_normal_input_type       = 0; /* corresponds to index of COLLADA_DATA_INPUT_TYPE_NORMAL value in input_types */
        const unsigned int                  n_texcoord_input_type     = 1; /* corresponds to index of COLLADA_DATA_INPUT_TYPE_TEXCOORD value in input_types */
        _collada_mesh_generator_input_data* input_data[n_input_types] = {NULL};

        for (unsigned int n_input_type = 0; n_input_type < n_input_types; ++n_input_type)
        {
            input_data[n_input_type] = _collada_mesh_generator_generate_input_data(context,
                                                                                   data,
                                                                                   geometry,
                                                                                   input_types[n_input_type],
                                                                                   n_polylist);
        }

        /* Create a mesh_material instance for the polygon list */
        mesh_material polylist_material = _collada_mesh_generator_get_geometry_material(context,
                                                                                        geometry,
                                                                                        n_polylist);

        /* Bind the found input data to the layer */
        unsigned int n_vertex_indices = 0;

        for (unsigned int n_input_type = 0; n_input_type < n_input_types; ++n_input_type)
        {
            if (input_data[n_input_type] != NULL)
            {
                unsigned int                   component_size = 0;
                const _collada_data_input_type input_type     = input_types[n_input_type];
                unsigned int                   n_components   = 0;
                mesh_layer_data_stream_type    stream_type    = _collada_mesh_generator_get_mesh_layer_data_stream_type_for_collada_data_input_type(input_type);

                _collada_mesh_generator_get_input_type_properties(input_type,
                                                                  &component_size,
                                                                  &n_components);

                mesh_add_layer_data_stream(result,
                                           current_layer_id,
                                           stream_type,
                                           input_data[n_input_type]->array_data_size / component_size / n_components,
                                           input_data[n_input_type]->array_data);

                if (input_type == COLLADA_DATA_INPUT_TYPE_VERTEX)
                {
                    n_vertex_indices = input_data[n_input_type]->n_indices;
                }
            }
        } /* for (all input types) */

        /* Add a new layer pass */
        ASSERT_DEBUG_SYNC(n_vertex_indices != 0, "No vertex indices found");

        mesh_layer_pass_id pass_id = mesh_add_layer_pass(result,
                                                         current_layer_id,
                                                         polylist_material,
                                                         n_vertex_indices);

        for (unsigned int n_input_type = 0; n_input_type < n_input_types; ++n_input_type)
        {
            if (input_data[n_input_type] != NULL)
            {
                const _collada_data_input_type input_type  = input_types[n_input_type];
                const unsigned int             n_streams   = system_resizable_vector_get_amount_of_elements                                     (input_data[n_input_type]->streams);
                mesh_layer_data_stream_type    stream_type = _collada_mesh_generator_get_mesh_layer_data_stream_type_for_collada_data_input_type(input_type);

                for (unsigned int n_set = 0; n_set < n_streams; ++n_set)
                {
                    _collada_mesh_generator_input_data_stream* stream_ptr = NULL;

                    if (system_resizable_vector_get_element_at(input_data[n_input_type]->streams, n_set, &stream_ptr) )
                    {
                        mesh_add_layer_pass_index_data(result,
                                                       current_layer_id,
                                                       pass_id,
                                                       stream_type,
                                                       n_set,
                                                       stream_ptr->index_data,
                                                       stream_ptr->min_index,
                                                       stream_ptr->max_index);
                    }
                    else
                    {
                        ASSERT_ALWAYS_SYNC(false, "Could not retrieve stream descriptor at index [%d]", n_set);
                    }
                }

                /* Mark normals data as used by the mesh */
                if (n_input_type == n_normal_input_type)
                {
                    has_normals_data_been_defined = true;
                }

                /* Good to release the helper instance */
                delete input_data[n_input_type];

                input_data[n_input_type] = NULL;
            }

            /* Sanity check: for cases where there are multiple polylists declared for a single mesh,
             *               make sure all layers either define normals data or they don't. Blended
             *               cases have *not* been tested and are likely to crash Emerald
             */
            if (n_input_type             == n_normal_input_type &&
                input_data[n_input_type] == NULL)
            {
                ASSERT_DEBUG_SYNC(!has_normals_data_been_defined,
                                  "Some polylists of a geometry mesh declare normals data, and some don't");
            }
        } /* for (all supported COLLADA data input types) */
    } /* for (all polylists) */

    /* If no data on normals is in the COLLADA container, we need to generate them
     * manually..
     */
    if (!has_normals_data_been_defined)
    {
        /* If the caching mode is enabled, the data may have already been stashed */
        system_hashed_ansi_string blob_name              = NULL;
        bool                      has_loaded_normal_data = false;
        bool                      is_caching_enabled     = false;

        collada_data_get_property(data,
                                  COLLADA_DATA_PROPERTY_CACHE_BINARY_BLOBS_MODE,
                                 &is_caching_enabled);

        if (is_caching_enabled)
        {
            /* Form the blob name */
            const char* blob_name_strings[] =
            {
                "collada_mesh_",
                system_hashed_ansi_string_get_buffer(geometry_id)
            };
            size_t      n_blob_name_strings = sizeof(blob_name_strings) /
                                              sizeof(blob_name_strings[0]);

            blob_name = system_hashed_ansi_string_create_by_merging_strings(n_blob_name_strings,
                                                                            blob_name_strings);

            /* Is the blob cached? */
            system_file_serializer serializer      = system_file_serializer_create_for_reading(blob_name);
            const void*            serializer_data = NULL;
            
            system_file_serializer_get_property(serializer,
                                                SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE,
                                               &serializer_data);

            if (serializer_data != NULL)
            {
                unsigned int n_result_layers     = 0;
                unsigned int n_serialized_layers = 0;

                mesh_get_property(result,
                                  MESH_PROPERTY_N_LAYERS,
                                 &n_result_layers);

                /* Read the data */
                system_file_serializer_read(serializer,
                                            sizeof(n_serialized_layers),
                                           &n_serialized_layers);

                if (n_result_layers != n_serialized_layers)
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Serialized blob describes [%d] layers, whereas the created mesh object offers [%d] layers",
                                      n_serialized_layers,
                                      n_result_layers);
                } /* if (n_result_layers != n_serialized_layers) */
                else
                {
                    unsigned int n_serialized_normals = 0;
                    const void*  serialized_normals   = NULL;

                    for (unsigned int n_layer = 0;
                                      n_layer < n_serialized_layers;
                                    ++n_layer)
                    {
                        /* Read the layer data */
                        unsigned int serializer_current_offset = 0;

                        system_file_serializer_read        (serializer,
                                                            sizeof(n_serialized_normals),
                                                           &n_serialized_normals);
                        system_file_serializer_get_property(serializer,
                                                            SYSTEM_FILE_SERIALIZER_PROPERTY_CURRENT_OFFSET,
                                                           &serializer_current_offset);

                        serialized_normals = (const unsigned char*) serializer_data + serializer_current_offset;

                        /* Carry on and add the layer data */
                        mesh_add_layer_data_stream(result,
                                                   n_layer,
                                                   MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                   n_serialized_normals,
                                                   serialized_normals);

                        system_file_serializer_read(serializer,
                                                    n_serialized_normals * sizeof(float) * 3,
                                                    NULL);
                    }

                    /* Reading complete! */
                    has_loaded_normal_data = true;
                }
            } /* if (serializer_data != NULL) */

            system_file_serializer_release(serializer);
        }

        if (!has_loaded_normal_data)
        {
            /* Apparently not. */
            mesh_generate_normal_data(result);

            /* Cache the blob if needed */
            if (is_caching_enabled)
            {
                /* Create the file serializer */
                system_file_serializer serializer = system_file_serializer_create_for_writing(blob_name);

                /* Store the number of layers */
                unsigned int n_layers = 0;

                mesh_get_property(result,
                                  MESH_PROPERTY_N_LAYERS,
                                 &n_layers);

                system_file_serializer_write(serializer,
                                             sizeof(n_layers),
                                            &n_layers);

                /* Store normal data created for all the layers */
                for (unsigned int n_layer = 0;
                                  n_layer < n_layers;
                                ++n_layer)
                {
                    unsigned int n_normal_items   = 0;
                    const void*   normal_data_ptr = NULL;

                    mesh_get_layer_data_stream_data(result,
                                                    n_layer,
                                                    MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                    &n_normal_items,
                                                    &normal_data_ptr);

                    system_file_serializer_write(serializer,
                                                 sizeof(n_normal_items),
                                                &n_normal_items);
                    system_file_serializer_write(serializer,
                                                 n_normal_items * sizeof(float) * 3,
                                                 normal_data_ptr);
                } /* for (all layers) */

                /* All done */
                system_file_serializer_release(serializer);
            } /* if (is_caching_enabled) */
        } /* if (!has_loaded_normal_data) */
    }
end:
    return result;
}

