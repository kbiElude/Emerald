/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_programs.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_scene_renderer_frustum_preview.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_text.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include "system/system_resources.h"
#include <string>
#include <sstream>

/*
 * Data BO structure:
 *
 *
 *  FTL--------FTR            FTL: far  top-left     corner (index: 6)
 *  |\         /|             FTR: far  top-right    corner (index: 7)
 *  | \       / |             FBL: far  bottom-left  corner (index: 5)
 *  |  NTL--NTR |             FBR: far  bottom-right corner (index: 8)
 *  FBL-|\--/|-FBR            NBL: near bottom-left  corner (index: 1)
 *   \  | OO | /              NBR: near bottom-right corner (index: 4)
 *    \ |/  \|/               NTL: near top-left     corner (index: 2)
 *     NBL--NBR               NTR: near top-right    corner (index: 3)
 *                            OO:  origin                   (index: 0)
 *
 * n UBYTE indices describing index data for the frustum.
 * n_cameras * 9 vertices (4 comps each)
 * n_lights  * 9 vertices (4 comps each)
 *
 * This implementation does not use ogl_primitive_renderer because
 * it does not support multi draw base vertex calls (not at the moment, at least!),
 * which we can leverage for rendering of multiple frustums.
 */
#define PRIMITIVE_RESTART_TERMINATOR_INDEX (255)

PRIVATE const unsigned char index_data_array[] =
{
    /* side planes */
    0, 1, 5, 6, 2,
    0, 3, 7, 8, 4, 0, PRIMITIVE_RESTART_TERMINATOR_INDEX,

    1, 2, 3, 4, 1,    PRIMITIVE_RESTART_TERMINATOR_INDEX, /* near plane */
    5, 6, 7, 8, 5,    PRIMITIVE_RESTART_TERMINATOR_INDEX, /* far  plane */

    4, 1, 5, 8, 4,    PRIMITIVE_RESTART_TERMINATOR_INDEX, /* bottom plane */
    2, 6, 7, 3, 2,    PRIMITIVE_RESTART_TERMINATOR_INDEX, /* top    plane */
};

#define BO_DATA_INDEX_FTL    (6)
#define BO_DATA_INDEX_FTR    (7)
#define BO_DATA_INDEX_FBL    (5)
#define BO_DATA_INDEX_FBR    (8)
#define BO_DATA_INDEX_NBL    (1)
#define BO_DATA_INDEX_NBR    (4)
#define BO_DATA_INDEX_NTL    (2)
#define BO_DATA_INDEX_NTR    (3)
#define BO_DATA_INDEX_ORIGIN (0)
#define BO_DATA_INDEX_MAX    (8)

#define BO_DATA_INDEX_DATA_OFFSET          (0)
#define BO_DATA_VERTEX_DATA_OFFSET         (sizeof(index_data_array) )
#define N_BYTES_PER_BO_DATA_ENTRY          (9 * sizeof(float) * 4)

/** TODO */
typedef struct _ogl_scene_renderer_frustum_preview_camera
{
    scene_camera       camera;
    ogl_text_string_id camera_text_id;
    ogl_text           text_renderer;

    explicit _ogl_scene_renderer_frustum_preview_camera(__in __notnull scene_camera       in_camera,
                                                        __in __notnull ogl_text           in_text_renderer,
                                                        __in           ogl_text_string_id in_camera_text_id)
    {
        camera         = in_camera;
        camera_text_id = in_camera_text_id;
        text_renderer  = in_text_renderer;

        scene_camera_retain(camera);
    }

    ~_ogl_scene_renderer_frustum_preview_camera()
    {
        if (camera != NULL)
        {
            ogl_text_delete_string(text_renderer,
                                   camera_text_id);

            scene_camera_release(camera);

            camera = NULL;
        }
    }
} _ogl_scene_renderer_frustum_preview_camera;

