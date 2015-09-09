#ifndef STAGE_PART4_H
#define STAGE_PART4_H

#include "shared.h"
#include "ogl/ogl_types.h"

/** Deinitializes all GL objects created to render part 4 */
PUBLIC RENDERING_CONTEXT_CALL void stage_part4_deinit(ogl_context context);

/** Initializes all GL objects created to render part 4 */
PUBLIC RENDERING_CONTEXT_CALL void stage_part4_init(ogl_context context);

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
PUBLIC RENDERING_CONTEXT_CALL void stage_part4_render(ogl_context context,
                                                      system_time frame_time,
                                                      const int*  rendering_area_px_topdown,
                                                      void*       unused);

#endif /* STAGE_PART4_H */