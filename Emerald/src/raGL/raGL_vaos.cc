#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_utils.h"
#include "raGL/raGL_vaos.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_utils.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


typedef struct _raGL_vaos_vao_vertex_attribute
{
    bool      is_integer;
    GLboolean normalized;
    GLuint    relativeoffset;
    GLint     size;
    GLenum    type;

    _raGL_vaos_vao_vertex_attribute()
    {
        is_integer     = false;
        normalized     = GL_FALSE;
        relativeoffset = 0;
        size           = 0;
        type           = UINT32_MAX;
    }

    static _raGL_vaos_vao_vertex_attribute create_from_ral_gfx_state_va(const ral_gfx_state_vertex_attribute& attribute)
    {
        uint32_t                        format_n_components;
        ral_format_type                 format_type;
        _raGL_vaos_vao_vertex_attribute result;

        ral_utils_get_format_property(attribute.format,
                                      RAL_FORMAT_PROPERTY_FORMAT_TYPE,
                                     &format_type);
        ral_utils_get_format_property(attribute.format,
                                      RAL_FORMAT_PROPERTY_N_COMPONENTS,
                                     &format_n_components);

        result.is_integer     = (format_type == RAL_FORMAT_TYPE_SINT)  ||
                                (format_type == RAL_FORMAT_TYPE_UINT);
        result.normalized     = (format_type == RAL_FORMAT_TYPE_SNORM) ||
                                (format_type == RAL_FORMAT_TYPE_UNORM);
        result.relativeoffset = attribute.offset;
        result.size           = format_n_components;
        result.type           = raGL_utils_get_ogl_data_format_for_ral_format(attribute.format);

        return result;
    }

    bool operator==(const _raGL_vaos_vao_vertex_attribute& in) const
    {
        return (in.is_integer     == is_integer     &&
                in.normalized     == normalized     &&
                in.relativeoffset == relativeoffset &&
                in.size           == size           &&
                in.type           == type);
    }
} _raGL_vaos_vao_vertex_attribute;

typedef struct _raGL_vaos_vao_vertex_buffer
{
    raGL_buffer buffer;
    GLintptr    offset;
    GLintptr    stride;
    uint32_t    va_index; /* UINT32_MAX if undefined */

    _raGL_vaos_vao_vertex_buffer()
    {
        buffer   = nullptr;
        offset   = 0;
        stride   = 0;
        va_index = UINT32_MAX;
    }

    bool operator==(const _raGL_vaos_vao_vertex_buffer& in) const
    {
        /* NOTE: va_index is ignored here. */
        return (buffer == in.buffer &&
                offset == in.offset &&
                stride == in.stride);
    }
} _raGL_vaos_vao_vertex_buffer;

typedef struct _raGL_vao
{
    raGL_buffer                     index_buffer;
    uint32_t                        n_vbs;
    GLuint                          vao_id;
    ral_gfx_state_vertex_attribute* vas; /* does NOT correspond to VAO VA state */
    raGL_vaos_vertex_buffer*        vbs; /* does NOT correspond to VAO VB state */

    bool is_a_match(raGL_buffer                           in_index_buffer,
                    uint32_t                              in_n_vertex_buffers,
                    const ral_gfx_state_vertex_attribute* in_vertex_attributes,
                    const raGL_vaos_vertex_buffer*        in_vertex_buffers)  const;

     _raGL_vao();
    ~_raGL_vao();
} _raGL_vao;

typedef struct _raGL_vaos
{
    raGL_backend     backend_gl;
    system_hash64map n_vbs_to_vao_map; /* maps n_vertex_buffers to _raGL_vaos_vao instance */

    system_critical_section          bake_cs;
    raGL_buffer                      bake_index_buffer;
    uint32_t                         bake_n_vas;
    uint32_t                         bake_n_vbs;
    volatile GLuint                  bake_result_vao_id;
    _raGL_vaos_vao_vertex_attribute* bake_vas_ptr;
    _raGL_vaos_vao_vertex_buffer*    bake_vbs_ptr;

     _raGL_vaos(raGL_backend in_backend_gl);
    ~_raGL_vaos();
} _raGL_vaos;


/** TODO */
_raGL_vao::_raGL_vao()
{
    index_buffer = nullptr;
    n_vbs        = 0;
    vao_id       = 0;
    vas          = nullptr;
    vbs          = nullptr;
}