typedef struct _ogl_scene_renderer_frustum_preview
{
    /* DO NOT retain/release ogl_context copy, as this object is managed
     * by ogl_context and retaining it will cause the rendering context
     * to never release itself.
     *
     */
    system_resizable_vector assigned_cameras; /* holds _ogl_scene_renderer_frustum_preview_camera instances */
    ogl_text_string_id      test_text_id;

    ogl_buffers             buffers; /* owned by context, do NOT release */
    ogl_context             context;
    unsigned char*          data_bo_buffer;
    system_timeline_time    data_bo_buffer_last_update_time;
    unsigned int            data_bo_buffer_size;
    GLuint                  data_bo_id; /* owned by ogl_buffers - do NOT release */
    unsigned int            data_bo_size;
    unsigned int            data_bo_start_offset;
    ogl_program             po;
    ogl_program_ub          po_ub;
    GLuint                  po_ub_bo_id;
    unsigned int            po_ub_bo_size;
    unsigned int            po_ub_bo_start_offset;
    GLint                   po_vp_ub_offset;
    scene                   scene;
    ogl_text                text_renderer;  /* TODO: use a global text renderer */
    GLuint                  vao_id;

    /* The following arrays are used for the multi draw-call. The number of elements
     * is equal to the total number of frustrums to be rendered. */
    unsigned int mdebv_array_size;
    GLint*       mdebv_basevertex_array;
    GLsizei*     mdebv_count_array;
    GLvoid**     mdebv_indices_array;

    _ogl_scene_renderer_frustum_preview()
    {
        assigned_cameras                = system_resizable_vector_create(4, /* capacity */
                                                                         sizeof(_ogl_scene_renderer_frustum_preview_camera*) );
        context                         = NULL;
        data_bo_buffer                  = NULL;
        data_bo_buffer_size             = 0;
        data_bo_buffer_last_update_time = -1;
        data_bo_id                      = 0;
        data_bo_size                    = 0;
        data_bo_start_offset            = -1;
        mdebv_array_size                = 0;
        mdebv_basevertex_array          = NULL;
        mdebv_count_array               = NULL;
        mdebv_indices_array             = NULL;
        po                              = NULL;
        po_ub_bo_id                     = -1;
        po_ub_bo_size                   = 0;
        po_ub_bo_start_offset           = 0;
        po_vp_ub_offset                 = -1;
        scene                           = NULL;
        text_renderer                   = NULL;
        vao_id                          = 0;

        test_text_id = -1;
    }

    ~_ogl_scene_renderer_frustum_preview()
    {
        ASSERT_DEBUG_SYNC(data_bo_id == 0,
                          "Data BO not released at the time ogl_scene_renderer_frustum_preview destructor was called.");

        if (assigned_cameras != NULL)
        {
            _ogl_scene_renderer_frustum_preview_camera* current_camera_ptr = NULL;

            while (system_resizable_vector_pop(assigned_cameras,
                                              &current_camera_ptr) )
            {
                delete current_camera_ptr;

                current_camera_ptr = NULL;
            }

            system_resizable_vector_release(assigned_cameras);
            assigned_cameras = NULL;
        } /* if (assigned_cameras != NULL) */

        if (data_bo_buffer != NULL)
        {
            delete [] data_bo_buffer;

            data_bo_buffer = NULL;
        }

        if (mdebv_basevertex_array != NULL)
        {
            delete [] mdebv_basevertex_array;

            mdebv_basevertex_array = NULL;
        }

        if (mdebv_count_array != NULL)
        {
            delete [] mdebv_count_array;

            mdebv_count_array = NULL;
        }

        if (mdebv_indices_array != NULL)
        {
            delete [] mdebv_indices_array;

            mdebv_indices_array = NULL;
        }

        if (po != NULL)
        {
            ogl_program_release(po);

            po = NULL;
        }

        if (text_renderer != NULL)
        {
            ogl_text_release(text_renderer);

            text_renderer = NULL;
        }
    }
} _ogl_scene_renderer_frustum_preview;


PRIVATE const char* po_fs = "#version 420\n"
                            "\n"
                            "out vec4 result;\n"
                            "\n"
                            "void main()\n"
                            "{\n"
                            "    result = vec4(1.0, 1.0, 1.0, 1.0);\n"
                            "}\n";
PRIVATE const char* po_vs = "#version 420\n"
                            "\n"
                            "in vec4 position;\n"
                            "\n"
                            "uniform dataVS\n"
                            "{\n"
                            "    mat4 vp;\n"
                            "};\n"
                            "\n"
                            "void main()\n"
                            "{\n"
                            "    gl_Position = vp * position;\n"
                            "}\n";


