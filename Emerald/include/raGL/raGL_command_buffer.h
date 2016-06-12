#ifndef RAGL_COMMAND_BUFFER_H
#define RAGL_COMMAND_BUFFER_H


#include "raGL/raGL_types.h"

/** TODO */
PUBLIC raGL_command_buffer raGL_command_buffer_create(ral_command_buffer command_buffer_ral,
                                                      ogl_context        context);

/** TODO */
PUBLIC void raGL_command_buffer_deinit();

/** TODO */
PUBLIC void raGL_command_buffer_execute(raGL_command_buffer command_buffer);

/** TODO */
PUBLIC void raGL_command_buffer_init();

/** TODO */
PUBLIC void raGL_command_buffer_release(raGL_command_buffer command_buffer);

/** TODO */
PUBLIC void raGL_command_buffer_start_recording(raGL_command_buffer command_buffer);

/** TODO */
PUBLIC void raGL_command_buffer_stop_recording(raGL_command_buffer command_buffer);


#endif /* RAGL_COMMAND_BUFFER_H */