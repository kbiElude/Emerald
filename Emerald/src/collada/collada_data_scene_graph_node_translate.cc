/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "collada/collada_data_scene_graph_node_translate.h"
#include "collada/collada_data_transformation.h"
#include "system/system_assertions.h"
#include "system/system_resizable_vector.h"

/** Please see header for spec */
PUBLIC collada_data_scene_graph_node_item collada_data_scene_graph_node_translate_create(__in      __notnull   tinyxml2::XMLElement* element_ptr,
                                                                                         __in_opt  __ecount(3) const float*          overriding_vec3,
                                                                                         __out_opt __ecount(3) float*                result_vec3)
{
    collada_data_scene_graph_node_item item          = NULL;
    const char*                        text          = element_ptr->GetText();
    const char*                        traveller_ptr = text;

    /* Read SID */
    const char* sid = element_ptr->Attribute("sid");

    ASSERT_ALWAYS_SYNC(sid != NULL,
                       "Could not read <translate> node's SID");

    /* Retrieve matrix data */
    float data[3];

    if (overriding_vec3 == NULL)
    {
        sscanf(element_ptr->GetText(),
               "%f %f %f",
               data + 0, data + 1, data + 2);
    }
    else
    {
        memcpy(data,
               overriding_vec3,
               sizeof(float) * 3);
    }

    if (result_vec3 != NULL)
    {
        memcpy(result_vec3,
               data,
               sizeof(float) * 3);
    }

    /* Instantiate new descriptor */
    collada_data_transformation new_transformation = collada_data_transformation_create_translate(element_ptr, data);

    ASSERT_ALWAYS_SYNC(new_transformation != NULL,
                       "Could not create COLLADA transformation descriptor");

    if (new_transformation == NULL)
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
