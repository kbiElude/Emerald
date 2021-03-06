#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_utils.h"
#include "raGL/raGL_vaos.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_utils.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"


typedef struct _raGL_vaos_vao_vertex_attribute
{
    ral_vertex_input_rate input_rate;
    bool                  is_integer;
    uint32_t              location;
    GLboolean             normalized;
    GLuint                relativeoffset;
    GLint                 size;
    GLenum                type;

    _raGL_vaos_vao_vertex_attribute()
    {
        input_rate     = RAL_VERTEX_INPUT_RATE_UNKNOWN;
        is_integer     = false;
        normalized     = GL_FALSE;
        relativeoffset = 0;
        size           = 0;
        type           = UINT32_MAX;
    }

    static _raGL_vaos_vao_vertex_attribute create_from_ral_gfx_state_va(const ral_gfx_state_vertex_attribute& attribute,
                                                                        uint32_t                              location)
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
        result.location       = location;
        result.normalized     = (format_type == RAL_FORMAT_TYPE_SNORM) ||
                                (format_type == RAL_FORMAT_TYPE_UNORM);
        result.input_rate     = attribute.input_rate;
        result.relativeoffset = attribute.offset;
        result.size           = format_n_components;
        result.type           = raGL_utils_get_ogl_enum_for_ral_format_type(format_type);

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
    ral_gfx_state_vertex_attribute* vas_gl;
    ral_gfx_state_vertex_attribute* vas_ref;
    raGL_vaos_vertex_buffer*        vbs_gl;
    raGL_vaos_vertex_buffer*        vbs_ref;

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
    vas_gl       = nullptr;
    vas_ref      = nullptr;
    vbs_gl       = nullptr;
    vbs_ref      = nullptr;
}

/** TODO */
_raGL_vao::~_raGL_vao()
{
    ogl_context context_gl = nullptr;

    ASSERT_DEBUG_SYNC(vao_id == 0,
                      "VAO not released GL-side at _raGL_vaos_vao destruction time");

    if (vas_gl != nullptr)
    {
        delete [] vas_gl;

        vas_gl = nullptr;
    }

    if (vas_ref != nullptr)
    {
        delete [] vas_ref;

        vas_ref = nullptr;
    }

    if (vbs_gl != nullptr)
    {
        delete[] vbs_gl;

        vbs_gl = nullptr;
    }

    if (vbs_ref != nullptr)
    {
        delete[] vbs_ref;

        vbs_ref = nullptr;
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
        result  = (in_vertex_attributes == nullptr && vas_ref == nullptr) ||
                  (in_vertex_attributes != nullptr && vas_ref != nullptr);
    }

    if (result                          &&
        in_vertex_attributes != nullptr &&
        in_vertex_buffers    != nullptr)
    {
        for (uint32_t n_vb = 0;
                      n_vb < in_n_vertex_buffers && result;
                    ++n_vb)
        {
            if (in_vertex_buffers[n_vb].buffer_raGL != nullptr)
            {
                result = in_vertex_buffers   [n_vb] == vbs_ref[n_vb] && 
                         in_vertex_attributes[n_vb] == vas_ref[n_vb];
            }
        }
    }

    return result;
}


/* Forward declarations */
PRIVATE ral_present_job _raGL_vaos_bake_vao_renderer_thread_callback    (ral_context                                                context,
                                                                         void*                                                      vaos_raw_ptr,
                                                                         const ral_rendering_handler_rendering_callback_frame_data* unused);
PRIVATE ral_present_job _raGL_vaos_release_vaos_renderer_thread_callback(ral_context                                                context,
                                                                         void*                                                      vaos_raw_ptr,
                                                                         const ral_rendering_handler_rendering_callback_frame_data* unused);


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
    uint32_t              n_map_entries     = 0;
    ral_rendering_handler rendering_handler = nullptr;

    raGL_backend_get_property(backend_gl,
                              RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                             &rendering_handler);

    /* Make sure no VAO is being baked at call time */
    if (bake_cs != nullptr)
    {
        system_critical_section_enter(bake_cs);
        system_critical_section_leave(bake_cs);

        system_critical_section_release(bake_cs);
        bake_cs = nullptr;
    }

    /* Release all created VAOs GL-side */
    ral_rendering_handler_request_rendering_callback(rendering_handler,
                                                     _raGL_vaos_release_vaos_renderer_thread_callback,
                                                     this,
                                                     false); /* present_after_executed */

    /* Release everything else */
    system_hash64map_get_property(n_vbs_to_vao_map,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_map_entries);

    for (uint32_t n_map_entry = 0;
                  n_map_entry < n_map_entries;
                ++n_map_entry)
    {
        _raGL_vao*              vao_ptr    = nullptr;
        system_resizable_vector vao_vector = nullptr;

        system_hash64map_get_element_at(n_vbs_to_vao_map,
                                        n_map_entry,
                                       &vao_vector,
                                        nullptr); /* result_hash_ptr */

        while (system_resizable_vector_pop(vao_vector,
                                          &vao_ptr) )
        {
            delete vao_ptr;
        }

        system_resizable_vector_release(vao_vector);
    }

    system_hash64map_release(n_vbs_to_vao_map);
}


