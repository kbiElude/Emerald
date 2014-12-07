/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data_effect.h"
#include "collada/collada_data_sampler2D.h"
#include "collada/collada_data_surface.h"
#include "system/system_assertions.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/** TODO */
enum _xml_tree_traversal_node_type
{
    /* uses node_name */
    XML_TREE_TRAVERSAL_NODE_TYPE_NODE,
    /* uses attribute_name, attribute_value, node_name */
    XML_TREE_TRAVERSAL_NODE_TYPE_NODE_WITH_TEXT_ATTRIBUTE_AND_VALUE
};

/** TODO */
typedef struct
{
    _xml_tree_traversal_node_type type;
    const char*                   node_name;

    const char* attribute_name;
    const char* attribute_value;
} _xml_tree_traversal_node;

/** TODO */
typedef struct _collada_data_shading_factor_item_texture
{
    collada_data_image           image;
    _collada_data_sampler_filter mag_filter;
    _collada_data_sampler_filter min_filter;
    system_hashed_ansi_string    texcoord_name;

    _collada_data_shading_factor_item_texture();
} _collada_data_shading_factor_item_texture;

/** Describes a single factor related to shading defined to an effect. */
typedef struct _collada_data_shading_factor_item
{
    void*                       data;
    collada_data_shading_factor type;

     _collada_data_shading_factor_item();
    ~_collada_data_shading_factor_item();
} _collada_data_shading_factor_item;


/** Describes data stored in a <effect>/<profile_COMMON>/<technique> node path.
 *  Can also optionally hold Lightwave-specific data (surface name), if it
 *  is available in source COLLADA data file.
 *
 *  The existing implementation is highly simplified and assumes only a single "common" technique
 *  inside a COMMON profile will ever be defined in COLLADA file. Should a more complex file be
 *  encountered, the implementation will throw an assertion failure.
 **/
typedef struct _collada_data_effect
{
    system_hashed_ansi_string id;
    _collada_data_shading     shading;

    _collada_data_shading_factor_item ambient;
    _collada_data_shading_factor_item diffuse;
    _collada_data_shading_factor_item luminosity;
    _collada_data_shading_factor_item shininess;
    _collada_data_shading_factor_item specular;

    /* Lightwave-specific data, extracted from <extra> sub-node */
    system_hashed_ansi_string uv_map_name;

    _collada_data_effect();
} _collada_data_effect;

/** Forward declarations */
PRIVATE void _collada_data_effect_init_lambert     (__in __notnull _collada_data_effect*               effect_ptr,
                                                    __in __notnull tinyxml2::XMLElement*               element_ptr,
                                                    __in __notnull system_hash64map                    samplers_by_id_map);
PRIVATE void _collada_data_effect_init_shading_item(__in __notnull tinyxml2::XMLElement*               element_ptr,
                                                    __in __notnull _collada_data_shading_factor_item*  result_ptr,
                                                    __in __notnull system_hash64map                    samplers_by_id_map);

/** TODO */
_collada_data_effect::_collada_data_effect()
{
    id          = system_hashed_ansi_string_get_default_empty_string();
    shading     = COLLADA_DATA_SHADING_UNKNOWN;
    uv_map_name = system_hashed_ansi_string_get_default_empty_string();
}

/** TODO */
_collada_data_shading_factor_item::_collada_data_shading_factor_item()
{
    data = NULL;
    type = COLLADA_DATA_SHADING_FACTOR_NONE;
}

/** TODO */
_collada_data_shading_factor_item::~_collada_data_shading_factor_item()
{
    switch (type)
    {
        case COLLADA_DATA_SHADING_FACTOR_NONE:
        {
            ASSERT_DEBUG_SYNC(data == NULL, "Shading factor item type is 'none' but data is not NULL");

            break;
        }

        case COLLADA_DATA_SHADING_FACTOR_FLOAT:
        {
            /* data is a single float */
            delete (float*) data;

            data = NULL;
            break;
        }

        case COLLADA_DATA_SHADING_FACTOR_TEXTURE:
        {
            /* data is a _collada_data_shading_factor_item_texture* instance */
            delete (_collada_data_shading_factor_item_texture*) data;

            data = NULL;
            break;
        }

        case COLLADA_DATA_SHADING_FACTOR_VEC4:
        {
            /* data is a float[4] instance */
            delete (float*) data;

            data = NULL;
            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unrecognized COLLADA data shading factor item");
        }
    } /* switch (type) */
}

/** TODO */
_collada_data_shading_factor_item_texture::_collada_data_shading_factor_item_texture()
{
    image         = NULL;
    mag_filter    = COLLADA_DATA_SAMPLER_FILTER_UNKNOWN;
    min_filter    = COLLADA_DATA_SAMPLER_FILTER_UNKNOWN;
    texcoord_name = system_hashed_ansi_string_get_default_empty_string();
}


