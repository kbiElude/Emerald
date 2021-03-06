#if 0

TODO: Remove. ogl_pipeline is deprecated.

/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "demo/nodes/nodes_video_pass_renderer.h"
#include "demo/demo_timeline.h"
#include "demo/demo_timeline_segment.h"
#include "demo/demo_timeline_segment_node.h"
#include "ral/ral_context.h"
#include "ral/ral_texture.h"
#include "ral/ral_utils.h"
#include "system/system_critical_section.h"


enum
{
    NODES_VIDEO_PASS_RENDERER_TEXTURE_OUTPUT,

    /* Always last */
    NODES_VIDEO_PASS_RENDERER_COUNT
};

typedef struct _nodes_video_pass_renderer
{
    /* CS used to serialize deinit/get/render/set calls */
    system_critical_section cs;

    ral_context                 context;
    demo_timeline_segment_node  node;           /* DO NOT release */
    demo_timeline_segment       parent_segment; /* DO NOT release */
    demo_timeline               timeline;       /* DO NOT release */

    demo_timeline_segment_node_output_id output_id;
    ral_texture                          output_texture;


    _nodes_video_pass_renderer(ral_context                in_context,
                               demo_timeline_segment      in_segment,
                               demo_timeline_segment_node in_node)
    {
        ASSERT_DEBUG_SYNC(in_context != nullptr,
                          "Input rendering context is NULL");

        context                     = in_context;
        cs                          = system_critical_section_create();
        node                        = in_node;
        output_id                   = -1;
        output_texture              = nullptr;
        parent_segment              = reinterpret_cast<demo_timeline_segment>(in_segment);

        demo_timeline_segment_get_property(parent_segment,
                                           DEMO_TIMELINE_SEGMENT_PROPERTY_TIMELINE,
                                          &timeline);
    }

    ~_nodes_video_pass_renderer()
    {
        if (cs != nullptr)
        {
            system_critical_section_release(cs);

            cs = nullptr;
        }
    }
} _nodes_video_pass_renderer;


/** Please see header for spec */
PUBLIC void nodes_video_pass_renderer_deinit(demo_timeline_segment_node_private node)
{
    _nodes_video_pass_renderer* node_ptr = reinterpret_cast<_nodes_video_pass_renderer*>(node);

    ASSERT_DEBUG_SYNC(node != nullptr,
                      "Input node is NULL");

    delete reinterpret_cast<_nodes_video_pass_renderer*>(node);
}

/** Please see header for spec */
PUBLIC bool nodes_video_pass_renderer_get_property(demo_timeline_segment_node_private node,
                                                   int                                property,
                                                   void*                              out_result_ptr)
{
    _nodes_video_pass_renderer* node_ptr = reinterpret_cast<_nodes_video_pass_renderer*>(node);
    bool                        result   = false;

    /* Sanity checks */
    if (node_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node is NULL");

        goto end;
    }

    /* Retrieve the result */
    system_critical_section_enter(node_ptr->cs);

    switch (property)
    {
        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized nodes_video_pass_renderer_property value getter request.");

            goto end_with_cs_leave;
        }
    }

    /* All done */
    result = true;

end_with_cs_leave:
    system_critical_section_leave(node_ptr->cs);

end:
    return result;
}

/** Please see header for spec */
PUBLIC bool nodes_video_pass_renderer_get_texture_memory_allocation_details(demo_timeline_segment_node_private node,
                                                                            uint32_t                           n_allocation,
                                                                            demo_texture_memory_allocation*    out_memory_allocation_data_ptr)
{
    _nodes_video_pass_renderer* node_data_ptr = reinterpret_cast<_nodes_video_pass_renderer*>(node);
    bool                        result        = true;

    if (node == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node instance is NULL");

        result = false;
        goto end;
    } /* if (node == NULL) */

    switch (n_allocation)
    {
        case NODES_VIDEO_PASS_RENDERER_TEXTURE_OUTPUT:
        {
            ral_format fb_color_format = RAL_FORMAT_UNKNOWN;

            ral_context_get_property(node_data_ptr->context,
                                     RAL_CONTEXT_PROPERTY_SYSTEM_FB_BACK_BUFFER_COLOR_FORMAT,
                                    &fb_color_format);

            out_memory_allocation_data_ptr->bound_texture             = node_data_ptr->output_texture;
            out_memory_allocation_data_ptr->components[0]             = RAL_TEXTURE_COMPONENT_RED;
            out_memory_allocation_data_ptr->components[1]             = RAL_TEXTURE_COMPONENT_GREEN;
            out_memory_allocation_data_ptr->components[2]             = RAL_TEXTURE_COMPONENT_BLUE;
            out_memory_allocation_data_ptr->components[3]             = RAL_TEXTURE_COMPONENT_ALPHA;
            out_memory_allocation_data_ptr->format                    = fb_color_format;
            out_memory_allocation_data_ptr->name                      = system_hashed_ansi_string_create("Video pass renderer output texture");
            out_memory_allocation_data_ptr->needs_full_mipmap_chain   = false;
            out_memory_allocation_data_ptr->n_layers                  = 1;
            out_memory_allocation_data_ptr->n_samples                 = 1;
            out_memory_allocation_data_ptr->size.mode                 = DEMO_TEXTURE_SIZE_MODE_PROPORTIONAL;
            out_memory_allocation_data_ptr->size.proportional_size[0] = 1.0f;
            out_memory_allocation_data_ptr->size.proportional_size[1] = 1.0f;
            out_memory_allocation_data_ptr->type                      = RAL_TEXTURE_TYPE_2D;

            break;
        }

        default:
        {
            result = false;
        }
    }

end:
    return result;
}

