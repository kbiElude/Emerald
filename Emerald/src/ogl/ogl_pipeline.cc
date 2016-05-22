/**
 *
 * Emerald (kbi/elude @2013-2015)
 *
 */
#include "shared.h"
#include "gfx/gfx_bfg_font_table.h"
#include "ogl/ogl_context.h"
#include "demo/demo_flyby.h"
#include "demo/demo_window.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_query.h"
#include "ral/ral_context.h"
#include "ral/ral_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_resources.h"
#include "system/system_types.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "varia/varia_text_renderer.h"

#define DEFAULT_STAGES_CAPACITY      (4)
#define DEFAULT_STAGE_STEPS_CAPACITY (4)
#define MAX_STAGE_STEP_NAME_LENGTH   (32)
#define ONE_SECOND_IN_NANOSECONDS    (NSEC_PER_SEC)
#define N_SAMPLES_PER_UPDATE         (30)
#define QO_RING_BUFFER_SIZE          (5)
#define START_Y                      (50)

/** Internal variable definitions */
const float TEXT_COLOR[4] = {1, 1, 1, 1};
const float TEXT_SCALE    = 0.5f;

/** Internal type definitions */

/** TODO */
typedef struct
{
    ral_context               context;
    system_hashed_ansi_string name;
    bool                      should_overlay_performance_info;

    varia_text_renderer       text_renderer;
    uint32_t                  text_y_delta;

    ui                        ui_instance;

    system_resizable_vector   stages;

    REFCOUNT_INSERT_VARIABLES
} _ogl_pipeline;

/** TODO */
typedef struct
{
    uint32_t                max_text_width_px;
    system_resizable_vector steps;
    uint32_t                text_strings_counter;
} _ogl_pipeline_stage;

/** TODO */
typedef struct
{
    ral_context                context;
    system_hashed_ansi_string  name;
    PFNOGLPIPELINECALLBACKPROC pfn_step_callback;
    void*                      step_callback_user_arg;

    uint64_t                   average_sample_time;                   /* only updated after N_SAMPLES_PER_UPDATE samples are accumulated */
    GLuint64                   execution_times[N_SAMPLES_PER_UPDATE]; /* in nanoseconds */
    uint64_t                   n_primitives_generated;
    uint32_t                   n_samples_gathered;
    uint32_t                   text_string_index;
    uint32_t                   text_time_index;

    ogl_query primitives_generated_qo;
    ogl_query timestamp_qo;
} _ogl_pipeline_stage_step;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_pipeline,
                               ogl_pipeline,
                              _ogl_pipeline);

/* Forward declarations */
PRIVATE void _ogl_pipeline_deinit_pipeline                             (_ogl_pipeline*             pipeline_ptr);
PRIVATE void _ogl_pipeline_deinit_pipeline_stage                       (_ogl_pipeline_stage*       stage_ptr);
PRIVATE void _ogl_pipeline_deinit_pipeline_stage_step                  (_ogl_pipeline_stage_step*  step_ptr);
PRIVATE void _ogl_pipeline_deinit_pipeline_stage_step_renderer_callback(ogl_context                context,
                                                                        void*                      user_arg);
PRIVATE void _ogl_pipeline_init_pipeline                               (_ogl_pipeline*             pipeline_ptr,
                                                                        ral_context                context,
                                                                        bool                       should_overlay_performance_info);
PRIVATE void _ogl_pipeline_init_pipeline_stage                         (_ogl_pipeline_stage*       stage_ptr);
PRIVATE void _ogl_pipeline_init_pipeline_stage_step                    (_ogl_pipeline_stage_step*  step_ptr,
                                                                        system_hashed_ansi_string  name,
                                                                        PFNOGLPIPELINECALLBACKPROC pfn_step_callback,
                                                                        void*                      step_callback_user_arg,
                                                                        ral_context                context);
PRIVATE void _ogl_pipeline_init_pipeline_stage_step_renderer_callback  (ogl_context                context,
                                                                        void*                      user_arg);
PRIVATE void _ogl_pipeline_release                                     (void*                      pipeline);