/* Forward declarations */
PRIVATE void _ogl_scene_renderer_frustum_preview_deinit_rendering_thread_callback(__in __notnull ogl_context                          context,
                                                                                  __in __notnull void*                                preview);
PRIVATE void _ogl_scene_renderer_frustum_preview_init_rendering_thread_callback  (__in __notnull ogl_context                          context,
                                                                                  __in __notnull void*                                preview);
PRIVATE void _ogl_scene_renderer_frustum_preview_update_data_bo_buffer           (__in __notnull _ogl_scene_renderer_frustum_preview* preview_ptr,
                                                                                  __in           system_timeline_time                 frame_time);

#ifdef _DEBUG
    PRIVATE void _ogl_scene_renderer_frustum_preview_verify_context_type(__in __notnull ogl_context);
#else
    #define _ogl_scene_renderer_frustum_preview_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _ogl_scene_renderer_frustum_preview_deinit_rendering_thread_callback(__in __notnull ogl_context context,
                                                                                  __in __notnull void*       preview)
{
    const ogl_context_gl_entrypoints*    entry_points = NULL;
    _ogl_scene_renderer_frustum_preview* preview_ptr  = (_ogl_scene_renderer_frustum_preview*) preview;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Release data BO */
    if (preview_ptr->data_bo_id != 0)
    {
        ogl_buffers_free_buffer_memory(preview_ptr->buffers,
                                       preview_ptr->data_bo_id,
                                       preview_ptr->data_bo_start_offset);

        preview_ptr->data_bo_id           = 0;
        preview_ptr->data_bo_start_offset = -1;
    }

    /* Release PO */
    if (preview_ptr->po != NULL)
    {
        ogl_program_release(preview_ptr->po);

        preview_ptr->po = NULL;
    }

    /* Release VAO */
    if (preview_ptr->vao_id != 0)
    {
        entry_points->pGLDeleteVertexArrays(1,
                                           &preview_ptr->vao_id);

        preview_ptr->vao_id = 0;
    }
}