/** TODO */
PRIVATE _collada_data_shading_factor_item* _collada_data_get_effect_shading_factor_item(__in __notnull _collada_data_effect*            effect_ptr,
                                                                                        __in           collada_data_shading_factor_item item)
{
    _collada_data_shading_factor_item* result = NULL;

    switch (item)
    {
        case COLLADA_DATA_SHADING_FACTOR_ITEM_AMBIENT:
        {
            result = &effect_ptr->ambient;

            break;
        }

        case COLLADA_DATA_SHADING_FACTOR_ITEM_DIFFUSE:
        {
            result = &effect_ptr->diffuse;

            break;
        }

        case COLLADA_DATA_SHADING_FACTOR_ITEM_LUMINOSITY:
        {
            result = &effect_ptr->luminosity;

            break;
        }

        case COLLADA_DATA_SHADING_FACTOR_ITEM_SHININESS:
        {
            result = &effect_ptr->shininess;

            break;
        }

        case COLLADA_DATA_SHADING_FACTOR_ITEM_SPECULAR:
        {
            result = &effect_ptr->specular;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecopgnized shading factor item [%d]", item);
        }
    }

    return result;
}

/** TODO */
PRIVATE void _collada_data_effect_init_lambert(__in __notnull _collada_data_effect* effect_ptr,
                                               __in __notnull tinyxml2::XMLElement* element_ptr,
                                               __in __notnull system_hash64map      samplers_by_id_map)
{
    effect_ptr->shading = COLLADA_DATA_SHADING_LAMBERT;

    /* Iterate through all children and make create relevant descriptors */
    tinyxml2::XMLElement* child_element_ptr = element_ptr->FirstChildElement();

    while (child_element_ptr != NULL)
    {
        const char* child_name = child_element_ptr->Name();
        ASSERT_DEBUG_SYNC(child_name != NULL,
                          "<lambert> child node name is NULL");

        if (strcmp(child_name, "emission") == 0)
        {
            _collada_data_effect_init_shading_item(child_element_ptr,
                                                   &effect_ptr->luminosity,
                                                    samplers_by_id_map);

        }
        else
        if (strcmp(child_name, "ambient")  == 0)
        {
            _collada_data_effect_init_shading_item(child_element_ptr,
                                                  &effect_ptr->ambient,
                                                   samplers_by_id_map);
        }
        else
        if (strcmp(child_name, "diffuse")  == 0)
        {
            _collada_data_effect_init_shading_item(child_element_ptr,
                                                  &effect_ptr->diffuse,
                                                   samplers_by_id_map);
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized <lambert> node child.");
        }

        /* Move to next child */
        child_element_ptr = child_element_ptr->NextSiblingElement();
    } /* while (child_element_ptr != NULL) */
}


/** TODO */
PRIVATE void _collada_data_effect_init_float_shading_item(__in __notnull _collada_data_shading_factor_item* result_ptr,
                                                          __in           float                              value)
{
    result_ptr->data = new float;
    result_ptr->type = COLLADA_DATA_SHADING_FACTOR_FLOAT;

    ASSERT_ALWAYS_SYNC(result_ptr->data != NULL,
                       "Out of memory");

    *(float*)result_ptr->data = value;
}

/** TODO */
PRIVATE void _collada_data_effect_init_shading_item(__in __notnull tinyxml2::XMLElement*               element_ptr,
                                                    __in __notnull _collada_data_shading_factor_item*  result_ptr,
                                                    __in __notnull system_hash64map                    samplers_by_id_map)
{
    /* We currently support <color> and <texture> sub-nodes */
    tinyxml2::XMLElement* child_element_ptr  = element_ptr->FirstChildElement();
    const char*           child_element_name = child_element_ptr->Name();

    ASSERT_DEBUG_SYNC(child_element_name != NULL,
                      "Child node name is NULL");

    if (strcmp(child_element_name, "color") == 0)
    {
        result_ptr->data = new (std::nothrow) float[4];
        result_ptr->type = COLLADA_DATA_SHADING_FACTOR_VEC4;

        ASSERT_ALWAYS_SYNC(result_ptr->data != NULL,
                           "Out of memory");

        sscanf_s(child_element_ptr->GetText(),
                 "%f %f %f %f",
                 ((float*) result_ptr->data) + 0,
                 ((float*) result_ptr->data) + 1,
                 ((float*) result_ptr->data) + 2,
                 ((float*) result_ptr->data) + 3);
    }
    else
    if (strcmp(child_element_name, "texture") == 0)
    {
        _collada_data_shading_factor_item_texture* new_data = new (std::nothrow) _collada_data_shading_factor_item_texture;

        ASSERT_ALWAYS_SYNC(new_data != NULL, "Out of memory");

        /* Identify the sampler we will be sampling from */
        const char*               sampler_name     = child_element_ptr->Attribute("texture");
        system_hashed_ansi_string sampler_name_has = NULL;
        collada_data_sampler2D    sampler          = NULL;

        ASSERT_DEBUG_SYNC(sampler_name != NULL,
                          "Sampler name is NULL");

        sampler_name_has = system_hashed_ansi_string_create(sampler_name);

        system_hash64map_get(samplers_by_id_map,
                             system_hashed_ansi_string_get_hash(sampler_name_has),
                            &sampler);

        ASSERT_DEBUG_SYNC(sampler != NULL,
                          "Could not find a sampler named [%s]",
                          sampler_name);

        /* Identify texcoord that will be used in material binding definition */
        const char*               texcoord_name     = child_element_ptr->Attribute("texcoord");
        system_hashed_ansi_string texcoord_name_has = NULL;

        ASSERT_DEBUG_SYNC(texcoord_name != NULL,
                          "Texcoord name is NULL");

        texcoord_name_has = system_hashed_ansi_string_create(texcoord_name);

        /* Form the descriptor */
        collada_data_surface sampler_surface = NULL;

        collada_data_sampler2D_get_properties(sampler,
                                             &sampler_surface,
                                             &new_data->mag_filter,
                                             &new_data->min_filter);

        collada_data_surface_get_property(sampler_surface,
                                          COLLADA_DATA_SURFACE_PROPERTY_TEXTURE,
                                         &new_data->image);

        new_data->texcoord_name = texcoord_name_has;

        /* Store it */
        result_ptr->data = new_data;
        result_ptr->type = COLLADA_DATA_SHADING_FACTOR_TEXTURE;
    }
    else
    {
        ASSERT_DEBUG_SYNC(child_element_name != NULL,
                          "Unsupported child node name [%s]",
                          child_element_name);
    }
}