/** TODO */
PRIVATE void _ogl_pipeline_deinit_pipeline(_ogl_pipeline* pipeline_ptr)
{
    if (pipeline_ptr->context != nullptr)
    {
        ral_context_release(pipeline_ptr->context);
    }

    while (true)
    {
        _ogl_pipeline_stage* stage_ptr = nullptr;

        if (system_resizable_vector_pop(pipeline_ptr->stages,
                                       &stage_ptr) )
        {
            _ogl_pipeline_deinit_pipeline_stage(stage_ptr);

            delete stage_ptr;
            stage_ptr = nullptr;
        }
        else
        {
            break;
        }
    }

    if (pipeline_ptr->ui_instance != nullptr)
    {
        ui_release(pipeline_ptr->ui_instance);
    }

    if (pipeline_ptr->text_renderer != nullptr)
    {
        varia_text_renderer_release(pipeline_ptr->text_renderer);
    }
}

/** TODO */
PRIVATE void _ogl_pipeline_deinit_pipeline_stage(_ogl_pipeline_stage* stage_ptr)
{
    _ogl_pipeline_stage_step* step_ptr = nullptr;

    while (system_resizable_vector_pop(stage_ptr->steps,
                                      &step_ptr) )
    {
        _ogl_pipeline_deinit_pipeline_stage_step(step_ptr);

        delete step_ptr;
        step_ptr = nullptr;
    }
}

/** TODO */
PRIVATE void _ogl_pipeline_deinit_pipeline_stage_step(_ogl_pipeline_stage_step* step_ptr)
{
    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(step_ptr->context),
                                                     _ogl_pipeline_deinit_pipeline_stage_step_renderer_callback,
                                                     step_ptr);

    if (step_ptr->context != nullptr)
    {
        ral_context_release(step_ptr->context);
    }
}

/** TODO */
PRIVATE void _ogl_pipeline_deinit_pipeline_stage_step_renderer_callback(ogl_context context,
                                                                        void*       user_arg)
{
    _ogl_pipeline_stage_step* step_ptr = (_ogl_pipeline_stage_step*) user_arg;

    if (step_ptr->primitives_generated_qo != nullptr)
    {
        ogl_query_release(step_ptr->primitives_generated_qo);

        step_ptr->primitives_generated_qo = nullptr;
    }

    if (step_ptr->timestamp_qo != nullptr)
    {
        ogl_query_release(step_ptr->timestamp_qo);

        step_ptr->timestamp_qo = nullptr;
    }
}

/** TODO */
PRIVATE void _ogl_pipeline_init_pipeline(_ogl_pipeline*            pipeline_ptr,
                                         ral_context               context,
                                         bool                      should_overlay_performance_info,
                                         system_hashed_ansi_string name)
{
    /* Retrieve output dimensions before continuing */
    system_window         context_window    = nullptr;
    gfx_bfg_font_table    font_table        = system_resources_get_meiryo_font_table();
    ral_rendering_handler rendering_handler = nullptr;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                            &context_window);

    system_window_get_property(context_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &rendering_handler);

    ogl_context_get_property  (ral_context_get_gl_context(context),
                               OGL_CONTEXT_PROPERTY_TEXT_RENDERER,
                              &pipeline_ptr->text_renderer);
    varia_text_renderer_retain(pipeline_ptr->text_renderer);

    /* Initialize */
    pipeline_ptr->context                         = context;
    pipeline_ptr->name                            = name;
    pipeline_ptr->should_overlay_performance_info = should_overlay_performance_info;
    pipeline_ptr->stages                          = system_resizable_vector_create(DEFAULT_STAGES_CAPACITY);
    pipeline_ptr->ui_instance                     = ui_create                     (pipeline_ptr->text_renderer,
                                                                                   system_hashed_ansi_string_create("Pipeline UI") );
    pipeline_ptr->text_y_delta                    = (uint32_t) (gfx_bfg_font_table_get_maximum_character_height(font_table) * TEXT_SCALE);

    const float text_scale = TEXT_SCALE;

    varia_text_renderer_set_text_string_property(pipeline_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_ID_DEFAULT,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                 TEXT_COLOR);
    varia_text_renderer_set_text_string_property(pipeline_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_ID_DEFAULT,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCALE,
                                                &text_scale);

    ral_context_retain(context);
}

