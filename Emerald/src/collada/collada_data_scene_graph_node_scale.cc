/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "collada/collada_data_scene_graph_node_scale.h"
#include "collada/collada_data_transformation.h"
#include "system/system_assertions.h"
#include "system/system_resizable_vector.h"

/** TODO */
PUBLIC collada_data_scene_graph_node_item collada_data_scene_graph_node_scale_create(tinyxml2::XMLElement* element_ptr)
{
    collada_data_scene_graph_node_item item          = nullptr;
    const char*                        text          = element_ptr->GetText();
    const char*                        traveller_ptr = text;

    /* Read SID */
    const char* sid = element_ptr->Attribute("sid");

    ASSERT_ALWAYS_SYNC(sid != nullptr,
                       "Could not read <scale> node's SID");

    /* Retrieve matrix data */
    float data[3];

    sscanf(element_ptr->GetText(),
           "%f %f %f",
           data + 0, data + 1, data + 2);

    /* Instantiate new descriptor */
    collada_data_transformation new_transformation = collada_data_transformation_create_scale(element_ptr,
                                                                                              data);

    ASSERT_ALWAYS_SYNC(new_transformation != nullptr,
                       "Could not create COLLADA transformation descriptor");

    if (new_transformation == nullptr)
    {
        goto end;
    }

    /* Wrap the transformation in an item */
    item = collada_data_scene_graph_node_item_create(COLLADA_DATA_NODE_ITEM_TYPE_TRANSFORMATION,
                                                     new_transformation,
                                                     system_hashed_ansi_string_create(sid) );

end:
    return item;
}