/** TODO */
PRIVATE _raGL_vao* _raGL_vaos_bake_vao(_raGL_vaos*                           in_vaos_ptr,
                                       raGL_buffer                           in_index_buffer,
                                       uint32_t                              in_n_vertex_buffers,
                                       const ral_gfx_state_vertex_attribute* in_vertex_attributes,
                                       const raGL_vaos_vertex_buffer*        in_vertex_buffers)
{
    _raGL_vao*            new_vao_ptr       = new (std::nothrow) _raGL_vao;
    ral_rendering_handler rendering_handler = nullptr;
    GLuint                result_vao_id     = 0;

    ASSERT_ALWAYS_SYNC(new_vao_ptr != nullptr,
                       "Out of memory");

    raGL_backend_get_property(in_vaos_ptr->backend_gl,
                              RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                             &rendering_handler);

    /* Convert input vertex attribute data to vertex array & vertex buffer representation */
    std::vector<_raGL_vaos_vao_vertex_attribute> baked_vas;
    std::vector<_raGL_vaos_vao_vertex_buffer>    baked_vbs;

    for (uint32_t n_vb = 0;
                  n_vb < in_n_vertex_buffers;
                ++n_vb)
    {
        const raGL_vaos_vertex_buffer* vb_ptr = in_vertex_buffers + n_vb;

        if (vb_ptr->buffer_raGL == nullptr)
        {
            continue;
        }

        const ral_gfx_state_vertex_attribute* attribute_ptr     = in_vertex_attributes + n_vb;
        _raGL_vaos_vao_vertex_attribute       current_va        = _raGL_vaos_vao_vertex_attribute::create_from_ral_gfx_state_va(*attribute_ptr,
                                                                                                                                n_vb);
        _raGL_vaos_vao_vertex_buffer          current_vb;
        auto                                  baked_va_iterator = std::find(baked_vas.begin(),
                                                                            baked_vas.end(),
                                                                            current_va);

        if (std::find(baked_vas.begin(),
                      baked_vas.end(),
                      current_va) == baked_vas.end() )
        {
            baked_vas.push_back(current_va);

            baked_va_iterator = baked_vas.begin() + (baked_vas.size() - 1);
        }

        ASSERT_DEBUG_SYNC(baked_va_iterator != baked_vas.end(),
                          "This should never happen");

        current_vb.buffer   = vb_ptr->buffer_raGL;
        current_vb.offset   = (GLintptr) (vb_ptr->start_offset);
        current_vb.stride   = attribute_ptr->stride;
        current_vb.va_index = n_vb;

        baked_vbs.push_back(current_vb);
    }

    /* In order to create the VAO GL-side, we need to move to the rendering thread */
    {
        system_critical_section_enter(in_vaos_ptr->bake_cs);

        in_vaos_ptr->bake_index_buffer  = in_index_buffer;
        in_vaos_ptr->bake_n_vas         = baked_vas.size();
        in_vaos_ptr->bake_vas_ptr       = (in_vaos_ptr->bake_n_vas > 0) ? &baked_vas[0] : nullptr;
        in_vaos_ptr->bake_n_vbs         = baked_vbs.size();
        in_vaos_ptr->bake_result_vao_id = 0;
        in_vaos_ptr->bake_vbs_ptr       = (in_vaos_ptr->bake_n_vbs > 0) ? &baked_vbs[0] : nullptr;

        ral_rendering_handler_request_rendering_callback(rendering_handler,
                                                         _raGL_vaos_bake_vao_renderer_thread_callback,
                                                         in_vaos_ptr,
                                                         false); /* present_after_executed */

        result_vao_id = in_vaos_ptr->bake_result_vao_id;

        system_critical_section_leave(in_vaos_ptr->bake_cs);
    }

    ASSERT_DEBUG_SYNC(result_vao_id != 0,
                      "Failed to bake a VAO");

    new_vao_ptr->index_buffer = in_index_buffer;
    new_vao_ptr->n_vbs        = in_n_vertex_buffers;
    new_vao_ptr->vao_id       = result_vao_id;

    if (baked_vbs.size() != 0)
    {
        new_vao_ptr->vas_gl  = new (std::nothrow) ral_gfx_state_vertex_attribute[baked_vas.size()];
        new_vao_ptr->vbs_gl  = new (std::nothrow) raGL_vaos_vertex_buffer       [baked_vbs.size()];

        ASSERT_ALWAYS_SYNC(new_vao_ptr->vas_gl != nullptr &&
                           new_vao_ptr->vbs_gl != nullptr,
                           "Out of memory");

        memcpy(new_vao_ptr->vas_gl,
               &baked_vas[0],
               sizeof(ral_gfx_state_vertex_attribute) * baked_vas.size());
        memcpy(new_vao_ptr->vbs_gl,
              &baked_vbs[0],
               sizeof(raGL_vaos_vertex_buffer) * baked_vbs.size());
    }
    else
    {
        new_vao_ptr->vas_gl = nullptr;
        new_vao_ptr->vbs_gl = nullptr;
    }

    if (in_vertex_attributes != nullptr &&
        in_vertex_buffers    != nullptr)
    {
        new_vao_ptr->vas_ref = new (std::nothrow) ral_gfx_state_vertex_attribute[in_n_vertex_buffers];
        new_vao_ptr->vbs_ref = new (std::nothrow) raGL_vaos_vertex_buffer       [in_n_vertex_buffers];

        ASSERT_ALWAYS_SYNC(new_vao_ptr->vas_ref != nullptr &&
                           new_vao_ptr->vbs_ref != nullptr,
                           "Out of memory");

        memcpy(new_vao_ptr->vas_ref,
               in_vertex_attributes,
               sizeof(ral_gfx_state_vertex_attribute) * in_n_vertex_buffers);
        memcpy(new_vao_ptr->vbs_ref,
               in_vertex_buffers,
               sizeof(raGL_vaos_vertex_buffer) * in_n_vertex_buffers);
    }
    else
    {
        new_vao_ptr->vas_ref = nullptr;
        new_vao_ptr->vbs_ref = nullptr;
    }

    return new_vao_ptr;
}