/** TODO */
PRIVATE void _ogl_pipeline_init_pipeline_stage(_ogl_pipeline_stage* stage_ptr)
{
    /* Counter is set to 1 for each stage, because the zeroth index is used for "total frame time" string */
    stage_ptr->max_text_width_px    = 0;
    stage_ptr->steps                = system_resizable_vector_create(DEFAULT_STAGE_STEPS_CAPACITY);
    stage_ptr->text_strings_counter = 1;
}

/** TODO */
PRIVATE void _ogl_pipeline_init_pipeline_stage_step(_ogl_pipeline_stage_step*  step_ptr,
                                                    system_hashed_ansi_string  name,
                                                    PFNOGLPIPELINECALLBACKPROC pfn_step_callback,
                                                    void*                      step_callback_user_arg,
                                                    ral_context                context)
{
    /* Truncate input name if too long */
    uint32_t name_length = system_hashed_ansi_string_get_length(name);

    if (name_length > MAX_STAGE_STEP_NAME_LENGTH)
    {
        name_length = MAX_STAGE_STEP_NAME_LENGTH;
    }

    step_ptr->average_sample_time           = 0;
    step_ptr->context                       = context;
    step_ptr->name                          = system_hashed_ansi_string_create_substring(system_hashed_ansi_string_get_buffer(name),
                                                                                         0,
                                                                                         name_length);
    step_ptr->n_primitives_generated        = 0;
    step_ptr->n_samples_gathered            = 0;
    step_ptr->pfn_step_callback             = pfn_step_callback;
    step_ptr->primitives_generated_qo       = nullptr;
    step_ptr->timestamp_qo                  = nullptr;
    step_ptr->step_callback_user_arg        = step_callback_user_arg;

    /* Zero out gathered execution times */
    memset(step_ptr->execution_times,
           0,
           sizeof(step_ptr->execution_times) );

    ral_context_retain                              (context);
    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(context),
                                                     _ogl_pipeline_init_pipeline_stage_step_renderer_callback,
                                                     step_ptr);
}

/** TODO */
PRIVATE void _ogl_pipeline_init_pipeline_stage_step_renderer_callback(ogl_context context,
                                                                      void*       user_arg)
{
    _ogl_pipeline_stage_step* step_ptr = (_ogl_pipeline_stage_step*) user_arg;

    step_ptr->primitives_generated_qo = ogl_query_create(step_ptr->context,
                                                         QO_RING_BUFFER_SIZE,
                                                         GL_PRIMITIVES_GENERATED);
    step_ptr->timestamp_qo            = ogl_query_create(step_ptr->context,
                                                         QO_RING_BUFFER_SIZE,
                                                         GL_TIME_ELAPSED);
}

/** TODO */
PRIVATE void _ogl_pipeline_release(void* pipeline)
{
    _ogl_pipeline_deinit_pipeline( (_ogl_pipeline*) pipeline);
}



/** Please see header for specification */
PUBLIC EMERALD_API uint32_t ogl_pipeline_add_stage(ogl_pipeline instance)
{
    _ogl_pipeline*       pipeline_ptr       = (_ogl_pipeline*) instance;
    _ogl_pipeline_stage* pipeline_stage_ptr = nullptr;
    uint32_t             result_stage_id    = -1;

    /* Instantiate new stage descriptor */
    pipeline_stage_ptr = new (std::nothrow) _ogl_pipeline_stage;

    ASSERT_DEBUG_SYNC(pipeline_stage_ptr != nullptr,
                      "Out of memory");

    if (pipeline_stage_ptr != nullptr)
    {
        /* Initialize */
        _ogl_pipeline_init_pipeline_stage(pipeline_stage_ptr);

        /* First stage ever? Add a zeroth text string we'll use to draw sum-up */
        if (varia_text_renderer_get_added_strings_counter(pipeline_ptr->text_renderer) == 0)
        {
            varia_text_renderer_add_string(pipeline_ptr->text_renderer);
        }

        /* Retrieve new stage id */
        system_resizable_vector_get_property(pipeline_ptr->stages,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &result_stage_id);

        /* Insert new descriptor */
        system_resizable_vector_insert_element_at(pipeline_ptr->stages,
                                                  result_stage_id,
                                                  pipeline_stage_ptr);
    }

    return result_stage_id;
}

