/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "collada/collada_data_scene_graph_node_matrix.h"
#include "collada/collada_data_transformation.h"
#include "system/system_assertions.h"
#include "system/system_resizable_vector.h"

PUBLIC collada_data_scene_graph_node_item collada_data_scene_graph_node_matrix_create(tinyxml2::XMLElement* element_ptr)
{
    collada_data_scene_graph_node_item item          = nullptr;
    const char*                        text          = element_ptr->GetText();
    const char*                        traveller_ptr = text;

    /* Read SID */
    const char* sid = element_ptr->Attribute("sid");

    ASSERT_ALWAYS_SYNC(sid != nullptr,
                       "Could not read <matrix> node's SID");

    /* Retrieve matrix data */
    float data[16];

    sscanf(element_ptr->GetText(),
           "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
           data + 0,  data + 1,  data + 2,  data + 3,
           data + 4,  data + 5,  data + 6,  data + 7,
           data + 8,  data + 9,  data + 10, data + 11,
           data + 12, data + 13, data + 14, data + 15);

    /* Instantiate new descriptor */
    collada_data_transformation new_transformation = collada_data_transformation_create_matrix(element_ptr,
                                                                                               data);

    ASSERT_ALWAYS_SYNC(new_transformation != nullptr,
                       "Could not create COLLADA transformation descriptor");

    if (new_transformation == nullptr)
    {
        goto end;
    }

    /* Wrap the transformation in a child descriptor */
    item = collada_data_scene_graph_node_item_create(COLLADA_DATA_NODE_ITEM_TYPE_TRANSFORMATION,
                                                     new_transformation,
                                                     system_hashed_ansi_string_create(sid) );

end:
    return item;
}
