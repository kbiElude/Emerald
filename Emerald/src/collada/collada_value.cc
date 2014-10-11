/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_value.h"
#include "system/system_assertions.h"

typedef struct _collada_value
{
    collada_data_animation    animation;
    float                     float_value;
    collada_value_type        type;

    _collada_value()
    {
        animation   = NULL;
        float_value = 0.0f;
        type        = COLLADA_VALUE_TYPE_UNKNOWN;
    }
};


/** Please see header for spec */
PUBLIC collada_value collada_value_create(__in           collada_value_type type,
                                          __in __notnull const void*        data_ptr)
{
    _collada_value* new_value_ptr = new (std::nothrow) _collada_value;

    ASSERT_DEBUG_SYNC(new_value_ptr != NULL,
                      "Out of memory");
    if (new_value_ptr == NULL)
    {
        goto end;
    }

    collada_value_set( (collada_value) new_value_ptr,
                       type,
                       data_ptr);

end:
    return (collada_value) new_value_ptr;
}

/** Please see header for spec */
PUBLIC void collada_value_get_property(__in  __notnull const collada_value    value,
                                       __in            collada_value_property property,
                                       __out __notnull void*                  out_result)
{
    _collada_value* value_ptr = (_collada_value*) value;

    switch (property)
    {
        case COLLADA_VALUE_PROPERTY_COLLADA_DATA_ANIMATION:
        {
            ASSERT_DEBUG_SYNC(value_ptr->type == COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                              "Requested a COLLADA_DATA_ANIMATION instance from an incompatible collada_value");

            *(collada_data_animation*) out_result = value_ptr->animation;
            break;
        }

        case COLLADA_VALUE_PROPERTY_FLOAT_VALUE:
        {
            ASSERT_DEBUG_SYNC(value_ptr->type == COLLADA_VALUE_TYPE_FLOAT,
                              "Requested a FLOAT instance from an incompatible collada_value");

            *(float*) out_result = value_ptr->float_value;
            break;
        }

        case COLLADA_VALUE_PROPERTY_TYPE:
        {
            *(collada_value_type*) out_result = value_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_value_property value");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void collada_value_release(__in __notnull collada_value value)
{
    delete (_collada_value*) value;
}

/** Please see header for spec */
PUBLIC void collada_value_set(__in __notnull collada_value      value,
                              __in           collada_value_type type,
                              __in __notnull const void*        data_ptr)
{
    _collada_value* value_ptr = (_collada_value*) value;

    switch (type)
    {
        case COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION:
        {
            value_ptr->animation = *((collada_data_animation*) data_ptr);
            value_ptr->type      = type;

            break;
        }

        case COLLADA_VALUE_TYPE_FLOAT:
        {
            value_ptr->float_value = *((float*) data_ptr);
            value_ptr->type        = type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA value type");
        }
    } /* switch (type) */
}