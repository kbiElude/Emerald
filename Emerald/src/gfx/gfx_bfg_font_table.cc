/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "gfx/gfx_bfg_font_table.h"
#include "gfx/gfx_bmp.h"
#include "gfx/gfx_image.h"
#include "gfx/gfx_types.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"

#pragma pack(push)
    #pragma pack(1)

    /** Private declarations */
    typedef struct
    {
        /* Order corresponds to .dat file contents */
        uint32_t map_width;
        uint32_t map_height;
        uint32_t cell_width;
        uint32_t cell_height;
        uint8_t  start_character_index;
        uint8_t  character_widths[256];
    } _gfx_bfg_font_table_dat;
#pragma pack(pop)

typedef struct 
{
    gfx_image               bmp_data;
    _gfx_bfg_font_table_dat dat_data;

    REFCOUNT_INSERT_VARIABLES
} _gfx_bfg_font_table;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(gfx_bfg_font_table,
                               gfx_bfg_font_table,
                              _gfx_bfg_font_table);

/* Forward declarations */
PRIVATE gfx_bfg_font_table _gfx_bfg_font_table_create_shared(system_hashed_ansi_string      font_name_hashed_ansi_string,
                                                             gfx_image                      bmp_file_data,
                                                             const _gfx_bfg_font_table_dat& dat_file_data);
PRIVATE bool              _gfx_bfg_font_table_load_dat_blob (void*                          blob,
                                                             _gfx_bfg_font_table_dat*       out_result);
PRIVATE bool              _gfx_bfg_font_table_load_dat_file (system_hashed_ansi_string      dat_file_name,
                                                             _gfx_bfg_font_table_dat*       out_result);
PRIVATE void              _gfx_bfg_font_table_release       (void*                          font_table);


/** TODO */
PRIVATE gfx_bfg_font_table _gfx_bfg_font_table_create_shared(system_hashed_ansi_string      font_name_hashed_ansi_string,
                                                             gfx_image                      bmp_file_data,
                                                             const _gfx_bfg_font_table_dat& dat_file_data)
{
    _gfx_bfg_font_table* result_ptr = nullptr;

    /* Do a few sanity checks */
    uint32_t bmp_height = 0;
    uint32_t bmp_width  = 0;
    uint32_t n_mipmaps  = 0;

    gfx_image_get_property(bmp_file_data,
                           GFX_IMAGE_PROPERTY_N_MIPMAPS,
                          &n_mipmaps);

    ASSERT_DEBUG_SYNC(n_mipmaps == 1,
                      "Unsupported number of mipmaps defined for a font table");

    gfx_image_get_mipmap_property(bmp_file_data,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_HEIGHT,
                                 &bmp_height);
    gfx_image_get_mipmap_property(bmp_file_data,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_WIDTH,
                                 &bmp_width);

    ASSERT_DEBUG_SYNC(bmp_width  == dat_file_data.map_width,
                      "Map width does not match corresponding .bmp width.");

    ASSERT_DEBUG_SYNC(bmp_height == dat_file_data.map_height,
                      "Map height does not match corresponding .bmp height.");

    if (bmp_width  != dat_file_data.map_width ||
        bmp_height != dat_file_data.map_height)
    {
        LOG_ERROR(".bmp / .dat mismatch!");

        goto end;
    }

    /* Still fine? We can create a object instance, then ! */
    result_ptr = new (std::nothrow) _gfx_bfg_font_table;

    ASSERT_DEBUG_SYNC(result_ptr != nullptr,
                      "Out of memory while allocating BFG font table instance!");

    if (result_ptr == nullptr)
    {
        LOG_ERROR("Could not allocate memory for BFG font table instance.");

        goto end;
    }

    /* Fill the object. */
    result_ptr->bmp_data = bmp_file_data;
    result_ptr->dat_data = dat_file_data;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_ptr,
                                                   _gfx_bfg_font_table_release,
                                                   OBJECT_TYPE_GFX_BFG_FONT_TABLE,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\GFX BFG Font Tables\\",
                                                                                                           system_hashed_ansi_string_get_buffer(font_name_hashed_ansi_string)) );

end:
    return (gfx_bfg_font_table) result_ptr;
}

/** Please see header for specification */
PRIVATE bool _gfx_bfg_font_table_load_dat_blob(void*                    blob,
                                               _gfx_bfg_font_table_dat* out_result)
{
    memcpy(out_result,
           blob,
           sizeof(_gfx_bfg_font_table_dat) );

    return true;
}

