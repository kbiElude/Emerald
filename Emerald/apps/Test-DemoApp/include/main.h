/**
 *
 * Test demo application @2015
 *
 */
#ifndef MAIN_H
#define MAIN_H

#include "demo/demo_types.h"


extern ral_context           _context;
extern ral_rendering_handler _rendering_handler;
extern demo_window           _window;

/** TODO */
PUBLIC void _render_scene(ral_context context,
                          uint32_t    frame_index,
                          system_time time,
                          const int*  rendering_area_px_topdown,
                          void*       not_used);

#endif /* MAIN_H */