/** TODO */
PRIVATE bool _collada_data_effect_read_float_xml_node(__in  __notnull tinyxml2::XMLElement* element_ptr,
                                                      __out __notnull float*                out_result_ptr)
{
    bool result = false;

    /* Indeed! Extract the float information */
    tinyxml2::XMLElement* float_element_ptr = element_ptr->FirstChildElement("float");

    ASSERT_DEBUG_SYNC(float_element_ptr != NULL,
                      "Animated information is not supported");

    if (float_element_ptr != NULL)
    {
        result = (sscanf(float_element_ptr->GetText(),
                         "%f",
                         out_result_ptr)) == 1;
    } /* if (float_element_ptr != NULL) */

    return result;
}
/** TODO */
PRIVATE tinyxml2::XMLElement* _collada_data_effect_traverse_xml_tree(__in                           __notnull tinyxml2::XMLElement*           root_element_ptr,
                                                                     __in                                     unsigned int                    n_traversal_nodes,
                                                                     __in_ecount(n_traversal_nodes) __notnull const _xml_tree_traversal_node* traversal_nodes)
{
    ASSERT_DEBUG_SYNC(root_element_ptr != NULL,
                      "Root element is NULL");
    ASSERT_DEBUG_SYNC(n_traversal_nodes != 0,
                      "n_traversal_nodes is zero");
    ASSERT_DEBUG_SYNC(traversal_nodes != NULL,
                      "traversal_nodes is NULL");

    /* Traverse the tree */
    tinyxml2::XMLElement* result = root_element_ptr;

    for (unsigned int n_traversal_node = 0;
                      n_traversal_node < n_traversal_nodes && result != NULL;
                    ++n_traversal_node)
    {
        const _xml_tree_traversal_node& current_node_descriptor = traversal_nodes[n_traversal_node];

        switch (current_node_descriptor.type)
        {
            case XML_TREE_TRAVERSAL_NODE_TYPE_NODE:
            {
                result = result->FirstChildElement(current_node_descriptor.node_name);

                break;
            } /* case XML_TREE_TRAVERSAL_NODE_TYPE_NODE: */

            case XML_TREE_TRAVERSAL_NODE_TYPE_NODE_WITH_TEXT_ATTRIBUTE_AND_VALUE:
            {
                result = result->FirstChildElement(current_node_descriptor.node_name);

                if (result != NULL)
                {
                    while (result != NULL)
                    {
                        const char* attribute_sid = result->Attribute(current_node_descriptor.attribute_name);

                        if (attribute_sid != NULL                           &&
                            strcmp(attribute_sid,
                                   current_node_descriptor.attribute_value) == 0)
                        {
                            break;
                        }

                        /* "Shader" attribute was not found, iterate */
                        result = result->NextSiblingElement(current_node_descriptor.node_name);
                    } /* while (result != NULL) */
                } /* if (result != NULL) */

                break;
            } /* case XML_TREE_TRAVERSAL_NODE_TYPE_NODE_WITH_TEXT_ATTRIBUTE_AND_VALUE: */

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized traversal node descriptor type");
            }
        } /* switch (current_node_descriptor.type) */
    } /* for (all traversal nodes) */

    return result;
}

