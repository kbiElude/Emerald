/** Trivia:
 *
 *  1. You can move across the timeline by tapping/holding left/right cursor. The rendering process
 *     can be completely paused (& later resumed) by pressing space. By default, this behavior is only
 *     available in debug builds, but you can toggle that behavior by modifying demo_timeline code.
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_loader.h"
#include "system/system_hashed_ansi_string.h"
#include "main.h"

#include "include/stage_intro.h"
#include "include/stage_part1.h"
#include "include/stage_part2.h"
#include "include/stage_part3.h"
#include "include/stage_part4.h"
#include "include/stage_outro.h"


/** Entry point */
#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE instance_handle,
                       HINSTANCE,
                       LPTSTR,
                       int)
#else
    int main()
#endif
{
    demo_app    demo   = demo_app_create(system_hashed_ansi_string_create("Demo") );
    demo_loader loader = NULL;

    demo_app_get_property(demo,
                          DEMO_APP_PROPERTY_LOADER,
                         &loader);

    /* Configure the loader operations */
    demo_loader_op_load_audio_stream load_audio_stream_op;

    load_audio_stream_op.audio_file_name               = system_hashed_ansi_string_create("demo.mp3");
    load_audio_stream_op.result_audio_stream_index_ptr = NULL;

    demo_loader_enqueue_operation(loader,
                                  DEMO_LOADER_OP_LOAD_AUDIO_STREAM,
                                 &load_audio_stream_op);

    stage_intro_init(loader);
    stage_part1_init(loader);
    stage_part2_init(loader);
    stage_part3_init(loader);
    stage_part4_init(loader);
    stage_outro_init(loader);

    /* Run the demo */
    demo_app_run(demo);

    return 0;
}