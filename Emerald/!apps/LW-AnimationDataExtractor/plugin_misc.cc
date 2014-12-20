#define _MSWIN

#include "shared.h"
#include "plugin.h"
#include "plugin_misc.h"
#include "scene/scene.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"

/** TODO */
void FillWithMiscellaneousData(__in __notnull scene in_scene)
{
    float fps = (float) scene_info_ptr->framesPerSecond;

    scene_set_property(in_scene,
                       SCENE_PROPERTY_FPS,
                      &fps);
}


