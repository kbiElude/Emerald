/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "collada/collada_data_scene_graph_node_translate.h"
#include "collada/collada_data_transformation.h"
#include "system/system_assertions.h"
#include "system/system_resizable_vector.h"

/** Please see header for spec */
PUBLIC collada_data_scene_graph_node_item collada_data_scene_graph_node_translate_create(tinyxml2::XMLElement* element_ptr,
                                                                                         const float*          overriding_vec3_ptr,
                                                                                         float*                out_result_vec3_ptr)
{
    collada_data_scene_graph_node_item item          = nullptr;
    const char*                        text          = element_ptr->GetText();
    const char*                        traveller_ptr = text;

    /* Read SID */
    const char* sid = element_ptr->Attribute("sid");

    ASSERT_ALWAYS_SYNC(sid != nullptr,
                       "Could not read <translate> node's SID");

    /* Retrieve matrix data */
    float data[3];

    if (overriding_vec3_ptr == nullptr)
    {
        sscanf(element_ptr->GetText(),
               "%f %f %f",
               data + 0, data + 1, data + 2);
    }
    else
    {
        memcpy(data,
               overriding_vec3_ptr,
               sizeof(float) * 3);
    }

    if (out_result_vec3_ptr != nullptr)
    {
        memcpy(out_result_vec3_ptr,
               data,
               sizeof(float) * 3);
    }

    /* Instantiate new descriptor */
    collada_data_transformation new_transformation = collada_data_transformation_create_translate(element_ptr,
                                                                                                  data);

    ASSERT_ALWAYS_SYNC(new_transformation != nullptr,
                       "Could not create COLLADA transformation descriptor");

    if (new_transformation == nullptr)
    {
        goto end;
    }

    /* Wrap in an item descriptor */
    item = collada_data_scene_graph_node_item_create(COLLADA_DATA_NODE_ITEM_TYPE_TRANSFORMATION,
                                                     new_transformation,
                                                     system_hashed_ansi_string_create(sid) );

end:
    return item;
}
