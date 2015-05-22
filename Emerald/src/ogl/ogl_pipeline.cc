/**
 *
 * Emerald (kbi/elude @2013-2015)
 *
 */
#include "shared.h"
#include "gfx/gfx_bfg_font_table.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_query.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_ui.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_resources.h"
#include "system/system_types.h"
#include "system/system_window.h"

#define DEFAULT_STAGES_CAPACITY      (4)
#define DEFAULT_STAGE_STEPS_CAPACITY (4)
#define MAX_STAGE_STEP_NAME_LENGTH   (32)
#define ONE_SECOND_IN_NANOSECONDS    (1000000000i64)
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
    ogl_context               context;
    system_hashed_ansi_string name;
    bool                      should_overlay_performance_info;

    ogl_text                  text_renderer;
    uint32_t                  text_y_delta;

    ogl_ui                    ui;

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
    ogl_context                context;
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
PRIVATE void _ogl_pipeline_deinit_pipeline                             (__in __notnull   _ogl_pipeline*             pipeline_ptr);
PRIVATE void _ogl_pipeline_deinit_pipeline_stage                       (__in __notnull   _ogl_pipeline_stage*       stage_ptr);
PRIVATE void _ogl_pipeline_deinit_pipeline_stage_step                  (__in __notnull   _ogl_pipeline_stage_step*  step_ptr);
PRIVATE void _ogl_pipeline_deinit_pipeline_stage_step_renderer_callback(__in __notnull   ogl_context                context,
                                                                                         void*                      user_arg);
PRIVATE void _ogl_pipeline_init_pipeline                               (__in __notnull   _ogl_pipeline*             pipeline_ptr,
                                                                        __in __notnull   ogl_context                context,
                                                                        __in             bool                       should_overlay_performance_info);
PRIVATE void _ogl_pipeline_init_pipeline_stage                         (__in __notnull   _ogl_pipeline_stage*       stage_ptr);
PRIVATE void _ogl_pipeline_init_pipeline_stage_step                    (__in __notnull   _ogl_pipeline_stage_step*  step_ptr,
                                                                        __in __notnull   system_hashed_ansi_string  name,
                                                                        __in __notnull   PFNOGLPIPELINECALLBACKPROC pfn_step_callback,
                                                                        __in __maybenull void*                      step_callback_user_arg,
                                                                        __in __notnull   ogl_context                context);
PRIVATE void _ogl_pipeline_init_pipeline_stage_step_renderer_callback  (__in __notnull   ogl_context                context,
                                                                                         void*                      user_arg);
PRIVATE void _ogl_pipeline_release                                     (                 void*                      pipeline);


#ifdef _DEBUG
    PRIVATE void _ogl_pipeline_verify_context_type(__in __notnull ogl_context);
#else
    #define _ogl_pipeline_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _ogl_pipeline_deinit_pipeline(__in __notnull _ogl_pipeline* pipeline_ptr)
{
    if (pipeline_ptr->context != NULL)
    {
        ogl_context_release(pipeline_ptr->context);
    } /* if (pipeline_ptr->context != NULL) */

    while (system_resizable_vector_get_amount_of_elements(pipeline_ptr->stages) )
    {
        _ogl_pipeline_stage* stage_ptr = NULL;

        if (system_resizable_vector_pop(pipeline_ptr->stages,
                                       &stage_ptr) )
        {
            _ogl_pipeline_deinit_pipeline_stage(stage_ptr);

            delete stage_ptr;
            stage_ptr = NULL;
        } /* if (system_resizable_vector_pop(pipeline_ptr->stages, &stage_ptr) ) */
    } /* while (system_resizable_vector_get_amount_of_elements(pipeline_ptr->stages) ) */

    if (pipeline_ptr->ui != NULL)
    {
        ogl_ui_release(pipeline_ptr->ui);
    }

    if (pipeline_ptr->text_renderer != NULL)
    {
        ogl_text_release(pipeline_ptr->text_renderer);
    }
}

/** TODO */
PRIVATE void _ogl_pipeline_deinit_pipeline_stage(__in __notnull _ogl_pipeline_stage* stage_ptr)
{
    _ogl_pipeline_stage_step* step_ptr = NULL;

    while (system_resizable_vector_pop(stage_ptr->steps,
                                      &step_ptr) )
    {
        _ogl_pipeline_deinit_pipeline_stage_step(step_ptr);

        delete step_ptr;
        step_ptr = NULL;
    } /* while (system_resizable_vector_pop(stage_ptr, &step_ptr) ) */
}