/** Please see header for spec */
PUBLIC demo_timeline_segment_node_private nodes_video_pass_renderer_init(demo_timeline_segment      segment,
                                                                         demo_timeline_segment_node node,
                                                                         ral_context                context)
{
    _nodes_video_pass_renderer* new_node_ptr = new (std::nothrow) _nodes_video_pass_renderer(context,
                                                                                             segment,
                                                                                             node);

    ASSERT_DEBUG_SYNC(new_node_ptr != nullptr,
                      "Out of memory");

    if (new_node_ptr != nullptr)
    {
        /* Add an output which uses the same color texture format as the rendering context. */
        ral_format                  fb_color_format = RAL_FORMAT_UNKNOWN;
        demo_texture_io_declaration texture_output_info;

        ral_context_get_property(new_node_ptr->context,
                                 RAL_CONTEXT_PROPERTY_SYSTEM_FB_BACK_BUFFER_COLOR_FORMAT,
                                &fb_color_format);

        texture_output_info.name              = system_hashed_ansi_string_create("Output");
        texture_output_info.texture_format    = fb_color_format;
        texture_output_info.texture_n_layers  = 1;
        texture_output_info.texture_n_samples = 1;
        texture_output_info.texture_type      = RAL_TEXTURE_TYPE_2D;

        ral_utils_get_format_property(fb_color_format,
                                      RAL_FORMAT_PROPERTY_N_COMPONENTS,
                                     &texture_output_info.texture_n_components);

        if (!demo_timeline_segment_node_add_texture_output(node,
                                                          &texture_output_info,
                                                          &new_node_ptr->output_id) )
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not add a new texture output to the pass renderer video segment node.");
        }
    }

    return (demo_timeline_segment_node_private) new_node_ptr;
}

/** Please see header for spec */
PUBLIC bool nodes_video_pass_renderer_render(demo_timeline_segment_node_private node,
                                             uint32_t                           frame_index,
                                             system_time                        frame_time,
                                             const int32_t*                     rendering_area_px_topdown)
{
    _nodes_video_pass_renderer* node_ptr = reinterpret_cast<_nodes_video_pass_renderer*>(node);
    bool                        result   = false;

    /* Sanity checks */
    if (node == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node instance is NULL");

        goto end;
    }

    /* Draw the requested pipeline stage */
    system_critical_section_enter(node_ptr->cs);

    if (node_ptr->rendering_pipeline_stage_id != -1)
    {
        /* TODO: This will be made API-agnostic once we have command buffers support in RAL */
        ogl_context                       context_gl      = ral_context_get_gl_context(node_ptr->context);
        const ogl_context_gl_entrypoints* entrypoints_ptr = nullptr;
        raGL_framebuffer                  fb_raGL         = ral_context_get_framebuffer_gl(node_ptr->context,
                                                                                           node_ptr->rendering_framebuffer);
        uint32_t                          fb_raGL_id      = 0;

        ogl_context_get_property     (context_gl,
                                      OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                     &entrypoints_ptr);
        raGL_framebuffer_get_property(fb_raGL,
                                      RAGL_FRAMEBUFFER_PROPERTY_ID,
                                     &fb_raGL_id);

        entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                            fb_raGL_id);

        result = ogl_pipeline_draw_stage(node_ptr->pipeline,
                                         node_ptr->rendering_pipeline_stage_id,
                                         frame_index,
                                         frame_time,
                                         rendering_area_px_topdown);
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "No rendering pipeline stage ID selected");
    }

    /* All done */
    system_critical_section_leave(node_ptr->cs);
    result = true;

end:
    return result;
}

/** Please see header for spec */
PUBLIC bool nodes_video_pass_renderer_set_property(demo_timeline_segment_node_private node,
                                                   int                                property,
                                                   const void*                        data)
{
    _nodes_video_pass_renderer* node_ptr = reinterpret_cast<_nodes_video_pass_renderer*>(node);
    bool                        result   = false;

    /* Sanity checks */
    if (node_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node is NULL");

        goto end;
    }

    /* Retrieve the result */
    system_critical_section_enter(node_ptr->cs);

    switch (property)
    {
        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized nodes_video_pass_renderer_property value getter request.");

            goto end_with_cs_leave;
        }
    }

    /* All done */
    result = true;

end_with_cs_leave:
    system_critical_section_leave(node_ptr->cs);

end:
    return result;
}

/** Please see header for specification */
PUBLIC void nodes_video_pass_renderer_set_texture_memory_allocation(demo_timeline_segment_node_private node,
                                                                    uint32_t                           n_allocation,
                                                                    ral_texture                        texture)
{
    _nodes_video_pass_renderer* node_ptr = reinterpret_cast<_nodes_video_pass_renderer*>(node);

    if (node_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node is NULL");

        goto end;
    }

    switch (n_allocation)
    {
        case NODES_VIDEO_PASS_RENDERER_TEXTURE_OUTPUT:
        {
            demo_texture_attachment_declaration new_attachment;

            /* We're going to render stuff to this texture. */
            node_ptr->output_texture = texture;

            ral_framebuffer_set_attachment_2D(node_ptr->rendering_framebuffer,
                                              RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                              0,  /* index */
                                              node_ptr->output_texture,
                                              0); /* n_mipmap */

            /* Attach the texture to the node's output, so that node users can access the texture. */
            new_attachment.texture_ral = texture;

            demo_timeline_segment_node_attach_texture_to_texture_io(node_ptr->node,
                                                                    false, /* is_input_id */
                                                                    node_ptr->output_id,
                                                                   &new_attachment);
            break;
        }
    }

end:
    ;
}

#endif