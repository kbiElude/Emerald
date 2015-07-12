/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "collada/collada_data_light.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_text.h"

/** Describes <light> node contents.
 */
typedef struct _collada_data_light
{
    system_hashed_ansi_string id;
    system_hashed_ansi_string name;
    collada_data_light_type   type;

    float color[3];
    float constant_attenuation;
    float falloff_angle;
    float falloff_exponent;
    float linear_attenuation;
    float quadratic_attenuation;
    float zfar;

    _collada_data_light()
    {
        /* Default values as per COLLADA spec */
        constant_attenuation  = 1.0f;
        falloff_angle         = 180.0f;
        falloff_exponent      = 0.0f;
        linear_attenuation    = 0.0f;
        quadratic_attenuation = 0.0f;
        zfar                  = 0.0f;
    }
} _collada_data_light;


/** TODO */
PRIVATE bool _collada_data_light_parse_color(tinyxml2::XMLElement* element_ptr,
                                             float*                result_ptr)
{
    tinyxml2::XMLElement* color_element_ptr = element_ptr->FirstChildElement("color");
    bool                  result            = false;

    ASSERT_DEBUG_SYNC(color_element_ptr != NULL,
                      "Required <color> sub-node not found.");

    if (color_element_ptr == NULL)
    {
        goto end;
    }

    sscanf(color_element_ptr->GetText(),
           "%f %f %f",
           result_ptr + 0,
           result_ptr + 1,
           result_ptr + 2);

    result = true;

end:
    return result;
}

/** Please see header for spec */
PUBLIC collada_data_light collada_data_light_create(tinyxml2::XMLElement* current_light_element_ptr)
{
    tinyxml2::XMLElement* current_child_element_ptr    = NULL;
    bool                  has_parsed_light             = false;
    _collada_data_light*  new_light_ptr                = NULL;
    tinyxml2::XMLElement* technique_common_element_ptr = NULL;

    /* Try to find <technique_common> sub-node */
    technique_common_element_ptr = current_light_element_ptr->FirstChildElement("technique_common");

    if (technique_common_element_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "<light> node does not contain a <technique_common> sub-node");

        goto end;
    }

   /* Create the descriptor */
    new_light_ptr = new (std::nothrow) _collada_data_light;

    ASSERT_DEBUG_SYNC(new_light_ptr != NULL,
                      "Out of memory")

    if (new_light_ptr == NULL)
    {
        goto end;
    }

    /* Parse the light details, given its type */
    current_child_element_ptr = technique_common_element_ptr->FirstChildElement();

    while (current_child_element_ptr != NULL)
    {
        const char* current_child_name = current_child_element_ptr->Name();

        if (strcmp(current_child_name,
                   "ambient") == 0)
        {
            new_light_ptr->type = COLLADA_DATA_LIGHT_TYPE_AMBIENT;

            /* Read mandatory items */
            if (!_collada_data_light_parse_color(current_child_element_ptr,
                                                 new_light_ptr->color) )
            {
                goto end;
            }
        }
        else
        if (strcmp(current_child_name,
                   "directional") == 0)
        {
            new_light_ptr->type = COLLADA_DATA_LIGHT_TYPE_DIRECTIONAL;

            /* Read mandatory items */
            if (!_collada_data_light_parse_color(current_child_element_ptr,
                                                 new_light_ptr->color) )
            {
                goto end;
            }
        }
        else
        if (strcmp(current_child_name,
                   "point") == 0)
        {
            new_light_ptr->type = COLLADA_DATA_LIGHT_TYPE_POINT;

            /* Read mandatory items */
            if (!_collada_data_light_parse_color(current_child_element_ptr,
                                                 new_light_ptr->color) )
            {
                goto end;
            }

            const tinyxml2::XMLElement* quadratic_attenuation_element_ptr = current_child_element_ptr->FirstChildElement("quadratic_attenuation");

            if (quadratic_attenuation_element_ptr != NULL)
            {
                system_text_get_float_from_text(quadratic_attenuation_element_ptr->GetText(),
                                               &new_light_ptr->quadratic_attenuation);
            }
            else
            {
                new_light_ptr->quadratic_attenuation = 0.0f;
            }

            /* Read optional items */
            const tinyxml2::XMLElement* constant_attenuation_element_ptr = current_child_element_ptr->FirstChildElement("constant_attenuation");
            const tinyxml2::XMLElement* linear_attenuation_element_ptr   = current_child_element_ptr->FirstChildElement("linear_attenuation");
            const tinyxml2::XMLElement* zfar_element_ptr                 = current_child_element_ptr->FirstChildElement("zfar");

            if (constant_attenuation_element_ptr != NULL)
            {
                system_text_get_float_from_text(constant_attenuation_element_ptr->GetText(),
                                               &new_light_ptr->constant_attenuation);
            }

            if (linear_attenuation_element_ptr != NULL)
            {
                system_text_get_float_from_text(linear_attenuation_element_ptr->GetText(),
                                               &new_light_ptr->linear_attenuation);
            }

            if (zfar_element_ptr != NULL)
            {
                system_text_get_float_from_text(zfar_element_ptr->GetText(),
                                               &new_light_ptr->zfar);
            }
        }
        else
        if (strcmp(current_child_name, "spot") == 0)
        {
            new_light_ptr->type = COLLADA_DATA_LIGHT_TYPE_SPOT;

            if (!_collada_data_light_parse_color(current_child_element_ptr,
                                                 new_light_ptr->color) )
            {
                goto end;
            }

            /* Read optional items */
            const tinyxml2::XMLElement* constant_attenuation_element_ptr  = current_child_element_ptr->FirstChildElement("constant_attenuation");
            const tinyxml2::XMLElement* falloff_angle_element_ptr         = current_child_element_ptr->FirstChildElement("falloff_angle");
            const tinyxml2::XMLElement* falloff_exponent_element_ptr      = current_child_element_ptr->FirstChildElement("falloff_exponent");
            const tinyxml2::XMLElement* linear_attenuation_element_ptr    = current_child_element_ptr->FirstChildElement("linear_attenuation");
            const tinyxml2::XMLElement* quadratic_attenuation_element_ptr = current_child_element_ptr->FirstChildElement("quadratic_attenuation");
            const tinyxml2::XMLElement* zfar_element_ptr                  = current_child_element_ptr->FirstChildElement("zfar");

            if (constant_attenuation_element_ptr != NULL)
            {
                system_text_get_float_from_text(constant_attenuation_element_ptr->GetText(),
                                               &new_light_ptr->constant_attenuation);
            }

            if (falloff_angle_element_ptr != NULL)
            {
                system_text_get_float_from_text(falloff_angle_element_ptr->GetText(),
                                               &new_light_ptr->falloff_angle);
            }

            if (falloff_exponent_element_ptr != NULL)
            {
                system_text_get_float_from_text(falloff_exponent_element_ptr->GetText(),
                                               &new_light_ptr->falloff_exponent);
            }

            if (linear_attenuation_element_ptr != NULL)
            {
                system_text_get_float_from_text(linear_attenuation_element_ptr->GetText(),
                                               &new_light_ptr->linear_attenuation);
            }

            if (quadratic_attenuation_element_ptr != NULL)
            {
                system_text_get_float_from_text(quadratic_attenuation_element_ptr->GetText(),
                                               &new_light_ptr->quadratic_attenuation);
            }

            if (zfar_element_ptr != NULL)
            {
                system_text_get_float_from_text(zfar_element_ptr->GetText(),
                                               &new_light_ptr->zfar);
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized light/technique sub-element found [%s]",
                              current_child_name);
        }

        /* Move on */
        current_child_element_ptr = current_child_element_ptr->NextSiblingElement();
    } /* while (current_child_element_ptr != NULL) */

    {
        new_light_ptr->id   = system_hashed_ansi_string_create(current_light_element_ptr->Attribute("id") );
        new_light_ptr->name = system_hashed_ansi_string_create(current_light_element_ptr->Attribute("name") );
    } /* if (new_light_ptr != NULL) */

