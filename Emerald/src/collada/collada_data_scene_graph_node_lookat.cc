/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "collada/collada_data_scene_graph_node_lookat.h"
#include "collada/collada_data_transformation.h"
#include "system/system_assertions.h"
#include "system/system_resizable_vector.h"

/** TODO */
PUBLIC collada_data_scene_graph_node_item collada_data_scene_graph_node_lookat_create(tinyxml2::XMLElement* element_ptr)
{
    float                              data[9];
    collada_data_scene_graph_node_item item               = nullptr;
    collada_data_transformation        new_transformation = nullptr;
    const char*                        text               = element_ptr->GetText();
    const char*                        traveller_ptr      = text;

    /* Read SID */
    const char* sid = element_ptr->Attribute("sid");

    ASSERT_ALWAYS_SYNC(sid != nullptr,
                       "Could not read <lookat> node's SID");

    /* Retrieve matrix data */
    sscanf(element_ptr->GetText(),
           "%f %f %f %f %f %f %f %f %f",
           data + 0, data + 1, data + 2,
           data + 3, data + 4, data + 5,
           data + 6, data + 7, data + 8);

    /* Instantiate new descriptor */
    new_transformation = collada_data_transformation_create_lookat(element_ptr,
                                                                   data);

    ASSERT_ALWAYS_SYNC(new_transformation != nullptr,
                       "Could not create COLLADA transformation descriptor");

    if (new_transformation != nullptr)
    {
        /* Wrap the transformation in an item */
        item = collada_data_scene_graph_node_item_create(COLLADA_DATA_NODE_ITEM_TYPE_TRANSFORMATION,
                                                         new_transformation,
                                                         system_hashed_ansi_string_create(sid) );
    }

    return item;
}
