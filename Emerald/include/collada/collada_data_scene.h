/**
 *
 * Emerald (kbi/elude @2014-2016)
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
    /* settable, scene */
    COLLADA_DATA_SCENE_PROPERTY_EMERALD_SCENE,

    /* settable, system_hashed_ansi_string */
    COLLADA_DATA_SCENE_PROPERTY_NAME,

    /* not settable, collada_data_scene_graph_node */
    COLLADA_DATA_SCENE_PROPERTY_ROOT_NODE,

    /* Always last */
    COLLADA_DATA_SCENE_PROPERTY_COUNT
};

/** TODO */
PUBLIC collada_data_scene collada_data_scene_create(tinyxml2::XMLElement* scene_element_ptr,
                                                    collada_data          collada_data);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_get_property(const collada_data_scene    scene,
                                                        collada_data_scene_property property,
                                                        void*                       out_data_ptr);

/** TODO */
PUBLIC void collada_data_scene_release(collada_data_scene scene);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_set_property(collada_data_scene          scene,
                                                        collada_data_scene_property property,
                                                        const void*                 data_ptr);

#endif /* COLLADA_DATA_SCENE_H */
