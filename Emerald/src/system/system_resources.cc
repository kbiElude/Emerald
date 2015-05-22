/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "../../resource.h"
#include "gfx/gfx_bfg_font_table.h"
#include "system/system_critical_section.h"
#include "system/system_log.h"
#include "system/system_resources.h"


/* Forward declarations */
PRIVATE void  _system_resources_get_embedded_meiryo_font_binaries(__out __notnull   void**  out_bmp_blob,
                                                                  __out __notnull   void**  out_dat_blob);
PRIVATE void* _system_resources_get_resource_blob                (__in  __maybenull HMODULE module_handle,
                                                                                    int     resource_id,
                                                                                    int     resource_type);

/* Private variables */
system_critical_section cs                = system_critical_section_create();
gfx_bfg_font_table      meiryo_font_table = NULL;

/** TODO */
PUBLIC void _system_resources_deinit()
{
    if (meiryo_font_table != NULL)
    {
        gfx_bfg_font_table_release(meiryo_font_table);
    }

    if (cs != NULL)
    {
        system_critical_section_release(cs);
    }
}

/** Please see header for specification */
PRIVATE void _system_resources_get_embedded_meiryo_font_binaries(__out __notnull void** out_bmp_blob,
                                                                 __out __notnull void** out_dat_blob)
{
    HMODULE dll_handle = ::GetModuleHandleA("Emerald.dll");

    ASSERT_DEBUG_SYNC(dll_handle != NULL,
                      "Module handle for Emerald is NULL!");

    if (dll_handle != NULL)
    {
        *out_bmp_blob = _system_resources_get_resource_blob(dll_handle,
                                                            IDR_FONT_BMP,
                                                            IDR_FONT_BMP);
        *out_dat_blob = _system_resources_get_resource_blob(dll_handle,
                                                            IDR_FONT_DAT,
                                                            IDR_FONT_DAT);

        ASSERT_DEBUG_SYNC(*out_bmp_blob != NULL,
                          "Could not retrieve bmp blob pointer.");
        ASSERT_DEBUG_SYNC(*out_dat_blob != NULL,
                          "Could not retrieve dat blob pointer.");
    }
}

/** TODO */
PRIVATE void* _system_resources_get_resource_blob(__in __maybenull HMODULE module_handle,
                                                                   int     resource_id,
                                                                   int     resource_type)
{
    void* result = NULL;

    system_critical_section_enter(cs);
    {
        HRSRC resource_handle = ::FindResourceA(module_handle,
                                                MAKEINTRESOURCE(resource_id),
                                                MAKEINTRESOURCE(resource_type) );        

        ASSERT_DEBUG_SYNC(resource_handle != NULL,
                          "Could not retrieve resource blob [%d]",
                          resource_id);

        if (resource_handle != NULL)
        {
            HGLOBAL loaded_resource_handle = ::LoadResource(module_handle,
                                                            resource_handle);

            ASSERT_DEBUG_SYNC(loaded_resource_handle != NULL,
                              "Could not load resource [%d]",
                              resource_id);

            if (loaded_resource_handle != NULL)
            {
                result = ::LockResource(loaded_resource_handle);

                ASSERT_DEBUG_SYNC(result != NULL,
                                  "Could not lock resource [%d]",
                                  resource_id);
            }
        }
    }
    system_critical_section_leave(cs);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void* system_resources_get_exe_resource(__in __notnull int resource_id,
                                                           __in           int resource_type)
{
    return _system_resources_get_resource_blob(0, /* module_handle */
                                               resource_id,
                                               resource_type);
}

/** Please see header for specification */
gfx_bfg_font_table system_resources_get_meiryo_font_table()
{
    system_critical_section_enter(cs);
    {
        if (meiryo_font_table == NULL)
        {
            LOG_INFO("Loading meiryo font table..");
            {
                void* meiryo_bmp_blob = NULL;
                void* meiryo_dat_blob = NULL;

                _system_resources_get_embedded_meiryo_font_binaries(&meiryo_bmp_blob,
                                                                    &meiryo_dat_blob);

                ASSERT_ALWAYS_SYNC(meiryo_bmp_blob != NULL &&
                                   meiryo_dat_blob != NULL,
                                   "Could not load embedded Meiryo font table.");

                if (meiryo_bmp_blob != NULL &&
                    meiryo_dat_blob != NULL)
                {
                    meiryo_font_table = gfx_bfg_font_table_create_from_ptr(system_hashed_ansi_string_create("System meiryo"),
                                                                           meiryo_bmp_blob,
                                                                           meiryo_dat_blob);

                    ASSERT_DEBUG_SYNC(meiryo_font_table != NULL,
                                      "Could not create Meiryo font table.");
                }
            }
        }
    }
    system_critical_section_leave(cs);

    return meiryo_font_table;
}