/** TODO */
_raGL_vao::~_raGL_vao()
{
    ogl_context context_gl = nullptr;

    ASSERT_DEBUG_SYNC(vao_id == 0,
                      "VAO not released GL-side at _raGL_vaos_vao destruction time");

    if (vas != nullptr)
    {
        delete [] vas;

        vas = nullptr;
    }

    if (vbs != nullptr)
    {
        delete[] vbs;

        vbs = nullptr;
    }
}

/** TODO */
bool _raGL_vao::is_a_match(raGL_buffer                           in_index_buffer,
                           uint32_t                              in_n_vertex_buffers,
                           const ral_gfx_state_vertex_attribute* in_vertex_attributes,
                           const raGL_vaos_vertex_buffer*        in_vertex_buffers)  const
{
    bool result = (index_buffer == in_index_buffer)     &&
                  (n_vbs        == in_n_vertex_buffers);

    ASSERT_DEBUG_SYNC(in_n_vertex_buffers == 0                                    ||
                      in_n_vertex_buffers != 0 && in_vertex_buffers != nullptr,
                      "Array of VA descriptors is null");

    if (result)
    {
        for (uint32_t n_vb = 0;
                      n_vb < in_n_vertex_buffers && result;
                    ++n_vb)
        {
            result = (in_vertex_buffers   [n_vb] == vbs[n_vb]) &&
                     (in_vertex_attributes[n_vb] == vas[n_vb]);
        }
    }

    return result;
}


/* Forward declarations */
PRIVATE void _raGL_vaos_bake_vao_renderer_thread_callback    (ogl_context context,
                                                              void*       vaos_raw_ptr);
PRIVATE void _raGL_vaos_release_vaos_renderer_thread_callback(ogl_context context,
                                                              void*       vaos_raw_ptr);


/** TODO */
_raGL_vaos::_raGL_vaos(raGL_backend in_backend_gl)
{
    backend_gl       = in_backend_gl;
    bake_cs          = system_critical_section_create();
    n_vbs_to_vao_map = system_hash64map_create(sizeof(_raGL_vao*) );
}

/** TODO */
_raGL_vaos::~_raGL_vaos()
{
    ogl_context context_gl    = nullptr;
    uint32_t    n_map_entries = 0;

    raGL_backend_get_property(backend_gl,
                              RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                             &context_gl);

    /* Make sure no VAO is being baked at call time */
    if (bake_cs != nullptr)
    {
        system_critical_section_enter(bake_cs);
        system_critical_section_leave(bake_cs);

        system_critical_section_release(bake_cs);
        bake_cs = nullptr;
    }

    /* Release all created VAOs GL-side */
    ogl_context_request_callback_from_context_thread(context_gl,
                                                     _raGL_vaos_release_vaos_renderer_thread_callback,
                                                     this);

    /* Release everything else */
    system_hash64map_get_property(n_vbs_to_vao_map,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_map_entries);

    for (uint32_t n_map_entry = 0;
                  n_map_entry < n_map_entries;
                ++n_map_entry)
    {
        _raGL_vao* vao_ptr = nullptr;

        system_hash64map_get(n_vbs_to_vao_map,
                             n_map_entry,
                            &vao_ptr);

        delete vao_ptr;
    }

    system_hash64map_release(n_vbs_to_vao_map);
}


