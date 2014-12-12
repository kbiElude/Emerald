/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data_image.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/** Describes <image> node contents.
 *
 *  NOTE: Only file sources are supported at the moment.
 */
typedef struct _collada_data_image
{
    system_hashed_ansi_string file_name;
    system_hashed_ansi_string file_name_with_path;

    system_hashed_ansi_string id;
    system_hashed_ansi_string name;

    bool requires_mipmaps;
} _collada_data_image;


/** Please see header for spec */
PUBLIC collada_data_image collada_data_image_create(__in __notnull tinyxml2::XMLElement* current_image_element_ptr)
{
    _collada_data_image* new_image_ptr = new (std::nothrow) _collada_data_image;

    ASSERT_DEBUG_SYNC(new_image_ptr != NULL, "Out of memory")
    if (new_image_ptr != NULL)
    {
        new_image_ptr->id               = system_hashed_ansi_string_create(current_image_element_ptr->Attribute("id") );
        new_image_ptr->name             = system_hashed_ansi_string_create(current_image_element_ptr->Attribute("name") );
        new_image_ptr->requires_mipmaps = false;

        /* Look for <init_from> node inside the node */
        tinyxml2::XMLElement* init_from_element_ptr = current_image_element_ptr->FirstChildElement("init_from");

        if (init_from_element_ptr != NULL)
        {
            /* The value inside the node is the file path */
            const char* file_prefix    = "file:///";
            const char* full_file_path = init_from_element_ptr->GetText();

            if (strstr(full_file_path, file_prefix) == full_file_path)
            {
                full_file_path += strlen(file_prefix);
            }

            /* Chances are the node text includes a path to the file. If that's the case,
             * we need to extract actual file name. */
            const char* file_name = NULL;

            if ( (file_name = strrchr(full_file_path, '/')) != NULL)
            {
                file_name++;
            }
            else
            {
                file_name = full_file_path;
            }

            /* Store the strings */
            new_image_ptr->file_name           = system_hashed_ansi_string_create(file_name);
            new_image_ptr->file_name_with_path = system_hashed_ansi_string_create(full_file_path);
        } /* if (init_from_element_ptr != NULL) */
        else
        {
            LOG_FATAL        ("No <init_from> node defined for <image>");
            ASSERT_DEBUG_SYNC(false, "Cannot add image");
        }
    }

    return (collada_data_image) new_image_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_image_get_property(__in  __notnull const collada_data_image          image,
                                                        __in                  collada_data_image_property property,
                                                        __out __notnull void*                             out_data_ptr)
{
    _collada_data_image* image_ptr = (_collada_data_image*) image;

    switch (property)
    {
        case COLLADA_DATA_IMAGE_PROPERTY_ID:
        {
            *((system_hashed_ansi_string*) out_data_ptr) = image_ptr->id;

            break;
        }

        case COLLADA_DATA_IMAGE_PROPERTY_REQUIRES_MIPMAPS:
        {
            *((bool*) out_data_ptr) = image_ptr->requires_mipmaps;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized COLLADA image property");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_image_get_properties(__in      __notnull collada_data_image         image,
                                                          __out_opt           system_hashed_ansi_string* out_name,
                                                          __out_opt           system_hashed_ansi_string* out_file_name,
                                                          __out_opt           system_hashed_ansi_string* out_file_name_with_path,
                                                          __out_opt           bool*                      out_requires_mipmaps)
{
    _collada_data_image* image_ptr = (_collada_data_image*) image;

    if (out_name != NULL)
    {
        *out_name = image_ptr->name;
    }

    if (out_file_name != NULL)
    {
        *out_file_name = image_ptr->file_name;
    }

    if (out_file_name_with_path != NULL)
    {
        *out_file_name_with_path = image_ptr->file_name_with_path;
    }

    if (out_requires_mipmaps != NULL)
    {
        *out_requires_mipmaps = image_ptr->requires_mipmaps;
    }
}

/* Please see header for specification */
PUBLIC void collada_data_image_release(__in __notnull __post_invalid collada_data_image image)
{
    delete (_collada_data_image*) image;

    image = NULL;
}

/* Please see header for specification */
PUBLIC void collada_data_image_set_property(__in __notnull collada_data_image          image,
                                            __in           collada_data_image_property property,
                                            __in __notnull const void*                 data)
{
    _collada_data_image* image_ptr = (_collada_data_image*) image;

    switch (property)
    {
        case COLLADA_DATA_IMAGE_PROPERTY_ID:
        {
            image_ptr->id = *((system_hashed_ansi_string*) data);

            break;
        }

        case COLLADA_DATA_IMAGE_PROPERTY_REQUIRES_MIPMAPS:
        {
            image_ptr->requires_mipmaps = *((bool*) data);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA image property requested");
        }
    } /* switch (property) */
}