/** TODO */
PRIVATE ral_present_job _raGL_vaos_bake_vao_renderer_thread_callback(ral_context                                                context,
                                                                     void*                                                      vaos_raw_ptr,
                                                                     const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    /* Note: The originating thread is blocking a CS during execution of this call-back.
     *       vaos_ptr->bake_* variables are guaranteed not to change until
     *       _raGL_vaos_bake_vao_renderer_thread_callback() leaves.
     */
    ogl_context                       context_gl      = nullptr;
    const ogl_context_gl_entrypoints* entrypoints_ptr = nullptr;
    GLuint                            result_vao_id   = 0;
    _raGL_vaos*                       vaos_ptr        = reinterpret_cast<_raGL_vaos*>(vaos_raw_ptr);

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);
    ogl_context_get_property(context_gl,
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

            entrypoints_ptr->pGLVertexAttribIFormat(current_va_ptr->location,
                                                    current_va_ptr->size,
                                                    current_va_ptr->type,
                                                    current_va_ptr->relativeoffset);
        }
        else
        {
            entrypoints_ptr->pGLVertexAttribFormat(current_va_ptr->location,
                                                   current_va_ptr->size,
                                                   current_va_ptr->type,
                                                   current_va_ptr->normalized,
                                                   current_va_ptr->relativeoffset);
        }

        ASSERT_DEBUG_SYNC(current_va_ptr->input_rate != RAL_VERTEX_INPUT_RATE_UNKNOWN,
                          "Invalid input rate specified for a vertex array");

        entrypoints_ptr->pGLVertexBindingDivisor(current_va_ptr->location,
                                                 (current_va_ptr->input_rate == RAL_VERTEX_INPUT_RATE_PER_VERTEX) ? 0
                                                                                                                  : 1);
    }

    /* Set up VBs */
    for (uint32_t n_vb = 0;
                  n_vb < vaos_ptr->bake_n_vbs;
                ++n_vb)
    {
        GLuint                              buffer_raGL_id = 0;
        const _raGL_vaos_vao_vertex_buffer* current_vb_ptr = vaos_ptr->bake_vbs_ptr + n_vb;

        raGL_buffer_get_property(current_vb_ptr->buffer,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &buffer_raGL_id);

        entrypoints_ptr->pGLBindVertexBuffer(n_vb,
                                             buffer_raGL_id,
                                             current_vb_ptr->offset,
                                             current_vb_ptr->stride);
    }

    /* Set up VA/VB bindings */
    for (uint32_t n_vb = 0;
                  n_vb < vaos_ptr->bake_n_vbs;
                ++n_vb)
    {
        const _raGL_vaos_vao_vertex_buffer* current_vb_ptr = vaos_ptr->bake_vbs_ptr + n_vb;

        entrypoints_ptr->pGLEnableVertexAttribArray(current_vb_ptr->va_index);
        entrypoints_ptr->pGLVertexAttribBinding    (current_vb_ptr->va_index,
                                                    n_vb);
    }

    /* All done */
    vaos_ptr->bake_result_vao_id = result_vao_id;

    /* No need for a present job - we fire GL calls directly */
    return nullptr;
}