/** TODO */
PRIVATE void _ogl_scene_renderer_frustum_preview_init_rendering_thread_callback(__in __notnull ogl_context context,
                                                                                __in __notnull void*       preview)
{
    const ogl_context_gl_entrypoints*    entry_points = NULL;
    _ogl_scene_renderer_frustum_preview* preview_ptr  = (_ogl_scene_renderer_frustum_preview*) preview;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Instantiate the VAO */
    entry_points->pGLGenVertexArrays(1,
                                    &preview_ptr->vao_id);

    /* Create text renderer instance name */
    const char*               limiter    = "/";
    system_hashed_ansi_string scene_name = NULL;

    scene_get_property(preview_ptr->scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);


    const char* final_renderer_name_parts[] =
    {
        system_hashed_ansi_string_get_buffer(scene_name),
        limiter,
        "frustum preview text renderer"
    };
    const uint32_t n_final_renderer_name_parts = sizeof(final_renderer_name_parts) /
                                                 sizeof(final_renderer_name_parts[0]);


    system_hashed_ansi_string final_renderer_name = system_hashed_ansi_string_create_by_merging_strings(n_final_renderer_name_parts,
                                                                                                        final_renderer_name_parts);

    /* Initialize the text renderer */
    const float text_color[4] = {1, 0, 0, 1};
    const float text_scale    = 0.5f;
    GLint       viewport[4];

    entry_points->pGLGetIntegerv(GL_VIEWPORT,
                                 viewport);

    preview_ptr->text_renderer = ogl_text_create(final_renderer_name,
                                                 context,
                                                 system_resources_get_meiryo_font_table(),
                                                 viewport[2],
                                                 viewport[3]);

    ogl_text_set_text_string_property(preview_ptr->text_renderer,
                                      TEXT_STRING_ID_DEFAULT,
                                      OGL_TEXT_STRING_PROPERTY_COLOR,
                                      text_color);
    ogl_text_set_text_string_property(preview_ptr->text_renderer,
                                      TEXT_STRING_ID_DEFAULT,
                                      OGL_TEXT_STRING_PROPERTY_SCALE,
                                     &text_scale);

    /* Is the PO already registered? */
    static const char*  po_name  = "Frustum preview renderer program";
           ogl_programs programs = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_PROGRAMS,
                            &programs);

    preview_ptr->po = ogl_programs_get_program_by_name(programs,
                                                       system_hashed_ansi_string_create(po_name));

    if (preview_ptr->po != NULL)
    {
        ogl_program_retain(preview_ptr->po);
    }
    else
    {
        /* Set up the PO */
        ogl_shader fs = NULL;
        ogl_shader vs = NULL;

        fs = ogl_shader_create(context,
                               SHADER_TYPE_FRAGMENT,
                               system_hashed_ansi_string_create("Frustum preview renderer FS") );
        vs = ogl_shader_create(context,
                               SHADER_TYPE_VERTEX,
                               system_hashed_ansi_string_create("Frustum preview renderer VS") );

        preview_ptr->po = ogl_program_create(context,
                                             system_hashed_ansi_string_create(po_name),
                                             OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

        if (!ogl_shader_set_body(fs,
                                 system_hashed_ansi_string_create(po_fs)) ||
            !ogl_shader_set_body(vs,
                                 system_hashed_ansi_string_create(po_vs)) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "ogl_shader_set_body() call(s) failed.");
        }

        if (!ogl_program_attach_shader(preview_ptr->po,
                                       fs)              ||
            !ogl_program_attach_shader(preview_ptr->po,
                                       vs) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "ogl_program_attach_shader() call(s) failed.");
        }

        if (!ogl_program_link(preview_ptr->po) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "ogl_program_link() failed.");
        }

        ogl_shader_release(fs);
        ogl_shader_release(vs);
    }

    /* Retrieve PO UB details */
    ogl_program_get_uniform_block_by_name(preview_ptr->po,
                                          system_hashed_ansi_string_create("dataVS"),
                                         &preview_ptr->po_ub);

    ASSERT_DEBUG_SYNC(preview_ptr->po_ub != NULL,
                      "dataVS UB uniform descriptor is NULL");

    ogl_program_ub_get_property(preview_ptr->po_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &preview_ptr->po_ub_bo_size);
    ogl_program_ub_get_property(preview_ptr->po_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_ID,
                               &preview_ptr->po_ub_bo_id);
    ogl_program_ub_get_property(preview_ptr->po_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                               &preview_ptr->po_ub_bo_start_offset);

    /* Retrieve PO uniform locations */
    const ogl_program_uniform_descriptor* po_vp_descriptor = NULL;

    if (!ogl_program_get_uniform_by_name(preview_ptr->po,
                                         system_hashed_ansi_string_create("vp"),
                                        &po_vp_descriptor) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "ogl_program_get_uniform_by_name() failed.");
    }
    else
    {
        preview_ptr->po_vp_ub_offset = po_vp_descriptor->ub_offset;

        ASSERT_DEBUG_SYNC(preview_ptr->po_vp_ub_offset != -1,
                          "VP UB offset is -1");
    }
}

