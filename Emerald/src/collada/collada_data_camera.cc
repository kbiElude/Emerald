/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data_camera.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_text.h"

/** Describes a single camera, as described by <camera>/<optics> */
typedef struct _collada_data_camera
{
    system_hashed_ansi_string id;
    system_hashed_ansi_string name;

    float aspect_ratio; /* = xfov         / yfov */
    float xfov;         /* = aspect_ratio * yfov */
    float yfov;         /* = aspect_ratio / xfov */
    float zfar;
    float znear;

    _collada_data_camera();
} _collada_data_camera;

/** TODO */
_collada_data_camera::_collada_data_camera()
{
    id   = system_hashed_ansi_string_get_default_empty_string();
    name = system_hashed_ansi_string_get_default_empty_string();

    aspect_ratio = 0.0f;
    xfov         = 0.0f;
    yfov         = 0.0f;
    zfar         = 0.0f;
    znear        = 0.0f;
}

/** TODO */
PUBLIC collada_data_camera collada_data_camera_create(__in __notnull tinyxml2::XMLElement* current_camera_element_ptr)
{
    _collada_data_camera* new_camera_ptr = new (std::nothrow) _collada_data_camera;

    ASSERT_DEBUG_SYNC(new_camera_ptr != NULL, "Out of memory")
    if (new_camera_ptr != NULL)
    {
        new_camera_ptr->id   = system_hashed_ansi_string_create(current_camera_element_ptr->Attribute("id") );
        new_camera_ptr->name = system_hashed_ansi_string_create(current_camera_element_ptr->Attribute("name") );

        /* Look for <optics> node inside the node */
        tinyxml2::XMLElement* optics_element_ptr = current_camera_element_ptr->FirstChildElement("optics");

        if (optics_element_ptr != NULL)
        {
            /* Find <technique_common> node */
            tinyxml2::XMLElement* technique_common_element_ptr = optics_element_ptr->FirstChildElement("technique_common");

            if (technique_common_element_ptr != NULL)
            {
                /* Two options here: orthogonal or perspective. We only support <perspective>
                 * at the moment */
                tinyxml2::XMLElement* orthogonal_element_ptr  = technique_common_element_ptr->FirstChildElement("orthogonal");
                tinyxml2::XMLElement* perspective_element_ptr = technique_common_element_ptr->FirstChildElement("perspective");

                if (orthogonal_element_ptr != NULL)
                {
                    LOG_FATAL        ("Orthogonal camera [%s] found which is unsuppoted",
                                      system_hashed_ansi_string_get_buffer(new_camera_ptr->name) );
                    ASSERT_DEBUG_SYNC(false, "Orthogonal cameras are not supported");
                }
                else
                if (perspective_element_ptr != NULL)
                {
                    tinyxml2::XMLElement* ar_element_ptr    = perspective_element_ptr->FirstChildElement("aspect_ratio");
                    tinyxml2::XMLElement* xfov_element_ptr  = perspective_element_ptr->FirstChildElement("xfov");
                    tinyxml2::XMLElement* yfov_element_ptr  = perspective_element_ptr->FirstChildElement("yfov");
                    tinyxml2::XMLElement* zfar_element_ptr  = perspective_element_ptr->FirstChildElement("zfar");
                    tinyxml2::XMLElement* znear_element_ptr = perspective_element_ptr->FirstChildElement("znear");

                    /* Spec allows the following scenarios:
                     *
                     * 1) A single <xfov> element definition;
                     * 2) A single <yfov> element definition;
                     * 3) Both an <xfov> and <yfov> element definition;
                     * 4) The <aspect_ratio> element and either <xfov> or <yfov>
                     *
                     * We only support case 4) which is used by Blender. Expand
                     * with support for other cases as necessary.
                     */
                    if (xfov_element_ptr != NULL && ar_element_ptr != NULL)
                    {
                        system_text_get_float_from_text(ar_element_ptr->GetText(),
                                                       &new_camera_ptr->aspect_ratio);
                        system_text_get_float_from_text(xfov_element_ptr->GetText(),
                                                       &new_camera_ptr->xfov);

                        new_camera_ptr->yfov = new_camera_ptr->xfov / new_camera_ptr->aspect_ratio;
                    }
                    else
                    if (yfov_element_ptr != NULL && ar_element_ptr != NULL)
                    {
                        system_text_get_float_from_text(ar_element_ptr->GetText(),
                                                       &new_camera_ptr->aspect_ratio);
                        system_text_get_float_from_text(yfov_element_ptr->GetText(),
                                                       &new_camera_ptr->yfov);

                        new_camera_ptr->xfov = new_camera_ptr->yfov * new_camera_ptr->aspect_ratio;
                    }
                    else
                    {
                        LOG_FATAL        ("Perspective camera [%s] uses unsupported parameter combination",
                                          system_hashed_ansi_string_get_buffer(new_camera_ptr->name) );
                        ASSERT_DEBUG_SYNC(false, "Unsupported perspective camera configuration");
                    }

                    if (zfar_element_ptr != NULL)
                    {
                        system_text_get_float_from_text(zfar_element_ptr->GetText(),
                                                       &new_camera_ptr->zfar);
                    }
                    else
                    {
                        LOG_FATAL        ("Perspective camera [%s] does not define a required <zfar> attribute",
                                          system_hashed_ansi_string_get_buffer(new_camera_ptr->name) );
                        ASSERT_DEBUG_SYNC(false, "<zfar> attribute is missing");
                    }

                    if (znear_element_ptr != NULL)
                    {
                        system_text_get_float_from_text(znear_element_ptr->GetText(),
                                                       &new_camera_ptr->znear);
                    }
                    else
                    {
                        LOG_FATAL        ("Perspective camera [%s] does not define a required <znear> attribute",
                                          system_hashed_ansi_string_get_buffer(new_camera_ptr->name) );
                        ASSERT_DEBUG_SYNC(false, "<znear> attribute is missing");
                    }
                } /* if (perspective_element_ptr != NULL) */
                else
                {
                    LOG_FATAL("Camera [%s] uses an unsupported type",
                              system_hashed_ansi_string_get_buffer(new_camera_ptr->name) );
                }
            } /* if (technique_common_element_ptr != NULL) */
            else
            {
                LOG_FATAL        ("No <technique_common> node defined for <camera>/<technique_common>");
                ASSERT_DEBUG_SYNC(false, "Cannot add camera");
            }
        } /* if (optics_element_ptr != NULL) */
        else
        {
            LOG_FATAL        ("No <optics> node defined for <camera>");
            ASSERT_DEBUG_SYNC(false, "Cannot add camera");
        }
    } /* if (new_camera_ptr != NULL) */

    return (collada_data_camera) new_camera_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void collada_data_camera_get_property(__in      __notnull collada_data_camera          camera,
                                                         __in                collada_data_camera_property property,
                                                         __out_opt           void*                        out_result)
{
    _collada_data_camera* camera_ptr = (_collada_data_camera*) camera;

    switch (property)
    {
        case COLLADA_DATA_CAMERA_PROPERTY_AR:
        {
            *(float*) out_result = camera_ptr->aspect_ratio;

            break;
        }

        case COLLADA_DATA_CAMERA_PROPERTY_ID:
        {
            *(system_hashed_ansi_string*) out_result = camera_ptr->id;

            break;
        }

        case COLLADA_DATA_CAMERA_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = camera_ptr->name;

            break;
        }

        case COLLADA_DATA_CAMERA_PROPERTY_XFOV:
        {
            *(float*) out_result = camera_ptr->xfov;

            break;
        }

        case COLLADA_DATA_CAMERA_PROPERTY_YFOV:
        {
            *(float*) out_result = camera_ptr->yfov;

            break;
        }

        case COLLADA_DATA_CAMERA_PROPERTY_ZFAR:
        {
            *(float*) out_result = camera_ptr->zfar;

            break;
        }

        case COLLADA_DATA_CAMERA_PROPERTY_ZNEAR:
        {
            *(float*) out_result = camera_ptr->znear;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_camera_property value");
        }
    } /* switch (property) */
}

/* Please see header for spec */
PUBLIC void collada_data_camera_release(__in __post_invalid collada_data_camera camera)
{
    if (camera != NULL)
    {
        delete (_collada_data_camera*) camera;

        camera = NULL;
    }
}