/** TODO */
PRIVATE ral_present_job _raGL_vaos_release_vaos_renderer_thread_callback(ral_context                                                context,
                                                                         void*                                                      vaos_raw_ptr,
                                                                         const ral_rendering_handler_rendering_callback_frame_data* unused)
{
    ogl_context                 context_gl      = nullptr;
    ogl_context_gl_entrypoints* entrypoints_ptr = nullptr;
    _raGL_vaos*                 vaos_ptr        = reinterpret_cast<_raGL_vaos*>(vaos_raw_ptr);
    uint32_t                    n_vao_vectors   = 0;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);
    ogl_context_get_property(context_gl,
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

        system_hash64map_get_element_at     (vaos_ptr->n_vbs_to_vao_map,
                                             n_vao_vector,
                                            &vaos,
                                             nullptr); /* result_hash_ptr */
        system_resizable_vector_get_property(vaos,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_vaos);

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

    /* No need for a present job - we fire GL calls directly */
    return nullptr;
}


/* Please see header for specification */
PUBLIC void raGL_vao_get_property(const raGL_vao    vao,
                                  raGL_vao_property property,
                                  void**            out_result_ptr)
{
    const _raGL_vao* vao_ptr = reinterpret_cast<const _raGL_vao*>(vao);

    switch (property)
    {
        case RAGL_VAO_PROPERTY_ID:
        {
            *reinterpret_cast<GLuint*>(out_result_ptr) = vao_ptr->vao_id;

            break;
        }

        case RAGL_VAO_PROPERTY_INDEX_BUFFER:
        {
            *reinterpret_cast<raGL_buffer*>(out_result_ptr) = vao_ptr->index_buffer;

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
PUBLIC raGL_vao raGL_vaos_get_vao(raGL_vaos                             in_vaos,
                                  raGL_buffer                           in_opt_index_buffer,
                                  uint32_t                              in_n_vertex_buffers,
                                  const ral_gfx_state_vertex_attribute* in_vertex_attributes,
                                  const raGL_vaos_vertex_buffer*        in_vertex_buffers)
{
    raGL_vao                result_vao  = nullptr;
    _raGL_vaos*             vaos_ptr    = reinterpret_cast<_raGL_vaos*>(in_vaos);
    system_resizable_vector vaos_vector = nullptr;

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
                result_vao = reinterpret_cast<raGL_vao>(vao_ptr);

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

    if (result_vao == nullptr)
    {
        /* Generate the VAO and cache it */
        _raGL_vao* new_vao_ptr = nullptr;

        new_vao_ptr = _raGL_vaos_bake_vao(vaos_ptr,
                                          in_opt_index_buffer,
                                          in_n_vertex_buffers,
                                          in_vertex_attributes,
                                          in_vertex_buffers);

        LOG_INFO("Baked a new VAO");

        system_resizable_vector_push(vaos_vector,
                                     new_vao_ptr);

        result_vao = reinterpret_cast<raGL_vao>(new_vao_ptr);
    }

    /* All done */
    ASSERT_DEBUG_SYNC(result_vao != nullptr,
                      "Null VAO about to be returned.");

    return result_vao;
}

/* Please see header for specification */
PUBLIC void raGL_vaos_release(raGL_vaos in_vaos)
{
    delete reinterpret_cast<_raGL_vaos*>(in_vaos);
}