/** TODO */
PRIVATE _raGL_vao* _raGL_vaos_bake_vao(_raGL_vaos*                           in_vaos_ptr,
                                       raGL_buffer                           in_index_buffer,
                                       uint32_t                              in_n_vertex_attributes,
                                       const ral_gfx_state_vertex_attribute* in_vertex_attributes,
                                       const raGL_vaos_vertex_buffer*        in_vertex_buffers)
{
    ogl_context context_gl                = nullptr;
    GLuint      index_buffer_raGL_id      = 0;
    uint32_t    index_buffer_start_offset = 0;
    _raGL_vao*  new_vao_ptr               = new (std::nothrow) _raGL_vao;
    GLuint      result_vao_id             = 0;

    ASSERT_ALWAYS_SYNC(new_vao_ptr != nullptr,
                       "Out of memory");

    raGL_backend_get_property(in_vaos_ptr->backend_gl,
                              RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                             &context_gl);

    raGL_buffer_get_property(in_index_buffer,
                             RAGL_BUFFER_PROPERTY_ID,
                            &index_buffer_raGL_id);
    raGL_buffer_get_property(in_index_buffer,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &index_buffer_start_offset);

    /* Convert input vertex attribute data to vertex array & vertex buffer representation */
    std::vector<_raGL_vaos_vao_vertex_attribute> baked_vas;
    std::vector<_raGL_vaos_vao_vertex_buffer>    baked_vbs;

    for (uint32_t n_va = 0;
                  n_va < in_n_vertex_attributes;
                ++n_va)
    {
        const ral_gfx_state_vertex_attribute* attribute_ptr = in_vertex_attributes + n_va;
        _raGL_vaos_vao_vertex_attribute       current_va    = _raGL_vaos_vao_vertex_attribute::create_from_ral_gfx_state_va(*attribute_ptr);

        if (std::find(baked_vas.begin(),
                      baked_vas.end(),
                      current_va) == baked_vas.end() )
        {
            baked_vas.push_back(current_va);
        }
    }

    for (uint32_t n_vb = 0;
                  n_vb < in_n_vertex_attributes;
                ++n_vb)
    {
        const ral_gfx_state_vertex_attribute* attribute_ptr     = in_vertex_attributes + n_vb;
        _raGL_vaos_vao_vertex_attribute       current_va        = _raGL_vaos_vao_vertex_attribute::create_from_ral_gfx_state_va(*attribute_ptr);
        _raGL_vaos_vao_vertex_buffer          current_vb;
        const raGL_vaos_vertex_buffer*        vb_ptr            = in_vertex_buffers + n_vb;
        auto                                  baked_va_iterator = std::find(baked_vas.begin(),
                                                                            baked_vas.end(),
                                                                            current_va);

        ASSERT_DEBUG_SYNC(baked_va_iterator != baked_vas.end(),
                          "This should never happen");

        current_vb.buffer   = vb_ptr->buffer_raGL;
        current_vb.offset   = (GLintptr) (vb_ptr->start_offset);
        current_vb.stride   = attribute_ptr->stride;
        current_vb.va_index = baked_va_iterator - baked_vas.begin();

        baked_vbs.push_back(current_vb);
    }

    /* In order to create the VAO GL-side, we need to move to the rendering thread */
    {
        system_critical_section_enter(in_vaos_ptr->bake_cs);

        in_vaos_ptr->bake_index_buffer  = in_index_buffer;
        in_vaos_ptr->bake_n_vas         = baked_vas.size();
        in_vaos_ptr->bake_vas_ptr       = &baked_vas[0];
        in_vaos_ptr->bake_n_vbs         = baked_vbs.size();
        in_vaos_ptr->bake_result_vao_id = 0;
        in_vaos_ptr->bake_vbs_ptr       = &baked_vbs[0];

        ogl_context_request_callback_from_context_thread(context_gl,
                                                         _raGL_vaos_bake_vao_renderer_thread_callback,
                                                         in_vaos_ptr);

        result_vao_id = in_vaos_ptr->bake_result_vao_id;

        system_critical_section_leave(in_vaos_ptr->bake_cs);
    }

    ASSERT_DEBUG_SYNC(result_vao_id != 0,
                      "Failed to bake a VAO");

    new_vao_ptr->index_buffer = in_index_buffer;
    new_vao_ptr->n_vbs        = in_n_vertex_attributes;
    new_vao_ptr->vao_id       = result_vao_id;

    if (in_n_vertex_attributes != 0)
    {
        new_vao_ptr->vas = new (std::nothrow) ral_gfx_state_vertex_attribute[in_n_vertex_attributes];
        new_vao_ptr->vbs = new (std::nothrow) raGL_vaos_vertex_buffer       [in_n_vertex_attributes];

        ASSERT_ALWAYS_SYNC(new_vao_ptr->vas != nullptr &&
                           new_vao_ptr->vbs != nullptr,
                           "Out of memory");

        memcpy(new_vao_ptr->vas,
               in_vertex_attributes,
               sizeof(ral_gfx_state_vertex_attribute) * in_n_vertex_attributes);
        memcpy(new_vao_ptr->vbs,
               in_vertex_buffers,
               sizeof(raGL_vaos_vertex_buffer) * in_n_vertex_attributes);
    }
    else
    {
        new_vao_ptr->vas = nullptr;
        new_vao_ptr->vbs = nullptr;
    }

    return new_vao_ptr;
}