/** TODO */
PUBLIC collada_data_effect collada_data_effect_create(__in __notnull tinyxml2::XMLElement* current_effect_element_ptr,
                                                      __in __notnull system_hash64map      images_by_id_map)
{
    _collada_data_effect* new_effect_ptr = new (std::nothrow) _collada_data_effect;

    ASSERT_ALWAYS_SYNC(new_effect_ptr != NULL,
                       "Out of memory");
    if (new_effect_ptr != NULL)
    {
        new_effect_ptr->id = system_hashed_ansi_string_create(current_effect_element_ptr->Attribute("id") );

        /* At this node level we're expecting only a single profile_COMMON node. */
        tinyxml2::XMLElement* profile_cg_element_ptr     = current_effect_element_ptr->FirstChildElement("profile_CG");
        tinyxml2::XMLElement* profile_common_element_ptr = current_effect_element_ptr->FirstChildElement("profile_COMMON");
        tinyxml2::XMLElement* profile_gles_element_ptr   = current_effect_element_ptr->FirstChildElement("profile_GLES");
        tinyxml2::XMLElement* profile_glsl_element_ptr   = current_effect_element_ptr->FirstChildElement("profile_GLSL");

        ASSERT_DEBUG_SYNC(profile_cg_element_ptr   == NULL &&
                          profile_gles_element_ptr == NULL &&
                          profile_glsl_element_ptr == NULL,
                          "COLLADA data file uses an incommon profile for one of the effects - only COMMON profiles are supported");

        ASSERT_DEBUG_SYNC(profile_common_element_ptr != NULL,
                          "COLLADA data file does not define a COMMON profile for at least one effect");

        if (profile_common_element_ptr != NULL)
        {
            /* <profile_COMMON> node should define exactly one technique */
            tinyxml2::XMLElement*   child_element_ptr        = profile_common_element_ptr->FirstChildElement();
            system_resizable_vector samplers                 = system_resizable_vector_create(4 /* capacity */,
                                                                                              sizeof(collada_data_sampler2D) );
            system_hash64map        samplers_by_id_map       = system_hash64map_create       (4 /* capacity */);
            system_resizable_vector surfaces                 = system_resizable_vector_create(4 /* capacity */,
                                                                                              sizeof(collada_data_surface) );
            system_hash64map        surfaces_by_id_map       = system_hash64map_create       (4 /* capacity */);

            while (child_element_ptr != NULL)
            {
                /* Which element is this? */
                const char* child_element_type = child_element_ptr->Name();

                if (strcmp(child_element_type, "technique") == 0)
                {
                    /* NOTE: This is a grossly simplified reader of <technique> node. We assume only one
                     *       shading item can be requested for a single technique.
                     */
                    bool                  has_shading_been_defined = false;
                    tinyxml2::XMLElement* technique_item_element_ptr = child_element_ptr->FirstChildElement();

                    while (technique_item_element_ptr != NULL)
                    {
                        const char* item_name = technique_item_element_ptr->Name();

                        ASSERT_DEBUG_SYNC(item_name != NULL,
                                          "Node name is NULL");

                        if (strcmp(item_name, "lambert") == 0)
                        {
                            _collada_data_effect_init_lambert(new_effect_ptr,
                                                              technique_item_element_ptr,
                                                              samplers_by_id_map);
                        }
                        else
                        {
                            /* TODO */
                            ASSERT_DEBUG_SYNC(false,
                                              "Technique item [%s] is not supported",
                                              item_name);
                        }

                        /* Iterate to next technique item */
                        technique_item_element_ptr = technique_item_element_ptr->NextSiblingElement();
                    } /* while (technique_item_element_ptr != NULL) */
                }
                else
                if (strcmp(child_element_type, "newparam") == 0)
                {
                    const char* child_element_sid = child_element_ptr->Attribute("sid");

                    ASSERT_DEBUG_SYNC(child_element_sid != NULL,
                                      "No sid attribute associated with a <surface> node");

                    /* Take the first child subnode of the node */
                    tinyxml2::XMLElement* child_subelement_ptr = child_element_ptr->FirstChildElement();

                    ASSERT_DEBUG_SYNC(child_subelement_ptr != NULL,
                                      "Child sub-element is NULL");

                    const char* child_subelement_type = child_subelement_ptr->Name();

                    if (strcmp(child_subelement_type, "surface") == 0)
                    {
                        system_hashed_ansi_string new_surface_id = system_hashed_ansi_string_create(child_element_sid);
                        collada_data_surface      new_surface    = collada_data_surface_create     (child_subelement_ptr,
                                                                                                    new_surface_id,
                                                                                                    images_by_id_map);

                        /* Store the descriptor */
                        system_resizable_vector_push(surfaces,
                                                     new_surface);

                        system_hash64map_insert(surfaces_by_id_map,
                                                system_hashed_ansi_string_get_hash(new_surface_id),
                                                new_surface,
                                                NULL,
                                                NULL);
                    }
                    else
                    if (strcmp(child_subelement_type, "sampler2D") == 0)
                    {
                        system_hashed_ansi_string new_sampler_id     = system_hashed_ansi_string_create       (child_element_sid);
                        collada_data_sampler2D    new_sampler        = collada_data_sampler2D_create          (new_sampler_id,
                                                                                                               child_subelement_ptr,
                                                                                                               surfaces_by_id_map);
                        tinyxml2::XMLElement*     source_element_ptr = child_subelement_ptr->FirstChildElement("source");
                        system_hashed_ansi_string surface_name       = system_hashed_ansi_string_create       (source_element_ptr->GetText() );
                        collada_data_surface      surface            = NULL;

                        system_hash64map_get(surfaces_by_id_map,
                                             system_hashed_ansi_string_get_hash(surface_name),
                                             &surface);
                        ASSERT_DEBUG_SYNC   (surface != NULL,
                                             "Could not find a surface by the name of [%s]",
                                             system_hashed_ansi_string_get_buffer(surface_name) );

                        /* Store it */
                        system_resizable_vector_push(samplers,
                                                     new_sampler);
                        system_hash64map_insert     (samplers_by_id_map,
                                                     system_hashed_ansi_string_get_hash(new_sampler_id),
                                                     new_sampler,
                                                     NULL,
                                                     NULL);
                    }
                    else
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Effect defines a <newparam> node, child of which was not recognized [%s]",
                                          child_subelement_type);
                    }
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized child node of <profile_COMMON> node");
                }

                /* Iterate over all children */
                child_element_ptr = child_element_ptr->NextSiblingElement();
            }

            /* Release helper vectors */
            collada_data_sampler2D sampler = NULL;
            collada_data_surface   surface = NULL;

            while (system_resizable_vector_pop(samplers,
                                              &sampler))
            {
                collada_data_sampler2D_release(sampler);

                sampler = NULL;
            }
            system_resizable_vector_release(samplers);
            samplers = NULL;

            if (samplers_by_id_map != NULL)
            {
                system_hash64map_release(samplers_by_id_map);

                samplers_by_id_map = NULL;
            }

            while (system_resizable_vector_pop(surfaces,
                                              &surface))
            {
                collada_data_surface_release(surface);

                surface = NULL;
            }
            system_resizable_vector_release(surfaces);
            surfaces = NULL;

            if (surfaces_by_id_map != NULL)
            {
                system_hash64map_release(surfaces_by_id_map);

                surfaces_by_id_map = NULL;
            }
        } /* if (profile_common_element_ptr != NULL) */
        else
        {
            LOG_FATAL("No <profile_COMMON> node defined for <effect>");

            ASSERT_DEBUG_SYNC(false,
                              "Cannot add effect");
        }

        /* Try to extract LW-specific data from <extra> */
        const _xml_tree_traversal_node traverse_to_shaders_component_node_path[] =
        {
            {XML_TREE_TRAVERSAL_NODE_TYPE_NODE, "extra"},
            {XML_TREE_TRAVERSAL_NODE_TYPE_NODE, "technique"},
            {XML_TREE_TRAVERSAL_NODE_TYPE_NODE, "shaders"},
            {XML_TREE_TRAVERSAL_NODE_TYPE_NODE, "component"}
        };
        const unsigned int    n_traverse_to_shaders_component_node_path_nodes = sizeof(traverse_to_shaders_component_node_path) /
                                                                                sizeof(traverse_to_shaders_component_node_path[0]);

        tinyxml2::XMLElement* root_shaders_component_element_ptr = _collada_data_effect_traverse_xml_tree(current_effect_element_ptr,
                                                                                                          n_traverse_to_shaders_component_node_path_nodes,
                                                                                                          traverse_to_shaders_component_node_path);

        if (root_shaders_component_element_ptr != NULL)
        {
            const _xml_tree_traversal_node traverse_to_shader_attribute_path[] =
            {
                {XML_TREE_TRAVERSAL_NODE_TYPE_NODE_WITH_TEXT_ATTRIBUTE_AND_VALUE, "attribute", "sid", "Shader"},
                {XML_TREE_TRAVERSAL_NODE_TYPE_NODE,                               "connected_component"},
                {XML_TREE_TRAVERSAL_NODE_TYPE_NODE,                               "component"},
            };
            const unsigned int             n_traverse_to_shader_attribute_path_nodes = sizeof(traverse_to_shader_attribute_path) /
                                                                                       sizeof(traverse_to_shader_attribute_path[0]);

            tinyxml2::XMLElement* shader_element_ptr = _collada_data_effect_traverse_xml_tree(root_shaders_component_element_ptr,
                                                                                              n_traverse_to_shader_attribute_path_nodes,
                                                                                              traverse_to_shader_attribute_path);

            if (shader_element_ptr != NULL)
            {
                /* This attribute stores various interesting stuff. Of our particular interest are:
                 *
                 * - Glosiness
                 * - Luminosity
                 * - Specularity
                 * - UV map name.
                 */
                const _xml_tree_traversal_node traverse_to_glosiness_node_path[] =
                {
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE_WITH_TEXT_ATTRIBUTE_AND_VALUE, "attribute",          "sid", "Glossiness"},
                };
                const _xml_tree_traversal_node traverse_to_luminosity_node_path[] =
                {
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE_WITH_TEXT_ATTRIBUTE_AND_VALUE, "attribute",          "sid", "Luminosity"},
                };
                const _xml_tree_traversal_node traverse_to_specularity_node_path[] =
                {
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE_WITH_TEXT_ATTRIBUTE_AND_VALUE, "attribute",          "sid", "Specularity"},
                };
                const _xml_tree_traversal_node traverse_to_UvMapName_node_path[] =
                {
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE_WITH_TEXT_ATTRIBUTE_AND_VALUE, "attribute",          "sid", "Color"},
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE,                               "connected_component"},
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE,                               "component"},
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE_WITH_TEXT_ATTRIBUTE_AND_VALUE, "attribute",          "sid", "Projection"},
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE,                               "connected_component"},
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE,                               "component"},
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE_WITH_TEXT_ATTRIBUTE_AND_VALUE, "attribute",          "sid", "UvMapName"},
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE,                               "string"}
                };

                const unsigned int             n_traverse_to_glosiness_node_path_nodes   = sizeof(traverse_to_glosiness_node_path) /
                                                                                           sizeof(traverse_to_glosiness_node_path[0]);
                const unsigned int             n_traverse_to_luminosity_node_path_nodes  = sizeof(traverse_to_luminosity_node_path) /
                                                                                           sizeof(traverse_to_luminosity_node_path[0]);
                const unsigned int             n_traverse_to_specularity_node_path_nodes = sizeof(traverse_to_specularity_node_path) /
                                                                                           sizeof(traverse_to_specularity_node_path[0]);
                const unsigned int             n_traverse_to_UvMapName_node_path_nodes   = sizeof(traverse_to_UvMapName_node_path)  /
                                                                                           sizeof(traverse_to_UvMapName_node_path[0]);

                tinyxml2::XMLElement* glosiness_element_ptr        = _collada_data_effect_traverse_xml_tree(shader_element_ptr,
                                                                                                            n_traverse_to_glosiness_node_path_nodes,
                                                                                                            traverse_to_glosiness_node_path);
                tinyxml2::XMLElement* luminosity_element_ptr       = _collada_data_effect_traverse_xml_tree(shader_element_ptr,
                                                                                                            n_traverse_to_luminosity_node_path_nodes,
                                                                                                            traverse_to_luminosity_node_path);
                tinyxml2::XMLElement* specularity_element_ptr      = _collada_data_effect_traverse_xml_tree(shader_element_ptr,
                                                                                                            n_traverse_to_specularity_node_path_nodes,
                                                                                                            traverse_to_specularity_node_path);
                tinyxml2::XMLElement* UvMapName_string_element_ptr = _collada_data_effect_traverse_xml_tree(shader_element_ptr,
                                                                                                            n_traverse_to_UvMapName_node_path_nodes,
                                                                                                            traverse_to_UvMapName_node_path);
                if (UvMapName_string_element_ptr != NULL)
                {
                    /* UV map name information is present! */
                    new_effect_ptr->uv_map_name = system_hashed_ansi_string_create(UvMapName_string_element_ptr->GetText() );
                } /* if (UvMapName_string_element_ptr != NULL) */

                /* Read float values */
                if (glosiness_element_ptr != NULL)
                {
                    float glosiness_value = 0.0f;

                    if (_collada_data_effect_read_float_xml_node(glosiness_element_ptr,
                                                                &glosiness_value) )
                    {
                        _collada_data_effect_init_float_shading_item(&new_effect_ptr->shininess,
                                                                     glosiness_value);
                    }
                } /* if (glosiness_element_ptr != NULL) */

                if (luminosity_element_ptr != NULL)
                {
                    float luminosity_value = 0.0f;

                    if (_collada_data_effect_read_float_xml_node(luminosity_element_ptr,
                                                                &luminosity_value) )
                    {
                        _collada_data_effect_init_float_shading_item(&new_effect_ptr->luminosity,
                                                                     luminosity_value);
                    }
                } /* if (luminosity_element_ptr != NULL) */

                if (specularity_element_ptr != NULL)
                {
                    float specularity_value = 0.0f;

                    if (_collada_data_effect_read_float_xml_node(specularity_element_ptr,
                                                                &specularity_value) )
                    {
                        _collada_data_effect_init_float_shading_item(&new_effect_ptr->specular,
                                                                     specularity_value);
                    }
                } /* if (specularity_element_ptr != NULL) */
            } /* if (shader_component_element_ptr != NULL) */

            /* If UV Map Name was not found, it can still be defined under <attribute> with sid="Name",
             * under the <string> node.
             */
            if (new_effect_ptr->uv_map_name == system_hashed_ansi_string_get_default_empty_string() )
            {
                const _xml_tree_traversal_node traverse_to_name_string_node_path[] =
                {
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE_WITH_TEXT_ATTRIBUTE_AND_VALUE, "attribute", "sid", "Name"},
                    {XML_TREE_TRAVERSAL_NODE_TYPE_NODE,                               "string"}
                };
                const unsigned int             n_traverse_to_name_string_node_path_nodes = sizeof(traverse_to_name_string_node_path) /
                                                                                           sizeof(traverse_to_name_string_node_path[0]);

                tinyxml2::XMLElement* string_element_ptr = _collada_data_effect_traverse_xml_tree(root_shaders_component_element_ptr,
                                                                                                  n_traverse_to_name_string_node_path_nodes,
                                                                                                  traverse_to_name_string_node_path);

                if (string_element_ptr != NULL)
                {
                    /* UV map name information is present! */
                    new_effect_ptr->uv_map_name = system_hashed_ansi_string_create(string_element_ptr->GetText() );
                }
            } /* if (uv map name data was not found) */
        } /* if (root_shaders_component_element_ptr != NULL) */
    } /* if (new_effect_ptr != NULL) */

    return (collada_data_effect) new_effect_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_effect_get_properties(__in      __notnull collada_data_effect    effect,
                                                           __out_opt           _collada_data_shading* out_shading,
                                                           __out_opt           int*                   out_shading_factor_item_bitmask)
{
    _collada_data_effect* effect_ptr = (_collada_data_effect*) effect;

    if (out_shading != NULL)
    {
        *out_shading = effect_ptr->shading;
    }

    if (out_shading_factor_item_bitmask != NULL)
    {
        *out_shading_factor_item_bitmask =
                                           (int) ((effect_ptr->ambient.type    != COLLADA_DATA_SHADING_FACTOR_NONE) ? COLLADA_DATA_SHADING_FACTOR_ITEM_AMBIENT    : 0) |
                                           (int) ((effect_ptr->diffuse.type    != COLLADA_DATA_SHADING_FACTOR_NONE) ? COLLADA_DATA_SHADING_FACTOR_ITEM_DIFFUSE    : 0) |
                                           (int) ((effect_ptr->luminosity.type != COLLADA_DATA_SHADING_FACTOR_NONE) ? COLLADA_DATA_SHADING_FACTOR_ITEM_LUMINOSITY : 0) |
                                           (int) ((effect_ptr->shininess.type  != COLLADA_DATA_SHADING_FACTOR_NONE) ? COLLADA_DATA_SHADING_FACTOR_ITEM_SHININESS  : 0) |
                                           (int) ((effect_ptr->specular.type   != COLLADA_DATA_SHADING_FACTOR_NONE) ? COLLADA_DATA_SHADING_FACTOR_ITEM_SPECULAR   : 0);
    }
}

