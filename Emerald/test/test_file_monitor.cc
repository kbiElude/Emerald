/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "test_events.h"
#include "gtest/gtest.h"
#include "shared.h"
#include "system/system_event.h"
#include "system/system_file_monitor.h"
#include "system/system_file_serializer.h"


PRIVATE const char* test_file_name = "Oink";


PRIVATE void _create_test_file()
{
    const char                test_contents[]      = "This is an example sentence.";
    system_hashed_ansi_string test_file_name_has   = system_hashed_ansi_string_create(test_file_name);
    system_file_serializer    test_file_serializer = NULL;

    test_file_serializer = system_file_serializer_create_for_writing(test_file_name_has);

    ASSERT_TRUE(test_file_serializer != NULL);

    system_file_serializer_write(test_file_serializer,
                                 sizeof(test_contents),
                                 test_contents);

    system_file_serializer_release(test_file_serializer);
    test_file_serializer = NULL;
}

PRIVATE void _on_file_contents_changed(system_hashed_ansi_string file_name,
                                       void*                     user_arg)
{
    system_event* event_ptr = (system_event*) user_arg;

    system_event_set(*event_ptr);
}


TEST(FilesTest, NoCallbackFromFileMonitorForReadFilesTest)
{
    system_event              callback_received_event = system_event_create(true); /* manual_reset */
    FILE*                     file_handle             = NULL;
    bool                      has_timed_out           = false;
    char                      temp_buffer[4];
    system_hashed_ansi_string test_file_name_has      = system_hashed_ansi_string_create(test_file_name);
    system_file_serializer    test_file_serializer    = NULL;

    /* Create the test file */
    _create_test_file();

    /* Initialize the file monitor */
    system_file_monitor_monitor_file_changes(test_file_name_has,
                                             true, /* should_enable */
                                            &_on_file_contents_changed,
                                            &callback_received_event);

    /* Open the file and try to read some data from it */
    file_handle = fopen(test_file_name,
                        "r+");

    ASSERT_TRUE(file_handle != NULL);

    fread (temp_buffer,
           sizeof(temp_buffer),
           1,
           file_handle);
    fclose(file_handle);

    /* Wait for a few seconds .. */
    system_event_wait_multiple(&callback_received_event,
                               1, /* n_elements */
                               false, /* wait_on_all_objects */
                               system_time_get_time_for_s(2),
                              &has_timed_out);

    ASSERT_TRUE (has_timed_out);
    ASSERT_FALSE(system_event_wait_single_peek(callback_received_event) );

    /* Clean up */
    system_file_monitor_monitor_file_changes(test_file_name_has,
                                             false, /* should_enable */
                                             NULL,  /* pfn_file_changed_proc */
                                             NULL); /* user_arg */

    system_event_release(callback_received_event);
}

TEST(FilesTest, SingleFileMonitorTest)
{
    system_event              callback_received_event = system_event_create(true); /* manual_reset */
    FILE*                     file_handle             = NULL;
    const char                test_contents2[]        = "That";
    system_hashed_ansi_string test_file_name_has      = system_hashed_ansi_string_create(test_file_name);

    /* Create the test file */
    _create_test_file();

    /* Initialize the file monitor */
    system_file_monitor_monitor_file_changes(test_file_name_has,
                                             true, /* should_enable */
                                            &_on_file_contents_changed,
                                            &callback_received_event);

    /* Write some more stuff to the file */
    file_handle = fopen(test_file_name,
                        "w+");

    ASSERT_TRUE(file_handle != NULL);

    fwrite(test_contents2,
           sizeof(test_contents2),
           1, /* _Count */
           file_handle);
    fclose(file_handle);

    /* Wait till we hear back. */
    system_event_wait_single(callback_received_event);

    /* Unregister from notifications. This must be done BEFORE
     * we release the 'callback received' event. Otherwise, spurious
     * call-backs may crash the unit test project.. */
    system_file_monitor_monitor_file_changes(test_file_name_has,
                                             false, /* should_enable */
                                             NULL,  /* pfn_file_changed_proc */
                                             NULL); /* user_arg */

    system_event_release(callback_received_event);
}