/** TODO */
PRIVATE void _ogl_scene_renderer_frustum_preview_update_data_bo_buffer(__in __notnull _ogl_scene_renderer_frustum_preview* preview_ptr,
                                                                       __in           system_timeline_time                 frame_time,
                                                                       __in __notnull system_matrix4x4                     vp)
{
    ASSERT_DEBUG_SYNC(preview_ptr->data_bo_buffer != NULL,
                      "Data BO buffer is NULL");

    /* Update index data */
    memcpy(preview_ptr->data_bo_buffer,
           index_data_array,
           sizeof(index_data_array) );

    /* Reallocate MDEBV argument space if necessary */
    const uint32_t n_cameras      = system_resizable_vector_get_amount_of_elements(preview_ptr->assigned_cameras);
    bool           realloc_needed = false;

    if (preview_ptr->mdebv_array_size < n_cameras)
    {
        if (preview_ptr->mdebv_basevertex_array != NULL)
        {
            delete [] preview_ptr->mdebv_basevertex_array;

            preview_ptr->mdebv_basevertex_array = NULL;
        }

        if (preview_ptr->mdebv_count_array != NULL)
        {
            delete [] preview_ptr->mdebv_count_array;

            preview_ptr->mdebv_count_array = NULL;
        }

        if (preview_ptr->mdebv_indices_array != NULL)
        {
            delete [] preview_ptr->mdebv_indices_array;

            preview_ptr->mdebv_indices_array = NULL;
        }

        preview_ptr->mdebv_basevertex_array = new (std::nothrow) GLint  [n_cameras];
        preview_ptr->mdebv_count_array      = new (std::nothrow) GLsizei[n_cameras];
        preview_ptr->mdebv_indices_array    = new (std::nothrow) GLvoid*[n_cameras];

        ASSERT_ALWAYS_SYNC(preview_ptr->mdebv_basevertex_array != NULL &&
                           preview_ptr->mdebv_count_array      != NULL &&
                           preview_ptr->mdebv_indices_array    != NULL,
                           "Out of memory");

        preview_ptr->mdebv_array_size = n_cameras;
        realloc_needed                = true;
    } /* if (preview_ptr->mdebv_array_size < n_cameras) */

    /* Iterate over all assigned cameras */
    for (uint32_t n_camera = 0;
                  n_camera < n_cameras;
                ++n_camera)
    {
        /* Retrieve transformation matrix for current camera */
        _ogl_scene_renderer_frustum_preview_camera* camera_ptr                           = NULL;
        scene_graph_node                            current_camera_owner_graph_node      = NULL;
        system_matrix4x4                            current_camera_transformation_matrix = NULL;

        if (!system_resizable_vector_get_element_at(preview_ptr->assigned_cameras,
                                                    n_camera,
                                                   &camera_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve scene_camera instance at index [%d]",
                              n_camera);

            continue;
        } /* if (could not retrieve scene_camera instance) */

        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,
                                  0, /* time is irrelevant for the query */
                                 &current_camera_owner_graph_node);

        ASSERT_DEBUG_SYNC(current_camera_owner_graph_node != NULL,
                          "No owner graph node assigned to the camera");

        scene_graph_node_get_property(current_camera_owner_graph_node,
                                      SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                     &current_camera_transformation_matrix);

        ASSERT_DEBUG_SYNC(current_camera_transformation_matrix != NULL,
                          "No transformation matrix available for the camera");

        /* Compute MVP-space frustum vertex positions */
        unsigned int bo_vertex_offset  = BO_DATA_VERTEX_DATA_OFFSET + n_camera * N_BYTES_PER_BO_DATA_ENTRY;
        float*       bo_vertex_data    = (float*) (preview_ptr->data_bo_buffer + bo_vertex_offset);
        float        frustum_origin[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_fbl[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_fbr[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_ftl[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_ftr[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_nbl[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_nbr[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_ntl[4]    = {0.0f, 0.0f, 0.0f, 1.0f};
        float        frustum_ntr[4]    = {0.0f, 0.0f, 0.0f, 1.0f};

        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_LEFT,
                                  frame_time,
                                  frustum_fbl);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_RIGHT,
                                  frame_time,
                                  frustum_fbr);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_LEFT,
                                  frame_time,
                                  frustum_ftl);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_RIGHT,
                                  frame_time,
                                  frustum_ftr);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_LEFT,
                                  frame_time,
                                  frustum_nbl);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_RIGHT,
                                  frame_time,
                                  frustum_nbr);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_LEFT,
                                  frame_time,
                                  frustum_ntl);
        scene_camera_get_property(camera_ptr->camera,
                                  SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_RIGHT,
                                  frame_time,
                                  frustum_ntr);

        system_matrix4x4_multiply_by_vector4(current_camera_transformation_matrix,
                                             frustum_origin,
                                             bo_vertex_data + 4 * BO_DATA_INDEX_ORIGIN);

        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_FBL,
               frustum_fbl,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_FBR,
               frustum_fbr,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_FTL,
               frustum_ftl,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_FTR,
               frustum_ftr,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_NBL,
               frustum_nbl,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_NBR,
               frustum_nbr,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_NTL,
               frustum_ntl,
               sizeof(float) * 4);
        memcpy(bo_vertex_data + 4 * BO_DATA_INDEX_NTR,
               frustum_ntr,
               sizeof(float) * 4);

        /* Position the label at camera origin. Now, the origin we have calculated
         * is in world space, so we need to move it to window space before repositioning
         * the text string.
         */
        const float* origin_model_space = bo_vertex_data + 4 * BO_DATA_INDEX_ORIGIN;
              float  origin_mvp_space   [4];
              float  origin_window_space[2];

        system_matrix4x4_multiply_by_vector4(vp,
                                             origin_model_space,
                                             origin_mvp_space);

        system_math_vector_mul3_float(origin_mvp_space,
                                      1.0f / origin_mvp_space[3],
                                      origin_mvp_space);

        /* Squeeze MVP to window coords and use this as the label's position.
         * If the camera is invisible, make sure it stays hidden in window space.
         */
        if (origin_mvp_space[3] >= 0.0f)
        {
            static const bool is_visible = true;

            origin_window_space[0] =         origin_mvp_space[0] * 0.5f + 0.5f;
            origin_window_space[1] = 1.0f - (origin_mvp_space[1] * 0.5f + 0.5f);

            ogl_text_set_text_string_property(camera_ptr->text_renderer,
                                              camera_ptr->camera_text_id,
                                              OGL_TEXT_STRING_PROPERTY_VISIBILITY,
                                             &is_visible);
        }
        else
        {
            static const bool is_visible = false;

            ogl_text_set_text_string_property(camera_ptr->text_renderer,
                                              camera_ptr->camera_text_id,
                                              OGL_TEXT_STRING_PROPERTY_VISIBILITY,
                                             &is_visible);
        }

        ogl_text_set_text_string_property(camera_ptr->text_renderer,
                                          camera_ptr->camera_text_id,
                                          OGL_TEXT_STRING_PROPERTY_POSITION_SS,
                                          origin_window_space);

        /* Update MDEBV draw call arguments */
        preview_ptr->mdebv_basevertex_array[n_camera] = (BO_DATA_INDEX_MAX + 1) * n_camera;
        preview_ptr->mdebv_count_array     [n_camera] = sizeof(index_data_array) / sizeof(index_data_array[0]);
        preview_ptr->mdebv_indices_array   [n_camera] = (GLvoid*) preview_ptr->data_bo_start_offset;
    } /* for (all assigned cameras) */

    /* Set up the data BO */
    GLuint                                                    data_dst_offset     = 0;
    GLuint                                                    data_src_offset     = 0;
    GLuint                                                    data_size           = 0;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints_ptr = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = NULL;

    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints_ptr);

    if (preview_ptr->data_bo_id != 0 && realloc_needed||
        preview_ptr->data_bo_id == 0)
    {
        if (preview_ptr->data_bo_id != 0)
        {
            ogl_buffers_free_buffer_memory(preview_ptr->buffers,
                                           preview_ptr->data_bo_id,
                                           preview_ptr->data_bo_start_offset);

            preview_ptr->data_bo_id           = 0;
            preview_ptr->data_bo_start_offset = -1;
        } /* if (preview_ptr->data_bo_id != 0) */

        ogl_buffers_allocate_buffer_memory(preview_ptr->buffers,
                                           preview_ptr->data_bo_buffer_size,
                                           1, /* alignment_requirement */
                                           OGL_BUFFERS_MAPPABILITY_NONE,
                                           OGL_BUFFERS_USAGE_MISCELLANEOUS,
                                           OGL_BUFFERS_FLAGS_NONE,
                                          &preview_ptr->data_bo_id,
                                          &preview_ptr->data_bo_start_offset);

        data_dst_offset = preview_ptr->data_bo_start_offset;
        data_src_offset = 0;
        data_size       = preview_ptr->data_bo_buffer_size;
    } /* if (data BO needs to be realloced) */
    else
    {
        data_dst_offset = preview_ptr->data_bo_start_offset + BO_DATA_VERTEX_DATA_OFFSET;
        data_src_offset = BO_DATA_VERTEX_DATA_OFFSET;
        data_size       = preview_ptr->data_bo_buffer_size - BO_DATA_VERTEX_DATA_OFFSET;
    }

    dsa_entrypoints_ptr->pGLNamedBufferSubDataEXT(preview_ptr->data_bo_id,
                                                  data_dst_offset,
                                                  data_size,
                                                  preview_ptr->data_bo_buffer + data_src_offset);

    preview_ptr->data_bo_buffer_last_update_time = frame_time;

    /* Set up the VAO */
    entrypoints_ptr->pGLBindVertexArray(preview_ptr->vao_id);
    entrypoints_ptr->pGLBindBuffer     (GL_ARRAY_BUFFER,
                                        preview_ptr->data_bo_id);
    entrypoints_ptr->pGLBindBuffer     (GL_ELEMENT_ARRAY_BUFFER,
                                        preview_ptr->data_bo_id);

    entrypoints_ptr->pGLEnableVertexAttribArray(0);       /* index */
    entrypoints_ptr->pGLVertexAttribPointer    (0,        /* index */
                                                4,        /* size */
                                                GL_FLOAT,
                                                GL_FALSE, /* normalized */
                                                0,        /* stride */
                                                (const GLvoid*) (preview_ptr->data_bo_start_offset + BO_DATA_VERTEX_DATA_OFFSET) );
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _ogl_scene_renderer_frustum_preview_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "ogl_scene_renderer_frustum_preview is only supported under GL contexts")
    }
