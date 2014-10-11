/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_SCENE_H
#define COLLADA_DATA_SCENE_H

#include "collada/collada_data_scene.h"
#include "collada/collada_data_scene_graph_node.h"
#include "collada/collada_types.h"
#include "scene/scene_types.h"
#include "tinyxml2.h"

enum collada_data_scene_property
{
    COLLADA_DATA_SCENE_PROPERTY_EMERALD_SCENE,  /*     settable, scene */
    COLLADA_DATA_SCENE_PROPERTY_NAME,           /*     settable, system_hashed_ansi_string */
    COLLADA_DATA_SCENE_PROPERTY_ROOT_NODE,      /* not settable, collada_data_scene_graph_node */

    /* Always last */
    COLLADA_DATA_SCENE_PROPERTY_COUNT
};

/** TODO */
PUBLIC collada_data_scene collada_data_scene_create(__in __notnull tinyxml2::XMLElement* scene_element_ptr,
                                                    __in __notnull collada_data          collada_data);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_get_property(__in  __notnull const collada_data_scene          scene,
                                                        __in                  collada_data_scene_property property,
                                                        __out __notnull void*                             out_data_ptr);

/** TODO */
PUBLIC void collada_data_scene_release(__in __notnull __post_invalid collada_data_scene scene);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_set_property(__in __notnull collada_data_scene          scene,
                                                        __in           collada_data_scene_property property,
                                                        __in __notnull void*                       data_ptr);

#endif /* COLLADA_DATA_SCENE_H */