/* Please see header for spec */
PUBLIC EMERALD_API void collada_data_effect_get_property(__in  __notnull const collada_data_effect          effect,
                                                         __in                  collada_data_effect_property property,
                                                         __out __notnull       void*                        out_data_ptr)
{
    const _collada_data_effect* effect_ptr = (const _collada_data_effect*) effect;

    switch (property)
    {
        case COLLADA_DATA_EFFECT_PROPERTY_ID:
        {
            *((system_hashed_ansi_string*) out_data_ptr) = effect_ptr->id;

            break;
        }

        case COLLADA_DATA_EFFECT_PROPERTY_UV_MAP_NAME:
        {
            *((system_hashed_ansi_string*) out_data_ptr) = effect_ptr->uv_map_name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized COLLADA effect property");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API collada_data_shading_factor_item collada_data_effect_get_shading_factor_item_by_texcoord(__in __notnull collada_data_effect       effect,
                                                                                                            __in __notnull system_hashed_ansi_string texcoord_name)
{
    _collada_data_effect*            effect_ptr = (_collada_data_effect*) effect;
    collada_data_shading_factor_item result     = COLLADA_DATA_SHADING_FACTOR_ITEM_UNKNOWN;

    struct _data_item
    {
        _collada_data_shading_factor_item* item_ptr;
        collada_data_shading_factor_item   result_value;
    } data[] =
    {
        {
            &effect_ptr->ambient,
            COLLADA_DATA_SHADING_FACTOR_ITEM_AMBIENT
        },

        {
            &effect_ptr->diffuse,
            COLLADA_DATA_SHADING_FACTOR_ITEM_DIFFUSE
        },

        {
            &effect_ptr->luminosity,
            COLLADA_DATA_SHADING_FACTOR_ITEM_LUMINOSITY
        },

        {
            &effect_ptr->shininess,
            COLLADA_DATA_SHADING_FACTOR_ITEM_SHININESS
        },

        {
            &effect_ptr->specular,
            COLLADA_DATA_SHADING_FACTOR_ITEM_SPECULAR
        }
    };
    const unsigned int n_data_items = sizeof(data) / sizeof(data[0]);

    for (unsigned int n_data_item = 0;
                      n_data_item < n_data_items;
                    ++n_data_item)
    {
        const _data_item& data_item = data[n_data_item];

        if (data_item.item_ptr->type == COLLADA_DATA_SHADING_FACTOR_TEXTURE &&
            system_hashed_ansi_string_is_equal_to_hash_string( ((_collada_data_shading_factor_item_texture*) data_item.item_ptr->data)->texcoord_name,
                                                               texcoord_name) )
        {
            result = data_item.result_value;

            break;
        }
    }

    ASSERT_DEBUG_SYNC(result != COLLADA_DATA_SHADING_FACTOR_ITEM_UNKNOWN,
                      "Could not identify shading factor item associated with specified texcoord [%s]",
                      system_hashed_ansi_string_get_buffer(texcoord_name) );

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_effect_get_shading_factor_item_properties(__in      __notnull collada_data_effect              effect,
                                                                               __in                collada_data_shading_factor_item item,
                                                                               __out_opt           collada_data_shading_factor*     out_type)
{
    _collada_data_effect*              effect_ptr = (_collada_data_effect*) effect;
    collada_data_shading_factor        result     = COLLADA_DATA_SHADING_FACTOR_UNKNOWN;
    _collada_data_shading_factor_item* item_ptr   = _collada_data_get_effect_shading_factor_item(effect_ptr,
                                                                                                 item);

    if (item_ptr != NULL)
    {
        result = item_ptr->type;
    }

    if (out_type != NULL)
    {
        *out_type = result;
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_effect_get_shading_factor_item_float_properties(__in __notnull collada_data_effect              effect,
                                                                                     __in           collada_data_shading_factor_item item,
                                                                                     __out          float*                           out_float)
{
    _collada_data_effect*              effect_ptr = (_collada_data_effect*) effect;
    _collada_data_shading_factor_item* item_ptr   = _collada_data_get_effect_shading_factor_item(effect_ptr,
                                                                                                 item);

    if (item_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(item_ptr->type == COLLADA_DATA_SHADING_FACTOR_FLOAT,
                          "Requested shading factor item is not a float");

        const float* data_ptr = (const float*) item_ptr->data;

        if (out_float != NULL)
        {
            *out_float = *data_ptr;
        }
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_effect_get_shading_factor_item_float4_properties(__in __notnull      collada_data_effect              effect,
                                                                                      __in                collada_data_shading_factor_item item,
                                                                                      __out_ecount_opt(4) float*                           out_float4)
{
    _collada_data_effect*              effect_ptr = (_collada_data_effect*) effect;
    _collada_data_shading_factor_item* item_ptr   = _collada_data_get_effect_shading_factor_item(effect_ptr,
                                                                                                 item);

    if (item_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(item_ptr->type == COLLADA_DATA_SHADING_FACTOR_VEC4,
                          "Requested shading factor item is not a vec4");

        const float* data_ptr = (const float*) item_ptr->data;

        if (out_float4 != NULL)
        {
            memcpy(out_float4,
                   data_ptr,
                   sizeof(float) * 4);
        }
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_effect_get_shading_factor_item_texture_properties(__in __notnull collada_data_effect              effect,
                                                                                       __in           collada_data_shading_factor_item item,
                                                                                       __out_opt      collada_data_image*              out_image,
                                                                                       __out_opt      _collada_data_sampler_filter*    out_mag_filter,
                                                                                       __out_opt      _collada_data_sampler_filter*    out_min_filter,
                                                                                       __out_opt      system_hashed_ansi_string*       out_texcoord_name)
{
    _collada_data_effect*              effect_ptr = (_collada_data_effect*) effect;
    _collada_data_shading_factor_item* item_ptr   = _collada_data_get_effect_shading_factor_item(effect_ptr, item);

    if (item_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(item_ptr->type == COLLADA_DATA_SHADING_FACTOR_TEXTURE,
                          "Requested shading factor item is not a texture");

        const _collada_data_shading_factor_item_texture* texture_ptr = (const _collada_data_shading_factor_item_texture*) item_ptr->data;

        if (out_image != NULL)
        {
            *out_image = (collada_data_image) texture_ptr->image;
        }

        if (out_mag_filter != NULL)
        {
            *out_mag_filter = texture_ptr->mag_filter;
        }

        if (out_min_filter != NULL)
        {
            *out_min_filter = texture_ptr->min_filter;
        }

        if (out_texcoord_name != NULL)
        {
            *out_texcoord_name = texture_ptr->texcoord_name;
        }
    }
}

/** Please see header for spec */
PUBLIC void collada_data_effect_release(__in __notnull __post_invalid collada_data_effect effect)
{
    delete (_collada_data_effect*) effect;

    effect = NULL;
}