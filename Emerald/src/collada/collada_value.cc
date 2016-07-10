/**
 *
 * Emerald (kbi/elude @2014-2016)
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
        animation   = nullptr;
        float_value = 0.0f;
        type        = COLLADA_VALUE_TYPE_UNKNOWN;
    }
} _collada_value;


/** Please see header for spec */
PUBLIC collada_value collada_value_create(collada_value_type type,
                                          const void*        data_ptr)
{
    _collada_value* new_value_ptr = new (std::nothrow) _collada_value;

    ASSERT_DEBUG_SYNC(new_value_ptr != nullptr,
                      "Out of memory");
    if (new_value_ptr == nullptr)
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
PUBLIC void collada_value_get_property(const collada_value    value,
                                       collada_value_property property,
                                       void*                  out_result_ptr)
{
    _collada_value* value_ptr = reinterpret_cast<_collada_value*>(value);

    switch (property)
    {
        case COLLADA_VALUE_PROPERTY_COLLADA_DATA_ANIMATION:
        {
            ASSERT_DEBUG_SYNC(value_ptr->type == COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
                              "Requested a COLLADA_DATA_ANIMATION instance from an incompatible collada_value");

            *reinterpret_cast<collada_data_animation*>(out_result_ptr) = value_ptr->animation;
            break;
        }

        case COLLADA_VALUE_PROPERTY_FLOAT_VALUE:
        {
            ASSERT_DEBUG_SYNC(value_ptr->type == COLLADA_VALUE_TYPE_FLOAT,
                              "Requested a FLOAT instance from an incompatible collada_value");

            *reinterpret_cast<float*>(out_result_ptr) = value_ptr->float_value;
            break;
        }

        case COLLADA_VALUE_PROPERTY_TYPE:
        {
            *reinterpret_cast<collada_value_type*>(out_result_ptr) = value_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_value_property value");
        }
    }
}

/** Please see header for spec */
PUBLIC void collada_value_release(collada_value value)
{
    delete reinterpret_cast<_collada_value*>(value);
}

/** Please see header for spec */
PUBLIC void collada_value_set(collada_value      value,
                              collada_value_type type,
                              const void*        data_ptr)
{
    _collada_value* value_ptr = reinterpret_cast<_collada_value*>(value);

    switch (type)
    {
        case COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION:
        {
            value_ptr->animation = *reinterpret_cast<const collada_data_animation*>(data_ptr);
            value_ptr->type      = type;

            break;
        }

        case COLLADA_VALUE_TYPE_FLOAT:
        {
            value_ptr->float_value = *reinterpret_cast<const float*>(data_ptr);
            value_ptr->type        = type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA value type");
        }
    }
}