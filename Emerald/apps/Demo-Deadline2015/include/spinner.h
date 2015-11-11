/* Spinner renderer */
#ifndef SPINNER_H
#define SPINNER_H

#include "shared.h"
#include "demo/demo_types.h"

typedef enum
{
    /* Settable; spinner_stage. */
    SPINNER_PROPERTY_CURRENT_STAGE,

     /* Not settable; ogl_texture. */
    SPINNER_PROPERTY_RESULT_COLOR_TO,

    /* Not settable; ogl_texture. */
    SPINNER_PROPERTY_RESULT_VELOCITY_TO,

    /* Not settable; GLint[4].
     *
     * Updated during _render() call.
     */
     SPINNER_PROPERTY_VIEWPORT,
} spinner_property;

typedef enum
{
    SPINNER_STAGE_START,
    SPINNER_STAGE_FIRST_SPIN,
    SPINNER_STAGE_SECOND_SPIN,

    SPINNER_STAGE_ANIMATION,

    /* Always last */
    SPINNER_STAGE_COUNT
} spinner_stage;

/** TODO */
PUBLIC void spinner_get_property(spinner_property property,
                                 void*            out_result_ptr);

/** TODO */
PUBLIC void spinner_init(demo_app    app,
                         demo_loader loader);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void spinner_render(uint32_t    n_frame,
                                                  system_time frame_time);

/** TODO */
PUBLIC void spinner_set_property(spinner_property property,
                                 const void*      data);

#endif /* SPINNER_H */