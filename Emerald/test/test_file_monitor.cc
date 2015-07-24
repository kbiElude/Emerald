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


PRIVATE void _on_file_contents_changed(system_hashed_ansi_string file_name,
                                       void*                     user_arg)
{
    system_event* event_ptr = (system_event*) user_arg;

    system_event_set(*event_ptr);
}

TEST(FilesTest, FileMonitor)
{
    system_event              callback_received_event = system_event_create(true); /* manual_reset */
    FILE*                     file_handle             = NULL;
    bool                      has_test_passed         = false;
    const char                test_contents[]         = "This is an example sentence.";
    const char                test_contents2[]        = "That";
    const char*               test_file_name          = "Oink";
    system_hashed_ansi_string test_file_name_has      = system_hashed_ansi_string_create(test_file_name);
    system_file_serializer    test_file_serializer    = NULL;

    /* Create the test file */
    test_file_serializer = system_file_serializer_create_for_writing(test_file_name_has);

    ASSERT_TRUE(test_file_serializer != NULL);

    system_file_serializer_write(test_file_serializer,
                                 sizeof(test_contents),
                                 test_contents);

    system_file_serializer_release(test_file_serializer);
    test_file_serializer = NULL;

    /* Initialize the file monitor */
    system_file_monitor_monitor_file_changes(test_file_name_has,
                                             true, /* should_enable */
                                            &_on_file_contents_changed,
                                            &has_test_passed);

    /* Write some more stuff to the file */
    file_handle = fopen(test_file_name,
                        "w+");

    ASSERT_TRUE(file_handle != NULL);

    fwrite(test_contents2,
           sizeof(test_contents2),
           1, /* _Count */
           file_handle);
    fclose(file_handle);

    system_event_wait_single(callback_received_event);
    system_event_release    (callback_received_event);
}