/** TODO */
PRIVATE void _raGL_vaos_bake_vao_renderer_thread_callback(ogl_context context,
                                                          void*       vaos_raw_ptr)
{
    /* Note: The originating thread is blocking a CS during execution of this call-back.
     *       vaos_ptr->bake_* variables are guaranteed not to change until
     *       _raGL_vaos_bake_vao_renderer_thread_callback() leaves.
     */
    const ogl_context_gl_entrypoints* entrypoints_ptr = nullptr;
    GLuint                            result_vao_id   = 0;
    _raGL_vaos*                       vaos_ptr        = reinterpret_cast<_raGL_vaos*>(vaos_raw_ptr);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    entrypoints_ptr->pGLGenVertexArrays(1,
                                       &result_vao_id);
    entrypoints_ptr->pGLBindVertexArray(result_vao_id);

    /* Set up index buffer binding */
    if (vaos_ptr->bake_index_buffer != nullptr)
    {
        GLuint index_buffer_raGL_id = 0;

        raGL_buffer_get_property(vaos_ptr->bake_index_buffer,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &index_buffer_raGL_id);

        entrypoints_ptr->pGLBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                       index_buffer_raGL_id);
    }

    /* Set up VAs */
    for (uint32_t n_va = 0;
                  n_va < vaos_ptr->bake_n_vas;
                ++n_va)
    {
        const _raGL_vaos_vao_vertex_attribute* current_va_ptr = vaos_ptr->bake_vas_ptr + n_va;

        if (current_va_ptr->is_integer)
        {
            ASSERT_DEBUG_SYNC(current_va_ptr->normalized == GL_FALSE,
                              "An integer VA cannot be normalized.");

            entrypoints_ptr->pGLVertexAttribIFormat(n_va,
                                                    current_va_ptr->size,
                                                    current_va_ptr->type,
                                                    current_va_ptr->relativeoffset);
        }
        else
        {
            entrypoints_ptr->pGLVertexAttribFormat(n_va,
                                                   current_va_ptr->size,
                                                   current_va_ptr->type,
                                                   current_va_ptr->normalized,
                                                   current_va_ptr->relativeoffset);
        }
    }

    /* Set up VBs */
    for (uint32_t n_vb = 0;
                  n_vb < vaos_ptr->bake_n_vbs;
                ++n_vb)
    {
        GLuint                              buffer_raGL_id           = 0;
        uint32_t                            buffer_raGL_start_offset = 0;
        const _raGL_vaos_vao_vertex_buffer* current_vb_ptr           = vaos_ptr->bake_vbs_ptr + n_vb;

        raGL_buffer_get_property(current_vb_ptr->buffer,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &buffer_raGL_id);
        raGL_buffer_get_property(current_vb_ptr->buffer,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &buffer_raGL_start_offset);

        entrypoints_ptr->pGLBindVertexBuffer(n_vb,
                                             buffer_raGL_id,
                                             current_vb_ptr->offset + buffer_raGL_start_offset,
                                             current_vb_ptr->stride);
    }

    /* Set up VA/VB bindings */
    for (uint32_t n_vb = 0;
                  n_vb < vaos_ptr->bake_n_vbs;
                ++n_vb)
    {
        const _raGL_vaos_vao_vertex_buffer* current_vb_ptr = vaos_ptr->bake_vbs_ptr + n_vb;

        entrypoints_ptr->pGLVertexAttribBinding(current_vb_ptr->va_index,
                                                n_vb);
    }

    /* All done */
    vaos_ptr->bake_result_vao_id = result_vao_id;
}

/** TODO */
PRIVATE void _raGL_vaos_release_vaos_renderer_thread_callback(ogl_context context,
                                                              void*       vaos_raw_ptr)
{
    ogl_context_gl_entrypoints* entrypoints_ptr = nullptr;
    _raGL_vaos*                 vaos_ptr        = reinterpret_cast<_raGL_vaos*>(vaos_raw_ptr);
    uint32_t                    n_vao_vectors   = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    system_hash64map_get_property(vaos_ptr->n_vbs_to_vao_map,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_vao_vectors);

    for (uint32_t n_vao_vector = 0;
                  n_vao_vector < n_vao_vectors;
                ++n_vao_vector)
    {
        uint32_t                n_vaos = 0;
        system_resizable_vector vaos   = nullptr;

        system_hash64map_get_element_at(vaos_ptr->n_vbs_to_vao_map,
                                        n_vao_vector,
                                       &vaos,
                                        nullptr); /* result_hash_ptr */

        for (uint32_t n_vao = 0;
                      n_vao < n_vaos;
                    ++n_vao)
        {
            _raGL_vao* vao_ptr = nullptr;

            system_resizable_vector_get_element_at(vaos,
                                                   n_vao,
                                                  &vao_ptr);

            entrypoints_ptr->pGLDeleteVertexArrays(1,
                                                  &vao_ptr->vao_id);

            vao_ptr->vao_id = 0;
        }
    }
}


