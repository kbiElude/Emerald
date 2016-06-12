/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "demo/nodes/nodes_postprocessing_output.h"
#include "demo/demo_timeline_segment.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_framebuffer.h"
#include "ral/ral_context.h"
#include "ral/ral_texture.h"
#include "ral/ral_utils.h"
#include "system/system_callback_manager.h"


typedef struct _nodes_postprocessing_output
{
    ral_framebuffer                     blit_src_fb;
    demo_timeline_segment_node_input_id color_data_node_input_id;
    ral_context                         context;
    ral_texture                         data_texture;
    demo_timeline_segment_node          node;
    demo_timeline_segment               segment;

    _nodes_postprocessing_output(ral_context                         in_context,
                                 demo_timeline_segment               in_segment,
                                 demo_timeline_segment_node          in_node,
                                 demo_timeline_segment_node_input_id in_color_data_node_input_id)
    {
        blit_src_fb              = NULL;
        color_data_node_input_id = in_color_data_node_input_id;
        context                  = in_context;
        data_texture             = NULL;
        node                     = in_node;
        segment                  = in_segment;
    }
} _nodes_postprocessing_output;


/* Forward declarations */
PRIVATE void _nodes_postprocessing_on_texture_attached_callback(const void*                   callback_data,
                                                                      void*                   user_arg);
PRIVATE void _nodes_postprocessing_on_texture_detached_callback(const void*                   callback_data,
                                                                      void*                   user_arg);
PRIVATE bool _nodes_postprocessing_output_update_subscriptions (_nodes_postprocessing_output* node_ptr,
                                                                bool                          should_subscribe);


/** TODO */
PRIVATE void _nodes_postprocessing_on_texture_attached_callback(const void* callback_data,
                                                                      void* user_arg)
{
    demo_timeline_segment_node_callback_texture_attached_callback_argument* callback_data_ptr = (demo_timeline_segment_node_callback_texture_attached_callback_argument*) callback_data;
    _nodes_postprocessing_output*                                           node_data_ptr     = (_nodes_postprocessing_output*)                                           user_arg;

    if (callback_data_ptr->is_input_id                                            &&
        callback_data_ptr->id          == node_data_ptr->color_data_node_input_id &&
        callback_data_ptr->node        == node_data_ptr->node)
    {
        /* Cache the texture */
        ASSERT_DEBUG_SYNC(callback_data_ptr->texture != NULL,
                          "NULL texture attached to a post-processing output node");
        ASSERT_DEBUG_SYNC(node_data_ptr->data_texture == NULL,
                          "Texture attached to a node input with another texture attachment already defined");

        node_data_ptr->data_texture = callback_data_ptr->texture;

        /* Update the blit source framebuffer */
        ral_framebuffer_set_attachment_2D(node_data_ptr->blit_src_fb,
                                          RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                          0, /* index */
                                          callback_data_ptr->texture,
                                          0 /* n_mipmap */);
    }
}

/** TODO */
PRIVATE void _nodes_postprocessing_on_texture_detached_callback(const void* callback_data,
                                                                      void* user_arg)
{
    demo_timeline_segment_node_callback_texture_detached_callback_argument* callback_data_ptr = (demo_timeline_segment_node_callback_texture_detached_callback_argument*) callback_data;
    _nodes_postprocessing_output*                                           node_data_ptr     = (_nodes_postprocessing_output*)                                           user_arg;

    if (callback_data_ptr->is_input_id                                            &&
        callback_data_ptr->id == node_data_ptr->color_data_node_input_id &&
        callback_data_ptr->node == node_data_ptr->node)
    {
        /* Drop the attachment */
        ASSERT_DEBUG_SYNC(node_data_ptr->data_texture != NULL,
                          "Texture attached to a node input with another texture attachment already defined");

        node_data_ptr->data_texture = NULL;

        ral_framebuffer_set_attachment_2D(node_data_ptr->blit_src_fb,
                                          RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                          0,    /* index      */
                                          NULL, /* texture_2d */
                                          0     /* n_mipmap   */);
    }
}

