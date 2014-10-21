/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data_scene_graph_node_rotate.h"
#include "collada/collada_data_transformation.h"
#include "system/system_assertions.h"
#include "system/system_resizable_vector.h"

/** TODO */
PUBLIC collada_data_scene_graph_node_item collada_data_scene_graph_node_rotate_create(__in __notnull tinyxml2::XMLElement* element_ptr)
{
    collada_data_scene_graph_node_item item          = NULL;
    const char*                        text          = element_ptr->GetText();
    const char*                        traveller_ptr = text;

    /* Retrieve matrix data */
    float data[4];

    sscanf_s(element_ptr->GetText(),
             "%f %f %f %f",
             data + 0, data + 1, data + 2,
             data + 3);

    /* Read SID */
    const char* sid = element_ptr->Attribute("sid");

    ASSERT_ALWAYS_SYNC(sid != NULL,
                       "Could not read <rotate> node's SID");

    /* Instantiate new descriptor */
    collada_data_transformation new_transformation = collada_data_transformation_create_rotate(element_ptr, data);

    ASSERT_ALWAYS_SYNC(new_transformation != NULL, "Could not create COLLADA transformation descriptor");
    if (new_transformation == NULL)
    {
        goto end;
    }

    /* Wrap the transformation in an item descriptor */
    item = collada_data_scene_graph_node_item_create(COLLADA_DATA_NODE_ITEM_TYPE_TRANSFORMATION,
                                                     new_transformation,
                                                     system_hashed_ansi_string_create(sid) );
end:
    return item;
}
