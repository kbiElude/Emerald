/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "demo/nodes/nodes_video_pass_renderer.h"
#include "demo/demo_timeline.h"
#include "demo/demo_timeline_segment_node.h"
#include "demo/demo_timeline_video_segment.h"
#include "ogl/ogl_context.h"  /* TODO: Remove OGL dep */
#include "ogl/ogl_pipeline.h" /* TODO: Remove OGL dep */
#include "ral/ral_utils.h"
#include "system/system_critical_section.h"


typedef struct _nodes_video_pass_renderer
{
    /* CS used to serialize deinit/get/render/set calls */
    system_critical_section cs;

    ogl_context                 context;
    demo_timeline_segment_node  node;           /* DO NOT release */
    demo_timeline_video_segment parent_segment; /* DO NOT release */
    ogl_pipeline                pipeline;
    demo_timeline               timeline;       /* DO NOT release */

    demo_timeline_segment_node_output_id output_id;
    uint32_t                             rendering_pipeline_stage_id;


    _nodes_video_pass_renderer(ogl_context                in_context,
                               demo_timeline_segment      in_segment,
                               demo_timeline_segment_node in_node)
    {
        ASSERT_DEBUG_SYNC(in_context != NULL,
                          "Input rendering context is NULL");

        context                     = in_context;
        cs                          = system_critical_section_create();
        node                        = in_node;
        output_id                   = -1;
        parent_segment              = (demo_timeline_video_segment) in_segment;
        rendering_pipeline_stage_id = -1;

        demo_timeline_video_segment_get_property(parent_segment,
                                                 DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_TIMELINE,
                                                &timeline);
        demo_timeline_get_property              (timeline,
                                                 DEMO_TIMELINE_PROPERTY_RENDERING_PIPELINE,
                                                &pipeline);
    }

    ~_nodes_video_pass_renderer()
    {
        if (cs != NULL)
        {
            system_critical_section_release(cs);

            cs = NULL;
        } /* if (cs != NULL) */
    }
} _nodes_video_pass_renderer;


/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void nodes_video_pass_renderer_deinit(demo_timeline_segment_node_private node)
{
    _nodes_video_pass_renderer* node_ptr = (_nodes_video_pass_renderer*) node;

    ASSERT_DEBUG_SYNC(node != NULL,
                      "Input node is NULL");

    system_critical_section_enter(node_ptr->cs);
    system_critical_section_leave(node_ptr->cs);

    delete (_nodes_video_pass_renderer*) node;
}

/** Please see header for spec */
PUBLIC bool nodes_video_pass_renderer_get_property(demo_timeline_segment_node_private node,
                                                   int                                property,
                                                   void*                              out_result_ptr)
{
    _nodes_video_pass_renderer* node_ptr = (_nodes_video_pass_renderer*) node;
    bool                        result   = false;

    /* Sanity checks */
    if (node_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node is NULL");

        goto end;
    }

    /* Retrieve the result */
    system_critical_section_enter(node_ptr->cs);

    switch (property)
    {
        case NODES_VIDEO_PASS_RENDERER_PROPERTY_RENDERING_PIPELINE_STAGE_ID:
        {
            *(uint32_t*) out_result_ptr = node_ptr->rendering_pipeline_stage_id;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized nodes_video_pass_renderer_property value getter request.");

            goto end_with_cs_leave;
        }
    } /* switch (property) */

    /* All done */
    result = true;

end_with_cs_leave:
    system_critical_section_leave(node_ptr->cs);

end:
    return result;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL demo_timeline_segment_node_private nodes_video_pass_renderer_init(demo_timeline_segment      segment,
                                                                                                demo_timeline_segment_node node,
                                                                                                void*                      unused1,
                                                                                                void*                      unused2,
                                                                                                void*                      unused3)
{
    _nodes_video_pass_renderer* new_node_ptr = new (std::nothrow) _nodes_video_pass_renderer(ogl_context_get_current_context(),
                                                                                             segment,
                                                                                             node);

    ASSERT_DEBUG_SYNC(new_node_ptr != NULL,
                      "Out of memory");

    if (new_node_ptr != NULL)
    {
        /* Add an output which uses the same color texture format as the rendering context. */
        ral_texture_format              fb_color_format = RAL_TEXTURE_FORMAT_UNKNOWN;
        ral_texture_component           texture_components[4];
        demo_texture_output_declaration texture_output_info;

        ogl_context_get_property(new_node_ptr->context,
                                 OGL_CONTEXT_PROPERTY_DEFAULT_FBO_COLOR_TEXTURE_FORMAT,
                                &fb_color_format);

        texture_components[0] = RAL_TEXTURE_COMPONENT_RED;
        texture_components[1] = RAL_TEXTURE_COMPONENT_GREEN;
        texture_components[2] = RAL_TEXTURE_COMPONENT_BLUE;
        texture_components[3] = RAL_TEXTURE_COMPONENT_ALPHA;

        texture_output_info.output_name                       = system_hashed_ansi_string_create("Output");
        texture_output_info.texture_components                = texture_components;
        texture_output_info.texture_needs_full_mipmap_chain   = false;
        texture_output_info.texture_n_layers                  = 1;
        texture_output_info.texture_n_samples                 = 1;
        texture_output_info.texture_n_start_layer_index       = 0;
        texture_output_info.texture_size.mode                 = DEMO_TEXTURE_SIZE_MODE_PROPORTIONAL;
        texture_output_info.texture_size.proportional_size[0] = 1.0f;
        texture_output_info.texture_size.proportional_size[1] = 1.0f;
        texture_output_info.texture_type                      = RAL_TEXTURE_TYPE_2D;

        ral_utils_get_texture_format_property(fb_color_format,
                                              RAL_TEXTURE_FORMAT_PROPERTY_N_COMPONENTS,
                                             &texture_output_info.texture_n_components);

        if (!demo_timeline_segment_node_add_texture_output(node,
                                                          &texture_output_info,
                                                          &new_node_ptr->output_id) )
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not add a new texture output to the pass renderer video segment node.");
        }
    } /* if (new_node_ptr != NULL) */

    return (demo_timeline_segment_node_private) new_node_ptr;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL bool nodes_video_pass_renderer_render(demo_timeline_segment_node_private node,
                                                                    uint32_t                           frame_index,
                                                                    system_time                        frame_time,
                                                                    const int32_t*                     rendering_area_px_topdown)
{
    _nodes_video_pass_renderer* node_ptr = (_nodes_video_pass_renderer*) node;
    bool                        result   = false;

    /* Sanity checks */
    if (node == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node instance is NULL");

        goto end;
    }

    /* Draw the requested pipeline stage */
    system_critical_section_enter(node_ptr->cs);

    if (node_ptr->rendering_pipeline_stage_id != -1)
    {
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
    _nodes_video_pass_renderer* node_ptr = (_nodes_video_pass_renderer*) node;
    bool                        result   = false;

    /* Sanity checks */
    if (node_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node is NULL");

        goto end;
    }

    /* Retrieve the result */
    system_critical_section_enter(node_ptr->cs);

    switch (property)
    {
        case NODES_VIDEO_PASS_RENDERER_PROPERTY_RENDERING_PIPELINE_STAGE_ID:
        {
            node_ptr->rendering_pipeline_stage_id = *(uint32_t*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized nodes_video_pass_renderer_property value getter request.");

            goto end_with_cs_leave;
        }
    } /* switch (property) */

    /* All done */
    result = true;

end_with_cs_leave:
    system_critical_section_leave(node_ptr->cs);

end:
    return result;
}