/* Please see header for specification */
PUBLIC void raGL_vao_get_property(raGL_vao          vao,
                                  raGL_vao_property property,
                                  void**            out_result_ptr)
{
    _raGL_vao* vao_ptr = reinterpret_cast<_raGL_vao*>(vao);

    switch (property)
    {
        case RAGL_VAO_PROPERTY_ID:
        {
            *(GLuint*) out_result_ptr = vao_ptr->vao_id;

            break;
        }

        case RAGL_VAO_PROPERTY_INDEX_BUFFER:
        {
            *(raGL_buffer*) out_result_ptr = vao_ptr->index_buffer;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_vao_property value.");
        }
    }
}


/* Please see header for specification */
PUBLIC raGL_vaos raGL_vaos_create(raGL_backend in_backend_gl)
{
    _raGL_vaos* new_vaos_ptr = new (std::nothrow) _raGL_vaos(in_backend_gl);

    ASSERT_ALWAYS_SYNC(new_vaos_ptr != nullptr,
                       "Out of memory");

    return reinterpret_cast<raGL_vaos>(new_vaos_ptr);
}

/* Please see header for specification */
PUBLIC GLuint raGL_vaos_get_vao(raGL_vaos                       in_vaos,
                                raGL_buffer                     in_opt_index_buffer,
                                uint32_t                        in_n_vertex_buffers,
                                ral_gfx_state_vertex_attribute* in_vertex_attributes,
                                const raGL_vaos_vertex_buffer*  in_vertex_buffers)
{
    GLuint                  result_vao_id = 0;
    _raGL_vaos*             vaos_ptr      = reinterpret_cast<_raGL_vaos*>(in_vaos);
    system_resizable_vector vaos_vector   = nullptr;

    system_hash64map_get(vaos_ptr->n_vbs_to_vao_map,
                         in_n_vertex_buffers,
                        &vaos_vector);

    if (vaos_vector != nullptr)
    {
        uint32_t n_vao_vector_entries = 0;

        system_resizable_vector_get_property(vaos_vector,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_vao_vector_entries);

        for (uint32_t n_vao = 0;
                      n_vao < n_vao_vector_entries;
                    ++n_vao)
        {
            _raGL_vao* vao_ptr = nullptr;

            system_resizable_vector_get_element_at(vaos_vector,
                                                   n_vao,
                                                  &vao_ptr);

            if (vao_ptr->is_a_match(in_opt_index_buffer,
                                    in_n_vertex_buffers,
                                    in_vertex_attributes,
                                    in_vertex_buffers) )
            {
                result_vao_id = vao_ptr->vao_id;

                break;
            }
        }
    }
    else
    {
        vaos_vector = system_resizable_vector_create(4 /* capacity */);

        system_hash64map_insert(vaos_ptr->n_vbs_to_vao_map,
                                in_n_vertex_buffers,
                                vaos_vector,
                                nullptr,  /* on_remove_callback          */
                                nullptr); /* on_remove_callback_user_arg */
    }

    if (result_vao_id == 0)
    {
        /* Generate the VAO and cache it */
        _raGL_vao* new_vao_ptr = nullptr;

        new_vao_ptr = _raGL_vaos_bake_vao(vaos_ptr,
                                            in_opt_index_buffer,
                                            in_n_vertex_buffers,
                                            in_vertex_attributes,
                                            in_vertex_buffers);

        LOG_INFO("Baked a new VAO with ID [%d]",
                 result_vao_id);

        system_resizable_vector_push(vaos_vector,
                                     new_vao_ptr);
    }

    /* All done */
    ASSERT_DEBUG_SYNC(result_vao_id != 0,
                      "Zero VAO id about to be returned.");

end:
    return result_vao_id;
}

/* Please see header for specification */
PUBLIC void raGL_vaos_release(raGL_vaos in_vaos)
{
    delete reinterpret_cast<_raGL_vaos*>(in_vaos);
}