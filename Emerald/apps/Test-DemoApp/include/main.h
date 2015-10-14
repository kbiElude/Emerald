/**
 *
 * Test demo application @2015
 *
 */
#ifndef MAIN_H
#define MAIN_H

extern ogl_context           _context;
extern ogl_rendering_handler _rendering_handler;
extern system_window         _window;

/** TODO */
PUBLIC void _render_scene(ogl_context context,
                          uint32_t    frame_index,
                          system_time time,
                          const int*  rendering_area_px_topdown,
                          void*       not_used);

#endif /* MAIN_H */