/** Please see header for specification */
PRIVATE bool _gfx_bfg_font_table_load_dat_file(system_hashed_ansi_string dat_file_name,
                                               _gfx_bfg_font_table_dat*  out_result)
{
    /* Open the file */
    FILE* file   = ::fopen(system_hashed_ansi_string_get_buffer(dat_file_name),
                           "rb");
    bool  result = false;

    ASSERT_DEBUG_SYNC(file != nullptr,
                      "%s could not have been found",
                      system_hashed_ansi_string_get_buffer(dat_file_name) );

    if (file == nullptr)
    {
        LOG_ERROR("BFG font table .dat file could not have been found (%s)",
                  system_hashed_ansi_string_get_buffer(dat_file_name) );

        goto end;
    }

    /* Try to read header contents */
    result = (::fread(out_result,
                      sizeof(*out_result),
                      1, /* count */
                      file) == 1);

    ASSERT_DEBUG_SYNC(result,
                      "Could not read header contents for file [%s].",
                      system_hashed_ansi_string_get_buffer(dat_file_name) );

    if (!result)
    {
        LOG_ERROR("BFG font table .dat file is corrupt - header could not have been read completely. [%s]",
                  system_hashed_ansi_string_get_buffer(dat_file_name) );

        goto end;
    }

    /* That's all, folks */
    ::fclose(file);

    result = true;

end:
    if (file != nullptr)
    {
        ::fclose(file);

        file = nullptr;
    }

    return result;
}

