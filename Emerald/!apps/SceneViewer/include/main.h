/**
 *
 * Internal Emerald scene viewer (kbi/elude @2014-2015)
 *
 */
#ifndef MAIN_H
#define MAIN_H

extern ogl_context   _context;
extern system_window _window;

/** TODO */
PUBLIC void _render_scene(ogl_context          context,
                          system_timeline_time time,
                          void*                not_used);

#endif /* MAIN_H */