/** Please see header for specification */
PUBLIC EMERALD_API uint32_t ogl_pipeline_add_stage_step(ogl_pipeline                               instance,
                                                        uint32_t                                   n_stage,
                                                        const ogl_pipeline_stage_step_declaration* step_ptr)
{
    _ogl_pipeline_stage_step* new_step_ptr       = nullptr;
    _ogl_pipeline*            pipeline_ptr       = (_ogl_pipeline*) instance;
    _ogl_pipeline_stage*      pipeline_stage_ptr = nullptr;
    uint32_t                  result             = -1;

    /* Retrieve stage descriptor */
    if (system_resizable_vector_get_element_at(pipeline_ptr->stages,
                                               n_stage,
                                              &pipeline_stage_ptr) )
    {
        /* Instantiate new step descriptor */
        new_step_ptr = new (std::nothrow) _ogl_pipeline_stage_step;

        ASSERT_DEBUG_SYNC(new_step_ptr != nullptr,
                          "Out of memory");

        if (new_step_ptr != nullptr)
        {
            /* Initialize new step */
            _ogl_pipeline_init_pipeline_stage_step(new_step_ptr,
                                                   step_ptr->name,
                                                   step_ptr->pfn_callback_proc,
                                                   step_ptr->user_arg,
                                                   pipeline_ptr->context);

            /* Assign text string ID we will use to print the execution time */
            uint32_t n_pipeline_stages = 0;

            system_resizable_vector_get_property(pipeline_stage_ptr->steps,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_pipeline_stages);

            varia_text_renderer_text_string_id new_text_string_id  = varia_text_renderer_add_string(pipeline_ptr->text_renderer);
            varia_text_renderer_text_string_id new_time_string_id  = varia_text_renderer_add_string(pipeline_ptr->text_renderer);
            const int                          new_text_x          = 0;
            const int                          new_time_x          = pipeline_stage_ptr->max_text_width_px;
            const int                          new_text_y          = START_Y + n_pipeline_stages * pipeline_ptr->text_y_delta;
            const int                          new_text_position[] = {new_text_x, new_text_y};
            const int                          new_time_position[] = {new_time_x, new_text_y};

            varia_text_renderer_set_text_string_property(pipeline_ptr->text_renderer,
                                                         new_text_string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                         TEXT_COLOR);
            varia_text_renderer_set_text_string_property(pipeline_ptr->text_renderer,
                                                         new_time_string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                         TEXT_COLOR);
            varia_text_renderer_set_text_string_property(pipeline_ptr->text_renderer,
                                                         new_text_string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,
                                                         new_text_position);
            varia_text_renderer_set_text_string_property(pipeline_ptr->text_renderer,
                                                         new_time_string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,
                                                         new_time_position);
            varia_text_renderer_set_text_string_property(pipeline_ptr->text_renderer,
                                                         new_text_string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCALE,
                                                        &TEXT_SCALE);
            varia_text_renderer_set_text_string_property(pipeline_ptr->text_renderer,
                                                         new_time_string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCALE,
                                                        &TEXT_SCALE);

            result                          = new_text_string_id;
            new_step_ptr->text_string_index = new_text_string_id;
            new_step_ptr->text_time_index   = new_time_string_id;

            pipeline_stage_ptr->text_strings_counter++;

            /* Form the string */
            const uint32_t buffer_length         = MAX_STAGE_STEP_NAME_LENGTH + 1 /* : */ + 1 /* null terminator */;
            char           buffer[buffer_length] = {0};

            snprintf(buffer,
                     buffer_length,
                     "%s:",
                     system_hashed_ansi_string_get_buffer(step_ptr->name) );

            /* Update the text string */
            varia_text_renderer_set(pipeline_ptr->text_renderer,
                                    new_text_string_id,
                                    buffer);

            /* Insert new step into the stage's steps vector */
            system_resizable_vector_push(pipeline_stage_ptr->steps,
                                         new_step_ptr);

            /* If newly added name is longer than any of the already present ones, we need to relocate all time strings */
            uint32_t text_width_px = 0;

            varia_text_renderer_get_text_string_property(pipeline_ptr->text_renderer,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                                         new_text_string_id,
                                                        &text_width_px);

            if (text_width_px > pipeline_stage_ptr->max_text_width_px)
            {
                pipeline_stage_ptr->max_text_width_px = text_width_px;

                for (uint32_t n = 0;
                              n < n_pipeline_stages + 1;
                            ++n)
                {
                    _ogl_pipeline_stage_step* pipeline_stage_step_ptr = nullptr;

                    system_resizable_vector_get_element_at(pipeline_stage_ptr->steps,
                                                           n,
                                                          &pipeline_stage_step_ptr);

                    if (pipeline_stage_step_ptr != nullptr)
                    {
                        int text_y           = START_Y + n * pipeline_ptr->text_y_delta;
                        int time_position[2] = {text_width_px, text_y};

                        varia_text_renderer_set_text_string_property(pipeline_ptr->text_renderer,
                                                                     pipeline_stage_step_ptr->text_time_index,
                                                                     VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,
                                                                     time_position);
                    }
                }
            }
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false, "Could not retrieve stage descriptor");
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API uint32_t ogl_pipeline_add_stage_with_steps(ogl_pipeline                               pipeline,
                                                              unsigned int                               n_steps,
                                                              const ogl_pipeline_stage_step_declaration* steps)
{
    uint32_t result_stage_id = -1;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(pipeline != nullptr,
                      "Input rendering pipeline is NULL");
    ASSERT_DEBUG_SYNC(n_steps > 0,
                      "No steps to add");

    /* Create a new pipeline stage.. */
    result_stage_id = ogl_pipeline_add_stage(pipeline);

    /* Set up the pipeline stage steps using the pass info structures */
    for (unsigned int n_step = 0;
                      n_step < n_steps;
                    ++n_step)
    {
        /* Note: we don't need to store the step id so we ignore the return value */
        ogl_pipeline_add_stage_step(pipeline,
                                    result_stage_id,
                                    steps + n_step);
    }

    return result_stage_id;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API ogl_pipeline ogl_pipeline_create(ral_context               context,
                                                                           bool                      should_overlay_performance_info,
                                                                           system_hashed_ansi_string name)
{
    _ogl_pipeline* pipeline_ptr = new (std::nothrow) _ogl_pipeline;

    ASSERT_DEBUG_SYNC(pipeline_ptr != nullptr,
                      "Out of memory");

    if (pipeline_ptr != nullptr)
    {
        _ogl_pipeline_init_pipeline(pipeline_ptr,
                                    context,
                                    should_overlay_performance_info,
                                    name);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(pipeline_ptr,
                                                       _ogl_pipeline_release,
                                                       OBJECT_TYPE_OGL_PIPELINE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Pipelines\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (ogl_pipeline) pipeline_ptr;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API bool ogl_pipeline_draw_stage(ogl_pipeline instance,
                                                                       uint32_t     n_stage,
                                                                       uint32_t     frame_index,
                                                                       system_time  time,
                                                                       const int*   rendering_area_px_topdown)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points   = nullptr;
    const ogl_context_gl_entrypoints*                         entry_points       = nullptr;
    demo_flyby                                                flyby              = nullptr;
    bool                                                      is_stage_dirty     = false;
    uint32_t                                                  n_stages           = 0;
    _ogl_pipeline*                                            pipeline_ptr       = (_ogl_pipeline*) instance;
    _ogl_pipeline_stage*                                      pipeline_stage_ptr = nullptr;
    bool                                                      result             = false;
    demo_window                                               window             = nullptr;

    ral_context_get_property(pipeline_ptr->context,
                             RAL_CONTEXT_PROPERTY_WINDOW_DEMO,
                            &window);
    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_FLYBY,
                            &flyby);
    ogl_context_get_property(ral_context_get_gl_context(pipeline_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(ral_context_get_gl_context(pipeline_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);

    /* For all stages != n_stage, we need to disable text rendering. */
    if (pipeline_ptr->should_overlay_performance_info)
    {
        system_resizable_vector_get_property(pipeline_ptr->stages,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_stages);

        for (uint32_t n_current_stage = 0;
                      n_current_stage < n_stages;
                    ++n_current_stage)
        {
            _ogl_pipeline_stage* current_stage_ptr     = nullptr;
            uint32_t             n_current_stage_steps = 0;
            bool                 should_be_visible     = (n_current_stage == n_stage);

            system_resizable_vector_get_element_at(pipeline_ptr->stages,
                                                   n_current_stage,
                                                  &current_stage_ptr);

            ASSERT_DEBUG_SYNC(current_stage_ptr != nullptr,
                              "Could not retrieve pipeline stage descriptor at index [%d]",
                              n_current_stage);

            system_resizable_vector_get_property(current_stage_ptr->steps,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_current_stage_steps);

            for (uint32_t n_current_stage_step = 0;
                          n_current_stage_step < n_current_stage_steps;
                        ++n_current_stage_step)
            {
                _ogl_pipeline_stage_step* current_stage_step_ptr = nullptr;

                system_resizable_vector_get_element_at(current_stage_ptr->steps,
                                                       n_current_stage_step,
                                                      &current_stage_step_ptr);

                ASSERT_DEBUG_SYNC(current_stage_step_ptr != nullptr,
                                  "Could not retrieve pipeline stage step descriptor at index [%d] for stage [%d]",
                                  n_current_stage_step,
                                  n_current_stage);

                varia_text_renderer_set_text_string_property(pipeline_ptr->text_renderer,
                                                             current_stage_step_ptr->text_time_index,
                                                             VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                            &should_be_visible);
                varia_text_renderer_set_text_string_property(pipeline_ptr->text_renderer,
                                                             current_stage_step_ptr->text_string_index,
                                                             VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                            &should_be_visible);
            }
        }
    }

    /* Let's render the pipeline stage. Retrieve the stage descriptor first */
    if (system_resizable_vector_get_element_at(pipeline_ptr->stages,
                                               n_stage,
                                              &pipeline_stage_ptr))
    {
        uint32_t n_steps = 0;

        system_resizable_vector_get_property(pipeline_stage_ptr->steps,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_steps);

        /* If the flyby is activated for the context the pipeline owns, update it now */
        bool is_flyby_active = false;

        demo_flyby_get_property(flyby,
                                DEMO_FLYBY_PROPERTY_IS_ACTIVE,
                               &is_flyby_active);

        if (is_flyby_active)
        {
            demo_flyby_update(flyby);
        }

        /* Execute one step at a time. Launch a query for every step, so that we can gather execution times
         * after all steps finish running and show the times */
        _ogl_pipeline_stage_step* step_ptr = NULL;

        for (size_t n_step = 0;
                    n_step < n_steps;
                  ++n_step)
        {
            if (system_resizable_vector_get_element_at(pipeline_stage_ptr->steps,
                                                       n_step,
                                                      &step_ptr) )
            {
                /* Kick off the timer and call user's code */
                if (pipeline_ptr->should_overlay_performance_info)
                {
                    ogl_query_begin(step_ptr->primitives_generated_qo);
                    ogl_query_begin(step_ptr->timestamp_qo);
                }
                {
                    step_ptr->pfn_step_callback(step_ptr->context,
                                                frame_index,
                                                time,
                                                rendering_area_px_topdown,
                                                step_ptr->step_callback_user_arg);
                }
                if (pipeline_ptr->should_overlay_performance_info)
                {
                    ogl_query_end(step_ptr->timestamp_qo);
                    ogl_query_end(step_ptr->primitives_generated_qo);
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve step descriptor");
            }
        }

        /* If any UI controls are around, draw them now */
        ui_draw(pipeline_ptr->ui_instance);

        if (pipeline_ptr->should_overlay_performance_info)
        {
            /* We only update timestamp step statistics AFTER a predefined number of samples has been accumulated.
             * Check if any samples are available, and - if so - update the internal storage */
            for (size_t n_step = 0;
                        n_step < n_steps;
                      ++n_step)
            {
                if (system_resizable_vector_get_element_at(pipeline_stage_ptr->steps,
                                                           n_step,
                                                          &step_ptr) )
                {
                    /* Retrieve the query result */
                    GLuint64 execution_time = 0;

                    if (ogl_query_peek_result(step_ptr->primitives_generated_qo,
                                             &step_ptr->n_primitives_generated) )
                    {
                        /* It may be a bold assumption, but at this point the timestamp data retrieval
                         * process should not be that expensive to do..
                         */
                        while (!ogl_query_peek_result(step_ptr->timestamp_qo,
                                                     &execution_time) )
                        {
                            /* Aw yiss, spin that lock. */
                        }

                        /* Store the result */
                        step_ptr->execution_times[step_ptr->n_samples_gathered++] = execution_time;

                        /* If enough samples are available, update the average */
                        if (step_ptr->n_samples_gathered >= N_SAMPLES_PER_UPDATE)
                        {
                            GLuint64 summed_sample_time = 0;

                            for (uint32_t n_sample = 0;
                                          n_sample < N_SAMPLES_PER_UPDATE;
                                        ++n_sample)
                            {
                                summed_sample_time += step_ptr->execution_times[n_sample];
                            }

                            step_ptr->average_sample_time = summed_sample_time / N_SAMPLES_PER_UPDATE;
                            step_ptr->n_samples_gathered  = 0;

                            /* We have the average value! Now stash it in our internal storage */
                            step_ptr->average_sample_time = (uint64_t) ((double) (step_ptr->average_sample_time) * 1000000.0) /
                                                                       ONE_SECOND_IN_NANOSECONDS;

                            /* Average step time was updated - mark the stage as dirty */
                            is_stage_dirty = true;
                        }
                    }
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve step descriptor");
                }
            }

            /* Update stage strings if marked as dirty */
            if (is_stage_dirty)
            {
                uint32_t max_name_width = 0;

                for (size_t n_step = 0;
                            n_step < n_steps;
                          ++n_step)
                {
                    if (system_resizable_vector_get_element_at(pipeline_stage_ptr->steps,
                                                               n_step,
                                                              &step_ptr) )
                    {
                        uint64_t step_ms = 0;

                        /* Convert the average sample time to a value represented in miliseconds */
                        step_ms = step_ptr->average_sample_time;

                        /* Clamp the step time */
                        if (step_ms < 0)
                        {
                            step_ms = 0;
                        }

                        /* Form the string */
                        char buffer[128] = {0};

                        snprintf(buffer,
                                 sizeof(buffer),
                                 "%u.%03u ms [%llu primitives generated]",
                                 (unsigned int) step_ms / 1000,
                                 (unsigned int) step_ms % 1000,
                                 (long long unsigned int) step_ptr->n_primitives_generated);

                        varia_text_renderer_set(pipeline_ptr->text_renderer,
                                                step_ptr->text_time_index,
                                                buffer);
                    }
                }
            }

            /* Render the results as a final pass */
            varia_text_renderer_draw(pipeline_ptr->context,
                                     pipeline_ptr->text_renderer);
        }

        result = true;
    }

    /* Done */
    ASSERT_DEBUG_SYNC(result,
                      "Stage execution failed");

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_context ogl_pipeline_get_context(ogl_pipeline instance)
{
    return ((_ogl_pipeline*) instance)->context;
}

/** Please see header for specification */
PUBLIC varia_text_renderer ogl_pipeline_get_text_renderer(ogl_pipeline instance)
{
    return ((_ogl_pipeline*) instance)->text_renderer;
}

/** Please see header for specification */
PUBLIC EMERALD_API ui ogl_pipeline_get_ui(ogl_pipeline instance)
{
    return ((_ogl_pipeline*) instance)->ui_instance;
}