#endif


/** Please see header for spec */
PUBLIC void ogl_scene_renderer_frustum_preview_assign_cameras(__in __notnull ogl_scene_renderer_frustum_preview preview,
                                                              __in           unsigned int                       n_cameras,
                                                              __in __notnull scene_camera*                      cameras)
{
    _ogl_scene_renderer_frustum_preview* preview_ptr = (_ogl_scene_renderer_frustum_preview*) preview;

    /* Release all cameras that may have been assigned in the past */
    {
        _ogl_scene_renderer_frustum_preview_camera* current_camera_ptr = NULL;

        while (system_resizable_vector_pop(preview_ptr->assigned_cameras,
                                          &current_camera_ptr) )
        {
            delete current_camera_ptr;

            current_camera_ptr = NULL;
        }
    }

    /* Add the new cameras to the vector */
    {
        for (uint32_t n_camera = 0;
                      n_camera < n_cameras;
                    ++n_camera)
        {
            system_hashed_ansi_string camera_name   = NULL;
            ogl_text_string_id        label_text_id = ogl_text_add_string(preview_ptr->text_renderer);

            scene_camera_get_property(cameras[n_camera],
                                      SCENE_CAMERA_PROPERTY_NAME,
                                      0, /* time - irrelevant */
                                     &camera_name);

            ogl_text_set(preview_ptr->text_renderer,
                         label_text_id,
                         system_hashed_ansi_string_get_buffer(camera_name) );

            _ogl_scene_renderer_frustum_preview_camera* current_camera_ptr = new _ogl_scene_renderer_frustum_preview_camera(cameras[n_camera],
                                                                                                                            preview_ptr->text_renderer,
                                                                                                                            label_text_id);

            system_resizable_vector_push(preview_ptr->assigned_cameras,
                                         current_camera_ptr);
        }
    }

    /* Re-allocate the data buffer we will use to hold data before updating the BO */
    const unsigned int n_bytes_needed = BO_DATA_VERTEX_DATA_OFFSET            +
                                        N_BYTES_PER_BO_DATA_ENTRY * n_cameras;

    if (preview_ptr->data_bo_buffer_size < n_bytes_needed)
    {
        if (preview_ptr->data_bo_buffer != NULL)
        {
            delete [] preview_ptr->data_bo_buffer;

            preview_ptr->data_bo_buffer      = NULL;
            preview_ptr->data_bo_buffer_size = 0;
        } /* if (preview_ptr->data_bo_buffer != NULL) */

        preview_ptr->data_bo_buffer = new (std::nothrow) unsigned char[n_bytes_needed];

        ASSERT_ALWAYS_SYNC(preview_ptr->data_bo_buffer != NULL,
                           "Out of memory");

        if (preview_ptr->data_bo_buffer != NULL)
        {
            preview_ptr->data_bo_buffer_size = n_bytes_needed;
        }
    } /* if (preview_ptr->data_bo_buffer != NULL) */

    /* The cameras assigned have likely changed. Reset the "last update time" state
     * so that the data BO contents are refreshed during next redraw. */
    preview_ptr->data_bo_buffer_last_update_time = -1;
}