/** TODO */
PRIVATE bool _nodes_postprocessing_output_update_subscriptions(_nodes_postprocessing_output* node_ptr,
                                                               bool                          should_subscribe)
{
    system_callback_manager segment_callback_manager = NULL;

    demo_timeline_segment_get_property(node_ptr->segment,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_CALLBACK_MANAGER,
                                      &segment_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(segment_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_ATTACHED_TO_NODE,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_on_texture_attached_callback,
                                                        node_ptr);
        system_callback_manager_subscribe_for_callbacks(segment_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_DETACHED_FROM_NODE,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_on_texture_detached_callback,
                                                        node_ptr);
    }
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(segment_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_ATTACHED_TO_NODE,
                                                           _nodes_postprocessing_on_texture_attached_callback,
                                                           node_ptr);
        system_callback_manager_unsubscribe_from_callbacks(segment_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_DETACHED_FROM_NODE,
                                                           _nodes_postprocessing_on_texture_detached_callback,
                                                           node_ptr);
    }

    return true;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void nodes_postprocessing_output_deinit(demo_timeline_segment_node_private node)
{
    _nodes_postprocessing_output* node_ptr = (_nodes_postprocessing_output*) node;

    if (node_ptr->blit_src_fb != nullptr
    {
        ral_context_delete_objects(node_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER,
                                   1, /* n_objects */
                                   (const void**) &node_ptr->blit_src_fb);

        node_ptr->blit_src_fb = nullptr;
    }

    _nodes_postprocessing_output_update_subscriptions(node_ptr,
                                                      false /* should_subscribe */);

    delete node_ptr;
    node_ptr = nullptr;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL demo_timeline_segment_node_private nodes_postprocessing_output_init(demo_timeline_segment      segment,
                                                                                                  demo_timeline_segment_node node,
                                                                                                  ral_context                context)
{
    ral_texture_format            color_texture_format;
    _nodes_postprocessing_output* new_node_data_ptr = nullptr;
    demo_texture_io_declaration   new_node_input_declaration;
    bool                          result;
    demo_timeline_segment_node_id texture_input_id;

    /* Determine texture format of the system framebuffer's color output. */
    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_COLOR_ATTACHMENT_TEXTURE_FORMAT,
                            &color_texture_format);

    /* Add a new input to the node we own */
    new_node_input_declaration.is_attachment_required = true;
    new_node_input_declaration.name                   = system_hashed_ansi_string_create("Color data");
    new_node_input_declaration.texture_format         = color_texture_format;
    new_node_input_declaration.texture_n_layers       = 1;
    new_node_input_declaration.texture_n_samples      = 1;
    new_node_input_declaration.texture_type           = RAL_TEXTURE_TYPE_2D;

    ral_utils_get_texture_format_property(color_texture_format,
                                          RAL_TEXTURE_FORMAT_PROPERTY_N_COMPONENTS,
                                         &new_node_input_declaration.texture_n_components);

    result = demo_timeline_segment_node_add_texture_input(node,
                                                         &new_node_input_declaration,
                                                         &texture_input_id);

    ASSERT_DEBUG_SYNC(result,
                      "Could not add a new texture input to the post-processing output node.");

    /* Create, initialize & return the new descriptor. */
    new_node_data_ptr = new _nodes_postprocessing_output(context,
                                                         segment,
                                                         node,
                                                         texture_input_id);

    ral_context_create_framebuffers(context,
                                    1, /* n_framebuffers */
                                   &new_node_data_ptr->blit_src_fb);

    _nodes_postprocessing_output_update_subscriptions(new_node_data_ptr,
                                                      true /* should_subscribe */);

    /* All done */
    return (demo_timeline_segment_node_private) new_node_data_ptr;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL bool nodes_postprocessing_output_render(demo_timeline_segment_node_private node,
                                                                      uint32_t                           frame_index,
                                                                      system_time                        frame_time,
                                                                      const int32_t*                     rendering_area_px_topdown)
{
    raGL_framebuffer                  blit_src_fb_raGL    = nullptr;
    GLuint                            blit_src_fb_raGL_id = -1;
    ogl_context                       context_gl          = nullptr;
    const ogl_context_gl_entrypoints* entrypoints_ptr     = nullptr;
    uint32_t                          fb_size[2];
    _nodes_postprocessing_output*     node_ptr            = (_nodes_postprocessing_output*) node;
    ral_framebuffer                   system_fb           = nullptr; 
    raGL_framebuffer                  system_fb_raGL      = nullptr;
    GLuint                            system_fb_raGL_id   = -1;

    context_gl = ral_context_get_gl_context(node_ptr->context);

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    ral_context_get_property(node_ptr->context,
                             RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFERS,
                            &system_fb);

    blit_src_fb_raGL = ral_context_get_framebuffer_gl(node_ptr->context,
                                                      node_ptr->blit_src_fb);
    system_fb_raGL   = ral_context_get_framebuffer_gl(node_ptr->context,
                                                      system_fb);

    raGL_framebuffer_get_property(blit_src_fb_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &blit_src_fb_raGL_id);
    raGL_framebuffer_get_property(system_fb_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &system_fb_raGL_id);

    ral_context_get_property(node_ptr->context,
                             RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_SIZE,
                             fb_size);

    /* Blit the data we were provided to the system FB.
     *
     * TODO: This could actually be a copy op, unless we're going to supersample one day. */
    entrypoints_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                        system_fb_raGL_id);
    entrypoints_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                        blit_src_fb_raGL_id);

    entrypoints_ptr->pGLBlitFramebuffer(0, /* srcX0 */
                                        0, /* srcY0 */
                                        fb_size[0],
                                        fb_size[1],
                                        0, /* dstX0 */
                                        0, /* dstY0 */
                                        fb_size[0],
                                        fb_size[1],
                                        GL_COLOR_BUFFER_BIT,
                                        GL_NEAREST);

    return true;
}
