/* Spinner renderer */
#ifndef SPINNER_H
#define SPINNER_H

#include "shared.h"

typedef enum
{
    SPINNER_STAGE_START,
    SPINNER_STAGE_FIRST_SPIN,
    SPINNER_STAGE_SECOND_SPIN,

    /* Always last */
    SPINNER_STAGE_COUNT
} spinner_stage;

/** Enqueues a new operation in the demo loader's queue, either showing
 *  a single frame (SPINNER_STAGE_START) or a spinning animation.
 *
 *  @param loader       Demo loader.
 *  @param loader_stage Tells which predefined animation parameters should be used.
 **/
PUBLIC void spinner_enqueue(demo_loader   loader,
                            spinner_stage loader_stage);

/** TODO */
PUBLIC void spinner_init(demo_app    app,
                         demo_loader loader);

/** TODO */
PUBLIC void spinner_render_to_default_fbo(uint32_t n_frame);

#endif /* SPINNER_H */