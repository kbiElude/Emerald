#include "shared.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_framebuffers.h"
#include "system/system_resizable_vector.h"

typedef struct _raGL_framebuffers_fbo
{
    ral_texture_view* color_attachments;
    ral_texture_view  ds_attachment;
    uint32_t          n_color_attachments;

    raGL_framebuffer fbo;


    bool is_a_match(uint32_t                in_n_attachments,
                    const ral_texture_view* in_opt_color_attachments,
                    ral_texture_view        in_opt_ds_attachment) const
    {
        bool result = (in_n_attachments     == n_color_attachments &&
                       in_opt_ds_attachment == ds_attachment);

        if (result)
        {
            result = (memcmp(in_opt_color_attachments,
                             color_attachments,
                             sizeof(ral_texture_view) * in_n_attachments) == 0);
        }

        return result;
    }

    bool uses_attachment(ral_texture_view in_attachment) const
    {
        if (ds_attachment == in_attachment)
        {
            return true;

        }

        for (uint32_t n_color_attachment = 0;
                      n_color_attachment < n_color_attachments;
                    ++n_color_attachment)
        {
            if (color_attachments[n_color_attachment] == in_attachment)
            {
                return true;
            }
        }

        return false;
    }

    explicit _raGL_framebuffers_fbo(ogl_context             in_context_gl,
                                    const ral_texture_view* in_color_attachments,
                                    uint32_t                in_n_color_attachments,
                                    ral_texture_view        in_ds_attachment)
    {
        if (in_n_color_attachments > 0)
        {
            color_attachments = new (std::nothrow) ral_texture_view[in_n_color_attachments];

            ASSERT_DEBUG_SYNC(color_attachments != nullptr,
                              "Out of memory");

            memcpy(color_attachments,
                   in_color_attachments,
                   sizeof(ral_texture_view) * in_n_color_attachments);
        }
        else
        {
            color_attachments = nullptr;
        }

        ds_attachment       = in_ds_attachment;
        fbo                 = raGL_framebuffer_create(in_context_gl,
                                                      in_n_color_attachments,
                                                      in_color_attachments,
                                                      in_ds_attachment);
        n_color_attachments = in_n_color_attachments;
    }

    ~_raGL_framebuffers_fbo()
    {
        if (color_attachments != nullptr)
        {
            delete [] color_attachments;

            color_attachments = nullptr;
        }
    }
} _raGL_framebuffers_fbo;

typedef struct _raGL_framebuffers
{
    ogl_context             context_gl;
    system_resizable_vector fbos;       /* todo: we could probably use a n_attachments->fbo vector hashmap here*/

    _raGL_framebuffers(ogl_context in_context_gl)
    {
        context_gl = in_context_gl;
        fbos       = system_resizable_vector_create(16); /* capacity */
    }

    ~_raGL_framebuffers()
    {
        if (fbos != nullptr)
        {
            uint32_t n_fbos = 0;

            system_resizable_vector_get_property(fbos,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_fbos);

            for (uint32_t n_fbo = 0;
                          n_fbo < n_fbos;
                        ++n_fbo)
            {
                _raGL_framebuffers_fbo* fbo_ptr = nullptr;

                system_resizable_vector_get_element_at(fbos,
                                                       n_fbo,
                                                      &fbo_ptr);

                delete fbo_ptr;
            }

            system_resizable_vector_release(fbos);
            fbos = nullptr;
        }
    }
} _raGL_framebuffers;


/** Please see header for specification */
PUBLIC raGL_framebuffers raGL_framebuffers_create(ogl_context context_gl)
{
    _raGL_framebuffers* framebuffers_ptr = new (std::nothrow) _raGL_framebuffers(context_gl);

    ASSERT_ALWAYS_SYNC(framebuffers_ptr != nullptr,
                       "Out of memory");

    return (raGL_framebuffers) framebuffers_ptr;
}

/** Please see header for specification */
PUBLIC void raGL_framebuffers_delete_fbos_with_attachment(raGL_framebuffers      in_framebuffers,
                                                          const ral_texture_view in_attachment)
{
    _raGL_framebuffers* framebuffers_ptr = reinterpret_cast<_raGL_framebuffers*>(in_framebuffers);
    bool                has_found        = false;
    uint32_t            n_fbos           = 0;

    system_resizable_vector_get_property(framebuffers_ptr->fbos,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_fbos);

    for (uint32_t n_fbo = 0;
                  n_fbo < n_fbos;
                  )
    {
        _raGL_framebuffers_fbo* fbo_ptr = nullptr;

        system_resizable_vector_get_element_at(framebuffers_ptr->fbos,
                                               n_fbo,
                                              &fbo_ptr);

        if (fbo_ptr->uses_attachment(in_attachment) )
        {
            raGL_framebuffer_release(fbo_ptr->fbo);

            delete fbo_ptr;

            system_resizable_vector_delete_element_at(framebuffers_ptr->fbos,
                                                      n_fbo);

            --n_fbos;
        }
        else
        {
            ++n_fbo;
        }
    }

}

/** Please see header for specification */
PUBLIC void raGL_framebuffers_get_framebuffer(raGL_framebuffers       in_framebuffers,
                                              uint32_t                in_n_attachments,
                                              const ral_texture_view* in_opt_color_attachments,
                                              const ral_texture_view  in_opt_ds_attachment,
                                              raGL_framebuffer*       out_framebuffer_ptr)
{
    _raGL_framebuffers*     framebuffers_ptr = reinterpret_cast<_raGL_framebuffers*>(in_framebuffers);
    bool                    has_found        = false;
    uint32_t                n_fbos           = 0;
    _raGL_framebuffers_fbo* result_fbo_ptr   = nullptr;

    system_resizable_vector_get_property(framebuffers_ptr->fbos,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_fbos);

    for (uint32_t n_fbo = 0;
                  n_fbo < n_fbos && result_fbo_ptr == nullptr;
                ++n_fbo)
    {
        _raGL_framebuffers_fbo* fbo_ptr = nullptr;

        system_resizable_vector_get_element_at(framebuffers_ptr->fbos,
                                               n_fbo,
                                              &fbo_ptr);

        if (fbo_ptr->is_a_match(in_n_attachments,
                                in_opt_color_attachments,
                                in_opt_ds_attachment) )
        {
            result_fbo_ptr = fbo_ptr;
        }
    }

    if (result_fbo_ptr == nullptr)
    {
        result_fbo_ptr = new _raGL_framebuffers_fbo(framebuffers_ptr->context_gl,
                                                    in_opt_color_attachments,
                                                    in_n_attachments,
                                                    in_opt_ds_attachment);

        ASSERT_DEBUG_SYNC(result_fbo_ptr != nullptr,
                          "Out of memory");

        system_resizable_vector_push(framebuffers_ptr->fbos,
                                     result_fbo_ptr);
    }

    *out_framebuffer_ptr = reinterpret_cast<raGL_framebuffer>(result_fbo_ptr->fbo);
}


/** Please see header for specification */
PUBLIC void raGL_framebuffers_release(raGL_framebuffers framebuffers)
{
    delete reinterpret_cast<_raGL_framebuffers*>(framebuffers);
}