/** TODO */
PRIVATE void _ogl_pipeline_deinit_pipeline_stage_step(__in __notnull _ogl_pipeline_stage_step* step_ptr)
{
    ogl_context_request_callback_from_context_thread(step_ptr->context,
                                                     _ogl_pipeline_deinit_pipeline_stage_step_renderer_callback,
                                                     step_ptr);

    if (step_ptr->context != NULL)
    {
        ogl_context_release(step_ptr->context);
    } /* if (step_ptr->context != NULL) */
}

/** TODO */
PRIVATE void _ogl_pipeline_deinit_pipeline_stage_step_renderer_callback(__in __notnull ogl_context context,
                                                                                       void*       user_arg)
{
    _ogl_pipeline_stage_step* step_ptr = (_ogl_pipeline_stage_step*) user_arg;

    if (step_ptr->primitives_generated_qo != NULL)
    {
        ogl_query_release(step_ptr->primitives_generated_qo);

        step_ptr->primitives_generated_qo = NULL;
    }

    if (step_ptr->timestamp_qo != NULL)
    {
        ogl_query_release(step_ptr->timestamp_qo);

        step_ptr->timestamp_qo = NULL;
    }
}

/** TODO */
PRIVATE void _ogl_pipeline_init_pipeline(__in __notnull _ogl_pipeline*            pipeline_ptr,
                                         __in __notnull ogl_context               context,
                                         __in           bool                      should_overlay_performance_info,
                                         __in __notnull system_hashed_ansi_string name)
{
    /* Retrieve output dimensions before continuing */
    system_window         context_window        = NULL;
    gfx_bfg_font_table    font_table            = system_resources_get_meiryo_font_table();
    ogl_rendering_handler rendering_handler     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_WINDOW,
                            &context_window);

    system_window_get_property(context_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &rendering_handler);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TEXT_RENDERER,
                            &pipeline_ptr->text_renderer);
    ogl_text_retain         (pipeline_ptr->text_renderer);

    /* Initialize */
    pipeline_ptr->context                         = context;
    pipeline_ptr->name                            = name;
    pipeline_ptr->should_overlay_performance_info = should_overlay_performance_info;
    pipeline_ptr->stages                          = system_resizable_vector_create(DEFAULT_STAGES_CAPACITY);
    pipeline_ptr->ui                              = ogl_ui_create                 (pipeline_ptr->text_renderer,
                                                                                   system_hashed_ansi_string_create("Pipeline UI") );
    pipeline_ptr->text_y_delta                    = (uint32_t) (gfx_bfg_font_table_get_maximum_character_height(font_table) * TEXT_SCALE);

    const float text_scale = TEXT_SCALE;

    ogl_text_set_text_string_property(pipeline_ptr->text_renderer,
                                      TEXT_STRING_ID_DEFAULT,
                                      OGL_TEXT_STRING_PROPERTY_COLOR,
                                      TEXT_COLOR);
    ogl_text_set_text_string_property(pipeline_ptr->text_renderer,
                                      TEXT_STRING_ID_DEFAULT,
                                      OGL_TEXT_STRING_PROPERTY_SCALE,
                                     &text_scale);

    ogl_context_retain(context);
}

/** TODO */
PRIVATE void _ogl_pipeline_init_pipeline_stage(__in __notnull _ogl_pipeline_stage* stage_ptr)
{
    /* Counter is set to 1 for each stage, because the zeroth index is used for "total frame time" string */
    stage_ptr->max_text_width_px    = 0;
    stage_ptr->steps                = system_resizable_vector_create(DEFAULT_STAGE_STEPS_CAPACITY);
    stage_ptr->text_strings_counter = 1;
}

/** TODO */
PRIVATE void _ogl_pipeline_init_pipeline_stage_step(__in __notnull   _ogl_pipeline_stage_step*  step_ptr,
                                                    __in __notnull   system_hashed_ansi_string  name,
                                                    __in __notnull   PFNOGLPIPELINECALLBACKPROC pfn_step_callback,
                                                    __in __maybenull void*                      step_callback_user_arg,
                                                    __in __notnull   ogl_context                context)
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
    step_ptr->primitives_generated_qo       = NULL;
    step_ptr->timestamp_qo                  = NULL;
    step_ptr->step_callback_user_arg        = step_callback_user_arg;

    /* Zero out gathered execution times */
    memset(step_ptr->execution_times,
           0,
           sizeof(step_ptr->execution_times) );

    ogl_context_retain                              (context);
    ogl_context_request_callback_from_context_thread(context,
                                                     _ogl_pipeline_init_pipeline_stage_step_renderer_callback,
                                                     step_ptr);
}

