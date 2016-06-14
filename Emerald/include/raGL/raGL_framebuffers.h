/**
 *
 * Emerald (kbi/elude @2016)
 *
 */
#ifndef RAGL_FRAMEBUFFERS_H
#define RAGL_FRAMEBUFFERS_H

#include "ogl/ogl_types.h"
#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"

/** TODO */
PUBLIC raGL_framebuffers raGL_framebuffers_create(ogl_context context_gl);

/** TODO */
PUBLIC void raGL_framebuffers_get_framebuffer(raGL_framebuffers       in_framebuffers,
                                              uint32_t                in_n_attachments,
                                              const ral_texture_view* in_opt_color_attachments,
                                              const ral_texture_view  in_opt_ds_attachment,
                                              raGL_framebuffer*       out_framebuffer_ptr);

/** TODO */
PUBLIC void raGL_framebuffers_release(raGL_framebuffers framebuffers);


#endif /* RALG_FRAMEBUFFERS_H */