/** Please see header for spec */
PUBLIC ogl_scene_renderer_frustum_preview ogl_scene_renderer_frustum_preview_create(__in __notnull ogl_context context,
                                                                                    __in __notnull scene       scene)
{
    _ogl_scene_renderer_frustum_preview* new_instance = new (std::nothrow) _ogl_scene_renderer_frustum_preview;

    ASSERT_ALWAYS_SYNC(new_instance != NULL,
                       "Out of memory");

    if (new_instance != NULL)
    {
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_BUFFERS,
                                &new_instance->buffers);

        new_instance->context = context;
        new_instance->scene   = scene;

        ogl_context_request_callback_from_context_thread(context,
                                                         _ogl_scene_renderer_frustum_preview_init_rendering_thread_callback,
                                                         new_instance);
    } /* if (new_instance != NULL) */

    return (ogl_scene_renderer_frustum_preview) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_scene_renderer_frustum_preview_release(__in __notnull __post_invalid ogl_scene_renderer_frustum_preview preview)
{
    _ogl_scene_renderer_frustum_preview* preview_ptr = (_ogl_scene_renderer_frustum_preview*) preview;

    ogl_context_request_callback_from_context_thread(preview_ptr->context,
                                                     _ogl_scene_renderer_frustum_preview_deinit_rendering_thread_callback,
                                                     preview);

    delete preview_ptr;
    preview_ptr = NULL;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_frustum_preview_render(__in __notnull ogl_scene_renderer_frustum_preview preview,
                                                                             __in           system_timeline_time               time,
                                                                             __in __notnull system_matrix4x4                   vp)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints_ptr = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = NULL;
    _ogl_scene_renderer_frustum_preview*                      preview_ptr         = (_ogl_scene_renderer_frustum_preview*) preview;

    /* Retrieve entry-points */
    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints_ptr);
    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Bind the renderer's VAO */
    ASSERT_DEBUG_SYNC(preview_ptr->vao_id != 0,
                      "VAO ID is 0");

    entrypoints_ptr->pGLBindVertexArray(preview_ptr->vao_id);

    /* Update the data BO contents if needed */
    if (preview_ptr->data_bo_buffer_last_update_time != time)
    {
        _ogl_scene_renderer_frustum_preview_update_data_bo_buffer(preview_ptr,
                                                                  time,
                                                                  vp);
    } /* if (preview_ptr->data_bo_buffer_last_update_time != time) */

    /* Update line width */
    entrypoints_ptr->pGLLineWidth(2.0f);

    /* Update VP before we kick off */
    const GLuint po_id = ogl_program_get_id(preview_ptr->po);

    entrypoints_ptr->pGLUseProgram(po_id);

    ogl_program_ub_set_nonarrayed_uniform_value(preview_ptr->po_ub,
                                                preview_ptr->po_vp_ub_offset,
                                                system_matrix4x4_get_column_major_data(vp),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);
    ogl_program_ub_sync                        (preview_ptr->po_ub);

    /* Draw! */
    entrypoints_ptr->pGLEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    {
        entrypoints_ptr->pGLBindBufferRange            (GL_UNIFORM_BUFFER,
                                                        0, /* index */
                                                        preview_ptr->po_ub_bo_id,
                                                        preview_ptr->po_ub_bo_start_offset,
                                                        preview_ptr->po_ub_bo_size);
        entrypoints_ptr->pGLMultiDrawElementsBaseVertex(GL_LINE_STRIP,
                                                        preview_ptr->mdebv_count_array,
                                                        GL_UNSIGNED_BYTE,
                                                        preview_ptr->mdebv_indices_array,
                                                        preview_ptr->mdebv_array_size,
                                                        preview_ptr->mdebv_basevertex_array);
    }
    entrypoints_ptr->pGLDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    /* Draw the labels */
    ogl_text_draw(preview_ptr->context,
                  preview_ptr->text_renderer);
}



