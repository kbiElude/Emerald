#ifndef RAGL_FRAMEBUFFER_H
#define RAGL_FRAMEBUFFER_H

#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"


typedef enum
{
    /* not settable; GLuint */
    RAGL_FRAMEBUFFER_PROPERTY_ID,

} raGL_framebuffer_property;

/** TODO */
PUBLIC raGL_framebuffer raGL_framebuffer_create(ogl_context             context,
                                                GLint                   fb_id,
                                                uint32_t                n_color_attachments,
                                                const ral_texture_view* color_attachments,
                                                ral_texture_view        opt_ds_attachment);

/** TODO */
PUBLIC EMERALD_API void raGL_framebuffer_get_property(raGL_framebuffer          fb,
                                                      raGL_framebuffer_property property,
                                                      void*                     out_result_ptr);

/** TODO */
PUBLIC void raGL_framebuffer_release(raGL_framebuffer fb);


#endif /* RAGL_FRAMEBUFFER_H */