/** Please see header for specification */
PRIVATE void _gfx_bfg_font_table_release(void* font_table)
{
    if (font_table != nullptr)
    {
        _gfx_bfg_font_table* font_table_ptr = (_gfx_bfg_font_table*) font_table;

        if (font_table_ptr->bmp_data != nullptr)
        {
            gfx_image_release(font_table_ptr->bmp_data);

            font_table_ptr->bmp_data = nullptr;
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_bfg_font_table gfx_bfg_font_table_create(system_hashed_ansi_string font_table_file_name_hashed_ansi_string)
{
    gfx_bfg_font_table result = nullptr;

    /* Form file paths */
    gfx_image                 bmp_file_data;
    system_hashed_ansi_string bmp_file_name_hashed_ansi_string = nullptr;
    _gfx_bfg_font_table_dat   dat_file_data;
    system_hashed_ansi_string dat_file_name_hashed_ansi_string = nullptr;
    uint32_t                  font_table_file_name_length      = system_hashed_ansi_string_get_length(font_table_file_name_hashed_ansi_string);
    const char*               font_table_file_name             = system_hashed_ansi_string_get_buffer(font_table_file_name_hashed_ansi_string);
    uint32_t                  bmp_dat_file_name_length         = font_table_file_name_length + 4 + 1;
    char*                     bmp_file_name                    = new (std::nothrow) char[bmp_dat_file_name_length];
    char*                     dat_file_name                    = new (std::nothrow) char[bmp_dat_file_name_length];

    ASSERT_DEBUG_SYNC(bmp_file_name != nullptr,
                      "Could not allocate memory for .bmp file name.");
    ASSERT_DEBUG_SYNC(dat_file_name != nullptr,
                      "Could not allocate memory for .dat file name.");

    if (bmp_file_name == nullptr ||
        dat_file_name == nullptr)
    {
        LOG_ERROR("Could not allocate memory for file name storage for [%s] file.",
                  font_table_file_name);

        goto end;
    }

    memcpy(bmp_file_name,
           font_table_file_name,
           font_table_file_name_length);
    memcpy(dat_file_name,
           font_table_file_name,
           font_table_file_name_length);

    strcpy(bmp_file_name + font_table_file_name_length,
           ".bmp");
    strcpy(dat_file_name + font_table_file_name_length,
           ".dat");

    bmp_file_name[bmp_dat_file_name_length - 1] = 0;
    dat_file_name[bmp_dat_file_name_length - 1] = 0;

    /* Create hashed ansi strings for new file names */
    bmp_file_name_hashed_ansi_string = system_hashed_ansi_string_create(bmp_file_name);
    dat_file_name_hashed_ansi_string = system_hashed_ansi_string_create(dat_file_name);

    /* Load .dat file */
    if (!_gfx_bfg_font_table_load_dat_file(dat_file_name_hashed_ansi_string,
                                          &dat_file_data) )
    {
        LOG_ERROR("Could not load BFG .dat file");

        goto end;
    }

    /* If we're fine, carry on with .bmp file */
    bmp_file_data = gfx_bmp_load_from_file(bmp_file_name_hashed_ansi_string,
                                           nullptr); /* file_unpacker */

    ASSERT_DEBUG_SYNC(bmp_file_data != nullptr,
                      "Could not load .bmp file [%s]",
                      system_hashed_ansi_string_get_buffer(bmp_file_name_hashed_ansi_string) );

    if (bmp_file_data == nullptr)
    {
        LOG_ERROR("Could not load BFG .bmp file");

        goto end;
    }

    result = _gfx_bfg_font_table_create_shared(font_table_file_name_hashed_ansi_string,
                                               bmp_file_data,
                                               dat_file_data);

end:
    if (result == nullptr)
    {
        if (bmp_file_data != nullptr)
        {
            gfx_image_release(bmp_file_data);

            bmp_file_data = nullptr;
        }
    }

    return (gfx_bfg_font_table) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_bfg_font_table gfx_bfg_font_table_create_from_ptr(system_hashed_ansi_string name,
                                                                         void*                     bmp_blob,
                                                                         void*                     dat_blob)
{
    gfx_bfg_font_table result = nullptr;

    /* Sanity check */
    ASSERT_DEBUG_SYNC(bmp_blob != nullptr &&
                      dat_blob != nullptr,
                      "Input blob(s) are NULL.");

    if (bmp_blob != nullptr &&
        dat_blob != nullptr)
    {
        /* Load .dat file */
        _gfx_bfg_font_table_dat dat_blob_data;

        if (!_gfx_bfg_font_table_load_dat_blob(dat_blob,
                                              &dat_blob_data) )
        {
            LOG_ERROR("Could not load BFG .dat file");

            goto end;
        }

        /* If we're fine, carry on with .bmp file */
        gfx_image bmp_blob_data = gfx_bmp_load_from_memory( (const unsigned char*) bmp_blob);

        ASSERT_DEBUG_SYNC(bmp_blob_data != nullptr,
                          "Could not load .bmp blob");

        if (bmp_blob_data == nullptr)
        {
            LOG_ERROR("Could not load BFG .bmp blob.");

            goto end;
        }

        result = _gfx_bfg_font_table_create_shared(name,
                                                   bmp_blob_data,
                                                   dat_blob_data);
    }

end:
    return (gfx_bfg_font_table) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API uint8_t gfx_bfg_font_table_get_maximum_character_height(gfx_bfg_font_table font_table)
{
    _gfx_bfg_font_table* font_table_ptr = (_gfx_bfg_font_table*) font_table;

    return font_table_ptr->dat_data.cell_height;
}

/** Please see header for specification */
PUBLIC EMERALD_API uint8_t gfx_bfg_font_table_get_maximum_character_width(gfx_bfg_font_table font_table)
{
    _gfx_bfg_font_table* font_table_ptr = (_gfx_bfg_font_table*) font_table;

    return font_table_ptr->dat_data.cell_width;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool gfx_bfg_font_table_get_character_properties(gfx_bfg_font_table font_table,
                                                                    uint8_t            ascii_index,
                                                                    float*             out_u1,
                                                                    float*             out_v1,
                                                                    float*             out_u2,
                                                                    float*             out_v2)
{
    _gfx_bfg_font_table*    font_table_ptr        = (_gfx_bfg_font_table*) font_table;
    _gfx_bfg_font_table_dat& map_properties       = font_table_ptr->dat_data;
    uint8_t                  n_character          = 0;
    uint32_t                 n_characters_per_row = 0;
    bool                     result               = false;
    uint32_t                 x1                   = UINT_MAX;
    uint32_t                 x2                   = UINT_MAX;
    uint32_t                 y1                   = UINT_MAX;
    uint32_t                 y2                   = UINT_MAX;

    /* Make sure the character is stored in the map */
    if (font_table_ptr->dat_data.start_character_index > ascii_index)
    {
        goto end;
    }

    /* Calculate (x,y) pair for top-left part of the character representation in the map */
    n_characters_per_row = map_properties.map_width / map_properties.cell_width;
    n_character          = ascii_index - map_properties.start_character_index;

    x1 = (n_character % n_characters_per_row) * map_properties.cell_width;
    y1 = (n_character / n_characters_per_row) * map_properties.cell_height;
    x2 = x1 + map_properties.character_widths[ascii_index];
    y2 = y1 + map_properties.cell_height;

    *out_u1 = float(x1) / float(map_properties.map_width);
    *out_u2 = float(x2) / float(map_properties.map_width);
    *out_v1 = float(y2) / float(map_properties.map_height);
    *out_v2 = float(y1) / float(map_properties.map_height);
    result  = true;

    /* That's it */
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool gfx_bfg_font_table_get_character_size(gfx_bfg_font_table font_table,
                                                              uint8_t            ascii_index,
                                                              uint8_t*           out_width,
                                                              uint8_t*           out_height)
{
    _gfx_bfg_font_table* font_table_ptr = (_gfx_bfg_font_table*) font_table;

    *out_width  = font_table_ptr->dat_data.character_widths[ascii_index];
    *out_height = font_table_ptr->dat_data.cell_height;

    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API const unsigned char* gfx_bfg_font_table_get_data_pointer(gfx_bfg_font_table font_table)
{
    _gfx_bfg_font_table* font_table_ptr = (_gfx_bfg_font_table*) font_table;
    const unsigned char* result         = nullptr;

    gfx_image_get_mipmap_property(font_table_ptr->bmp_data,
                                  0,
                                  GFX_IMAGE_MIPMAP_PROPERTY_DATA_POINTER,
                                 &result);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void gfx_bfg_font_table_get_data_properties(gfx_bfg_font_table font_table,
                                                               uint32_t*          out_map_width,
                                                               uint32_t*          out_map_height)
{
    _gfx_bfg_font_table* font_table_ptr = (_gfx_bfg_font_table*) font_table;

    *out_map_height = font_table_ptr->dat_data.map_height;
    *out_map_width  = font_table_ptr->dat_data.map_width;
}