/** TODO */
PRIVATE void _ogl_pipeline_init_pipeline_stage_step_renderer_callback(__in __notnull ogl_context context,
                                                                                     void*       user_arg)
{
    _ogl_pipeline_stage_step* step_ptr = (_ogl_pipeline_stage_step*) user_arg;

    step_ptr->primitives_generated_qo = ogl_query_create(context,
                                                         QO_RING_BUFFER_SIZE,
                                                         GL_PRIMITIVES_GENERATED);
    step_ptr->timestamp_qo            = ogl_query_create(context,
                                                         QO_RING_BUFFER_SIZE,
                                                         GL_TIME_ELAPSED);
}

/** TODO */
PRIVATE void _ogl_pipeline_release(void* pipeline)
{
    _ogl_pipeline_deinit_pipeline( (_ogl_pipeline*) pipeline);
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _ogl_pipeline_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "ogl_pipeline is only supported under GL contexts")
    }
#endif


/** Please see header for specification */
PUBLIC EMERALD_API uint32_t ogl_pipeline_add_stage_step(__in __notnull ogl_pipeline               instance,
                                                        __in           uint32_t                   n_stage,
                                                        __in __notnull system_hashed_ansi_string  step_name,
                                                        __in __notnull PFNOGLPIPELINECALLBACKPROC pfn_step_callback,
                                                        __in __notnull void*                      step_callback_user_arg)
{
    _ogl_pipeline_stage_step* new_step_ptr       = NULL;
    _ogl_pipeline*            pipeline_ptr       = (_ogl_pipeline*) instance;
    _ogl_pipeline_stage*      pipeline_stage_ptr = NULL;
    uint32_t                  result             = -1;

    /* Retrieve stage descriptor */
    if (system_resizable_vector_get_element_at(pipeline_ptr->stages,
                                               n_stage,
                                              &pipeline_stage_ptr) )
    {
        /* Instantiate new step descriptor */
        new_step_ptr = new (std::nothrow) _ogl_pipeline_stage_step;

        ASSERT_DEBUG_SYNC(new_step_ptr != NULL,
                          "Out of memory");

        if (new_step_ptr != NULL)
        {
            /* Initialize new step */
            _ogl_pipeline_init_pipeline_stage_step(new_step_ptr,
                                                   step_name,
                                                   pfn_step_callback,
                                                   step_callback_user_arg,
                                                   pipeline_ptr->context);

            /* Assign text string ID we will use to print the execution time */
            uint32_t n_pipeline_stages = system_resizable_vector_get_amount_of_elements(pipeline_stage_ptr->steps);

            ogl_text_string_id new_text_string_id  = ogl_text_add_string(pipeline_ptr->text_renderer);
            ogl_text_string_id new_time_string_id  = ogl_text_add_string(pipeline_ptr->text_renderer);
            const int          new_text_x          = 0;
            const int          new_time_x          = pipeline_stage_ptr->max_text_width_px;
            const int          new_text_y          = START_Y + n_pipeline_stages * pipeline_ptr->text_y_delta;
            const int          new_text_position[] = {new_text_x, new_text_y};
            const int          new_time_position[] = {new_time_x, new_text_y};

            ogl_text_set_text_string_property(pipeline_ptr->text_renderer,
                                              new_text_string_id,
                                              OGL_TEXT_STRING_PROPERTY_COLOR,
                                              TEXT_COLOR);
            ogl_text_set_text_string_property(pipeline_ptr->text_renderer,
                                              new_time_string_id,
                                              OGL_TEXT_STRING_PROPERTY_COLOR,
                                              TEXT_COLOR);
            ogl_text_set_text_string_property(pipeline_ptr->text_renderer,
                                              new_text_string_id,
                                              OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                              new_text_position);
            ogl_text_set_text_string_property(pipeline_ptr->text_renderer,
                                              new_time_string_id,
                                              OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                              new_time_position);
            ogl_text_set_text_string_property(pipeline_ptr->text_renderer,
                                              new_text_string_id,
                                              OGL_TEXT_STRING_PROPERTY_SCALE,
                                              &TEXT_SCALE);
            ogl_text_set_text_string_property(pipeline_ptr->text_renderer,
                                              new_time_string_id,
                                              OGL_TEXT_STRING_PROPERTY_SCALE,
                                              &TEXT_SCALE);

            result                          = new_text_string_id;
            new_step_ptr->text_string_index = new_text_string_id;
            new_step_ptr->text_time_index   = new_time_string_id;

            pipeline_stage_ptr->text_strings_counter++;

            /* Form the string */
            const uint32_t buffer_length         = MAX_STAGE_STEP_NAME_LENGTH + 1 /* : */ + 1 /* null terminator */;
            char           buffer[buffer_length] = {0};

            sprintf_s(buffer,
                      buffer_length,
                      "%s:",
                      system_hashed_ansi_string_get_buffer(step_name) );

            /* Update the text string */
            ogl_text_set(pipeline_ptr->text_renderer,
                         new_text_string_id,
                         buffer);

            /* Insert new step into the stage's steps vector */
            system_resizable_vector_push(pipeline_stage_ptr->steps,
                                         new_step_ptr);

            /* If newly added name is longer than any of the already present ones, we need to relocate all time strings */
            uint32_t text_width_px = 0;

            ogl_text_get_text_string_property(pipeline_ptr->text_renderer,
                                              OGL_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                              new_text_string_id,
                                             &text_width_px);

            if (text_width_px > pipeline_stage_ptr->max_text_width_px)
            {
                pipeline_stage_ptr->max_text_width_px = text_width_px;

                for (uint32_t n = 0;
                              n < n_pipeline_stages + 1;
                            ++n)
                {
                    _ogl_pipeline_stage_step* pipeline_stage_step_ptr = NULL;

                    system_resizable_vector_get_element_at(pipeline_stage_ptr->steps,
                                                           n,
                                                          &pipeline_stage_step_ptr);

                    if (pipeline_stage_step_ptr != NULL)
                    {
                        int text_y           = START_Y + n * pipeline_ptr->text_y_delta;
                        int time_position[2] = {text_width_px, text_y};

                        ogl_text_set_text_string_property(pipeline_ptr->text_renderer,
                                                          pipeline_stage_step_ptr->text_time_index,
                                                          OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                                          time_position);
                    } /* if (pipeline_stage_step_ptr != NULL) */
                }
            }
        }
    } /* if (system_resizable_vector_get_element_at(pipeline_ptr->stages, n_stage, &pipeline_stage_ptr) ) */
    else
    {
        ASSERT_DEBUG_SYNC(false, "Could not retrieve stage descriptor");
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API uint32_t ogl_pipeline_add_stage(__in __notnull ogl_pipeline instance)
{
    _ogl_pipeline*       pipeline_ptr       = (_ogl_pipeline*) instance;
    _ogl_pipeline_stage* pipeline_stage_ptr = NULL;
    uint32_t             result_stage_id    = -1;

    /* Instantiate new stage descriptor */
    pipeline_stage_ptr = new (std::nothrow) _ogl_pipeline_stage;

    ASSERT_DEBUG_SYNC(pipeline_stage_ptr != NULL,
                      "Out of memory");

    if (pipeline_stage_ptr != NULL)
    {
        /* Initialize */
        _ogl_pipeline_init_pipeline_stage(pipeline_stage_ptr);

        /* First stage ever? Add a zeroth text string we'll use to draw sum-up */
        if (ogl_text_get_added_strings_counter(pipeline_ptr->text_renderer) == 0)
        {
            ogl_text_add_string(pipeline_ptr->text_renderer);
        }

        /* Retrieve new stage id */
        result_stage_id = system_resizable_vector_get_amount_of_elements(pipeline_ptr->stages);

        /* Insert new descriptor */
        system_resizable_vector_insert_element_at(pipeline_ptr->stages,
                                                  result_stage_id,
                                                  pipeline_stage_ptr);
    } /* if (pipeline_stage_ptr != NULL) */

    return result_stage_id;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API ogl_pipeline ogl_pipeline_create(__in __notnull ogl_context               context,
                                                                           __in           bool                      should_overlay_performance_info,
                                                                           __in __notnull system_hashed_ansi_string name)
{
    _ogl_pipeline* pipeline_ptr = new (std::nothrow) _ogl_pipeline;

    _ogl_pipeline_verify_context_type(context);

    ASSERT_DEBUG_SYNC(pipeline_ptr != NULL,
                      "Out of memory");

    if (pipeline_ptr != NULL)
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
    } /* if (pipeline_ptr != NULL) */

    return (ogl_pipeline) pipeline_ptr;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API bool ogl_pipeline_draw_stage(__in __notnull ogl_pipeline         instance,
                                                                       __in           uint32_t             n_stage,
                                                                       __in           system_timeline_time time)
{
    ogl_flyby                                                 flyby              = NULL;
    bool                                                      is_stage_dirty     = false;
    _ogl_pipeline*                                            pipeline_ptr       = (_ogl_pipeline*) instance;
    _ogl_pipeline_stage*                                      pipeline_stage_ptr = NULL;
    bool                                                      result             = false;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points   = NULL;
    const ogl_context_gl_entrypoints*                         entry_points       = NULL;

    ogl_context_get_property(pipeline_ptr->context,
                             OGL_CONTEXT_PROPERTY_FLYBY,
                            &flyby);
    ogl_context_get_property(pipeline_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(pipeline_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);

    /* Retrieve the stage descriptor first */
    if (system_resizable_vector_get_element_at(pipeline_ptr->stages,
                                               n_stage,
                                              &pipeline_stage_ptr))
    {
        uint32_t n_steps = system_resizable_vector_get_amount_of_elements(pipeline_stage_ptr->steps);

        /* If the flyby is activated for the context the pipeline owns, update it now */
        bool is_flyby_active = false;

        ogl_flyby_get_property(flyby,
                               OGL_FLYBY_PROPERTY_IS_ACTIVE,
                              &is_flyby_active);

        if (is_flyby_active)
        {
            ogl_flyby_update(flyby);
        } /* if (ogl_flyby_is_active(pipeline_ptr->context) ) */

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
                ogl_query_begin(step_ptr->primitives_generated_qo);
                ogl_query_begin(step_ptr->timestamp_qo);
                {
                    step_ptr->pfn_step_callback(step_ptr->context,
                                                time,
                                                step_ptr->step_callback_user_arg);
                }
                ogl_query_end(step_ptr->timestamp_qo);
                ogl_query_end(step_ptr->primitives_generated_qo);
            } /* if (system_resizable_vector_get_element_at(pipeline_stage_ptr->steps, n_step, &step_ptr) ) */
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve step descriptor");
            }
        } /* for (uint32_t n_step = 0; n_step < n_steps; ++n_step) */

        /* If any UI controls are around, draw them now */
        ogl_ui_draw(pipeline_ptr->ui);

        /* We only update timestamp step statistics AFTER a predefined number of samples have been accumulated.
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
                        } /* for (uint32_t n_sample = 0; n_sample < N_SAMPLES_PER_UPDATE; ++n_sample) */

                        step_ptr->average_sample_time = summed_sample_time / N_SAMPLES_PER_UPDATE;
                        step_ptr->n_samples_gathered  = 0;

                        /* We have the average value! Now stash it in our internal storage */
                        step_ptr->average_sample_time = (uint64_t) ((double) (step_ptr->average_sample_time) * 1000000.0) /
                                                                   ONE_SECOND_IN_NANOSECONDS;

                        /* Average step time was updated - mark the stage as dirty */
                        is_stage_dirty = true;
                    } /* if (step_ptr->n_samples_gathered >= N_SAMPLES_PER_UPDATE) */
                } /* if (primitives generated QO data available) */
            } /* if (system_resizable_vector_get_element_at(pipeline_stage_ptr->steps, n_step, &step_ptr) )*/
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve step descriptor");
            }
        } /* for (size_t n_step = 0; n_step < n_steps; ++n_step) */

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

                    sprintf_s(buffer,
                              sizeof(buffer),
                              "%d.%03d ms [%lld primitives generated]",
                              (unsigned int) step_ms / 1000,
                              (unsigned int) step_ms % 1000,
                              step_ptr->n_primitives_generated);

                    ogl_text_set(pipeline_ptr->text_renderer,
                                 step_ptr->text_time_index,
                                 buffer);
                }
            }
        } /* if (is_stage_dirty) */

        /* Render the results as a final pass */
        ogl_text_draw(pipeline_ptr->context,
                      pipeline_ptr->text_renderer);

        result = true;
    }

    /* Done */
    ASSERT_DEBUG_SYNC(result,
                      "Stage execution failed");

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_context ogl_pipeline_get_context(__in __notnull ogl_pipeline instance)
{
    return ((_ogl_pipeline*) instance)->context;
}

/** Please see header for specification */
PUBLIC ogl_text ogl_pipeline_get_text_renderer(__in __notnull ogl_pipeline instance)
{
    return ((_ogl_pipeline*) instance)->text_renderer;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_ui ogl_pipeline_get_ui(__in __notnull ogl_pipeline instance)
{
    return ((_ogl_pipeline*) instance)->ui;
}