end:
    return (collada_data_light) new_light_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_light_get_property(const collada_data_light          light,
                                                              collada_data_light_property property,
                                                        void*                             out_data_ptr)
{
    _collada_data_light* light_ptr = (_collada_data_light*) light;

    switch (property)
    {
        case COLLADA_DATA_LIGHT_PROPERTY_COLOR:
        {
            *((float**) out_data_ptr) = light_ptr->color;

            break;
        }

        case COLLADA_DATA_LIGHT_PROPERTY_CONSTANT_ATTENUATION:
        {
            *((float*) out_data_ptr) = light_ptr->constant_attenuation;

            break;
        }

        case COLLADA_DATA_LIGHT_PROPERTY_FALLOFF_ANGLE:
        {
            *((float*) out_data_ptr) = light_ptr->falloff_angle;

            break;
        }

        case COLLADA_DATA_LIGHT_PROPERTY_FALLOFF_EXPONENT:
        {
            *((float*) out_data_ptr) = light_ptr->falloff_exponent;

            break;
        }

        case COLLADA_DATA_LIGHT_PROPERTY_ID:
        {
            *((system_hashed_ansi_string*) out_data_ptr) = light_ptr->id;

            break;
        }

        case COLLADA_DATA_LIGHT_PROPERTY_LINEAR_ATTENUATION:
        {
            *((float*) out_data_ptr) = light_ptr->linear_attenuation;

            break;
        }

        case COLLADA_DATA_LIGHT_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) out_data_ptr) = light_ptr->name;

            break;
        }

        case COLLADA_DATA_LIGHT_PROPERTY_QUADRATIC_ATTENUATION:
        {
            *((float*) out_data_ptr) = light_ptr->quadratic_attenuation;

            break;
        }

        case COLLADA_DATA_LIGHT_PROPERTY_TYPE:
        {
            *((collada_data_light_type*) out_data_ptr) = light_ptr->type;

            break;
        }

        case COLLADA_DATA_LIGHT_PROPERTY_ZFAR:
        {
            *((float*) out_data_ptr) = light_ptr->zfar;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized COLLADA light property");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC void collada_data_light_release(collada_data_light light)
{
    delete (_collada_data_light*) light;

    light = NULL;
}

