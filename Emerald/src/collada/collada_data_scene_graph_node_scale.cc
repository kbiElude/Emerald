/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data_scene_graph_node_scale.h"
#include "collada/collada_data_transformation.h"
#include "system/system_assertions.h"
#include "system/system_resizable_vector.h"

/** TODO */
PUBLIC collada_data_scene_graph_node_item collada_data_scene_graph_node_scale_create(__in __notnull tinyxml2::XMLElement* element_ptr)
{
    collada_data_scene_graph_node_item item          = NULL;
    const char*                        text          = element_ptr->GetText();
    const char*                        traveller_ptr = text;

    /* Retrieve matrix data */
    float data[3];

    sscanf_s(element_ptr->GetText(),
             "%f %f %f",
             data + 0, data + 1, data + 2);

    /* Instantiate new descriptor */
    collada_data_transformation new_transformation = collada_data_transformation_create_scale(element_ptr, data);

    ASSERT_ALWAYS_SYNC(new_transformation != NULL, "Could not create COLLADA transformation descriptor");
    if (new_transformation == NULL)
    {
        goto end;
    }

    /* Wrap the transformation in an item */
    item = collada_data_scene_graph_node_item_create(COLLADA_DATA_NODE_ITEM_TYPE_TRANSFORMATION, new_transformation);

end:
    return item;
}
