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
PUBLIC void _render_scene(ogl_context          context,
                          system_timeline_time time,
                          void*                not_used);

#endif /* MAIN_H */
