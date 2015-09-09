#ifndef STAGE_INTRO_H
#define STAGE_INTRO_H

#include "shared.h"
#include "ogl/ogl_types.h"

/** Deinitializes all GL objects created to render intro stage */
PUBLIC RENDERING_CONTEXT_CALL void stage_intro_deinit(ogl_context context);

/** Initializes all GL objects created to render intro stage */
PUBLIC RENDERING_CONTEXT_CALL void stage_intro_init(ogl_context context);

/** Renders frame contents.
 *
 *  @param context                   Rendering context.
 *  @param frame_time                Frame time (relative to the beginning of the timeline).
 *  @param rendering_area_px_topdown Rendering area, to which frame contents should be rendered.
 *                                   The four ints (x1/y1/x2/y2) are calculated, based on the aspect
 *                                   ratio defined for the video segment.
 *                                   It is your responsibility to update the scissor area / viewport.
 *  @param unused                    Rendering call-back user argument. We're not using this, so
 *                                   the argument is always NULL.
 **/
PUBLIC RENDERING_CONTEXT_CALL void stage_intro_render(ogl_context context,
                                                      system_time frame_time,
                                                      const int*  rendering_area_px_topdown,
                                                      void*       unused);

#endif /* STAGE_INTRO_H */