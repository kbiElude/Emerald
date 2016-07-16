#ifndef RAGL_VAOS_H
#define RAGL_VAOS_H

#include "raGL/raGL_types.h"

typedef enum
{
    /* not settable; GLuint */
    RAGL_VAO_PROPERTY_ID,

    /* not settable; raGL_buffer */
    RAGL_VAO_PROPERTY_INDEX_BUFFER
} raGL_vao_property;

typedef struct raGL_vaos_vertex_buffer
{
    raGL_buffer buffer_raGL;
    uint32_t    start_offset;

    bool operator==(const raGL_vaos_vertex_buffer& in) const
    {
        return (buffer_raGL  == in.buffer_raGL) &&
               (start_offset == in.start_offset);
    }
} raGL_vaos_vertex_buffer;


/** TODO */
PUBLIC void raGL_vao_get_property(const raGL_vao    vao,
                                  raGL_vao_property property,
                                  void**            out_result_ptr);

/** TODO */
PUBLIC raGL_vaos raGL_vaos_create(raGL_backend in_backend_gl);

/** TODO */
PUBLIC raGL_vao raGL_vaos_get_vao(raGL_vaos                             in_vaos,
                                  raGL_buffer                           in_opt_index_buffer,
                                  uint32_t                              in_n_vertex_buffers,
                                  const ral_gfx_state_vertex_attribute* in_vertex_attributes,
                                  const raGL_vaos_vertex_buffer*        in_vertex_buffers);

/** TODO */
PUBLIC void raGL_vaos_release(raGL_vaos in_vaos);


#endif /* RAGL_VAOS_H */