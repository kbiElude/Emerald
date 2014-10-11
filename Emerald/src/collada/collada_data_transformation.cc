/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data_transformation.h"
#include "collada/collada_value.h"
#include "system/system_assertions.h"
#include "system/system_resizable_vector.h"

/** Describes a single transformation as found under
 *  <visual_scene>/<node> */
typedef struct _collada_data_transformation
{
    system_hashed_ansi_string         sid;
    _collada_data_transformation_type type;

    /* Corresponds to an instance of:
     *
     * _collada_data_transformation_lookat    if LOOKAT    type is used.
     * _collada_data_transformation_matrix    if MATRIX    type is used.
     * _collada_data_transformation_rotate    if ROTATE    type is used.
     * _collada_data_transformation_scale     if SCALE     type is used.
     * _collada_data_transformation_skew      if SKEW      type is used.
     * _collada_data_transformation_translate if TRANSLATE type is used.
     */
    void* data;

    explicit _collada_data_transformation(_collada_data_transformation_type in_type);
            ~_collada_data_transformation();
} _collada_data_transformation;

/** Describes a lookat transformation (as per <lookat>) */
typedef struct _collada_data_transformation_lookat
{
    collada_value eye              [3];
    collada_value interest_position[3];
    collada_value up               [3];

    /* Constructor */
    explicit _collada_data_transformation_lookat(__in_ecount(3) __notnull const float* in_eye,
                                                 __in_ecount(3) __notnull const float* in_interest_position,
                                                 __in_ecount(3) __notnull const float* in_up);
    /* Destructor */
    ~_collada_data_transformation_lookat();
} _collada_data_transformation_lookat;

/** Describes a matrix transformation (as per <matrix>) */
typedef struct _collada_data_transformation_matrix
{
    /* NOTE: Row-by-row data ordering */
    collada_value data[4 * 4];

    /* Constructor */
    explicit _collada_data_transformation_matrix(__in_ecount(16) __notnull const float* in_data)
    {
        for (uint32_t n = 0;
                      n < 4 * 4;
                    ++n)
        {
            data[n] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT,
                                           in_data + n);
        }
    }

    /* Destructor */
    ~_collada_data_transformation_matrix()
    {
        for (uint32_t n = 0;
                      n < 4 * 4;
                    ++n)
        {
            if (data[n] != NULL)
            {
                collada_value_release(data[n]);

                data[n] = NULL;
            }
        }
    }
} _collada_data_transformation_matrix;

/** Describes a rotate transformation (as per <rotate>) */
typedef struct _collada_data_transformation_rotate
{
    collada_value axis[3];
    collada_value angle; /* expressed in degrees */

    /* Constructor */
    explicit _collada_data_transformation_rotate(__in_ecount(3) __notnull const float* in_axis,
                                                                                float  in_angle)
    {
        axis[0] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT, in_axis + 0);
        axis[1] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT, in_axis + 1);
        axis[2] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT, in_axis + 2);
        angle   = collada_value_create(COLLADA_VALUE_TYPE_FLOAT, &in_angle);
    }

    ~_collada_data_transformation_rotate()
    {
        if (axis[0] != NULL)
        {
            collada_value_release(axis[0]);

            axis[0] = NULL;
        }

        if (axis[1] != NULL)
        {
            collada_value_release(axis[1]);

            axis[1] = NULL;
        }

        if (axis[2] != NULL)
        {
            collada_value_release(axis[2]);

            axis[2] = NULL;
        }

        if (angle != NULL)
        {
            collada_value_release(angle);

            angle = NULL;
        }
    }
} _collada_data_transformation_rotate;

/** Describes a scale transformation (as per <scale>) */
typedef struct _collada_data_transformation_scale
{
    collada_value data[3];

    /* Constructor */
    explicit _collada_data_transformation_scale(__in_ecount(3) __notnull const float* in_data)
    {
        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            data[n] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT, in_data + n);
        }
    }

    ~_collada_data_transformation_scale()
    {
        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            if (data[n] != NULL)
            {
                collada_value_release(data[n]);

                data[n] = NULL;
            }
        }
    }
} _collada_data_transformation_scale;

/** Describes a skew transformation (as per <skew>) */
typedef struct _collada_data_transformation_skew
{
    collada_value angle;              /* degrees */
    collada_value rotation_axis   [3];
    collada_value translation_axis[3];

    /* Constructor */
    explicit _collada_data_transformation_skew(__in                           float  in_angle,
                                               __in_ecount(3) __notnull const float* in_rotation_axis,
                                               __in_ecount(3) __notnull const float* in_translation_axis)
    {
        angle = collada_value_create(COLLADA_VALUE_TYPE_FLOAT, &in_angle);

        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            rotation_axis   [n] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT, in_rotation_axis    + n);
            translation_axis[n] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT, in_translation_axis + n);
        }
    }

    /* Destructor */
    ~_collada_data_transformation_skew()
    {
        if (angle != NULL)
        {
            collada_value_release(angle);

            angle = NULL;
        }

        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            if (rotation_axis[n] != NULL)
            {
                collada_value_release(rotation_axis[n]);

                rotation_axis[n] = NULL;
            }

            if (translation_axis[n] != NULL)
            {
                collada_value_release(translation_axis[n]);

                translation_axis[n] = NULL;
            }
        }
    }
} _collada_data_transformation_skew;

/** Describes a translate transformation (as per <translate>) */
typedef struct _collada_data_transformation_translate
{
    collada_value data[3];

    /* Constructor */
    explicit _collada_data_transformation_translate(__in_ecount(3) __notnull const float* in_data)
    {
        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            data[n] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT, in_data + n);
        }
    }

    ~_collada_data_transformation_translate()
    {
        for (uint32_t n = 0;
                      n < 3;
                    ++n)
        {
            if (data[n] != NULL)
            {
                collada_value_release(data[n]);

                data[n] = NULL;
            }
        }
    }
} _collada_data_transformation_translate;


/** TODO */
_collada_data_transformation::_collada_data_transformation(_collada_data_transformation_type in_type)
{
    data = NULL;
    sid  = system_hashed_ansi_string_get_default_empty_string();
    type = in_type;
}

/** TODO */
_collada_data_transformation::~_collada_data_transformation()
{
    if (data != NULL)
    {
        switch (type)
        {
            case COLLADA_DATA_TRANSFORMATION_TYPE_LOOKAT:
            {
                delete (_collada_data_transformation_lookat*) data;

                break;
            }

            case COLLADA_DATA_TRANSFORMATION_TYPE_MATRIX:
            {
                delete (_collada_data_transformation_matrix*) data;

                break;
            }

            case COLLADA_DATA_TRANSFORMATION_TYPE_ROTATE:
            {
                delete (_collada_data_transformation_rotate*) data;

                break;
            }

            case COLLADA_DATA_TRANSFORMATION_TYPE_SCALE:
            {
                delete (_collada_data_transformation_scale*) data;

                break;
            }

            case COLLADA_DATA_TRANSFORMATION_TYPE_SKEW:
            {
                delete (_collada_data_transformation_skew*) data;

                break;
            }

            case COLLADA_DATA_TRANSFORMATION_TYPE_TRANSLATE:
            {
                delete (_collada_data_transformation_translate*) data;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false, "Unrecognized transformation type encountered");
            }
        } /* switch (type) */
    } /* if (data != NULL) */
}

/** TODO */
_collada_data_transformation_lookat::_collada_data_transformation_lookat(__in_ecount(3) __notnull const float* in_eye,
                                                                         __in_ecount(3) __notnull const float* in_interest_position,
                                                                         __in_ecount(3) __notnull const float* in_up)
{
    for (uint32_t n_component = 0;
                  n_component < 3;
                ++n_component)
    {
        eye              [n_component] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT, in_eye               + n_component);
        interest_position[n_component] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT, in_interest_position + n_component);
        up               [n_component] = collada_value_create(COLLADA_VALUE_TYPE_FLOAT, in_up                + n_component);
    }
}

/** TODO */
_collada_data_transformation_lookat::~_collada_data_transformation_lookat()
{
    for (uint32_t n_component = 0;
                  n_component < 3;
                ++n_component)
    {
        if (eye[n_component] != NULL)
        {
            collada_value_release(eye[n_component]);

            eye[n_component] = NULL;
        }

        if (interest_position[n_component] != NULL)
        {
            collada_value_release(interest_position[n_component]);

            interest_position[n_component] = NULL;
        }

        if (up[n_component] != NULL)
        {
            collada_value_release(up[n_component]);

            up[n_component] = NULL;
        }
    }
}


/** Please see header for specification */
PUBLIC collada_data_transformation collada_data_transformation_create_lookat(__in           __notnull tinyxml2::XMLElement* element_ptr,
                                                                             __in_ecount(9) __notnull const float*          data)
{
    _collada_data_transformation*        new_transformation_ptr      = new (std::nothrow) _collada_data_transformation       (COLLADA_DATA_TRANSFORMATION_TYPE_LOOKAT);
    _collada_data_transformation_lookat* new_transformation_data_ptr = new (std::nothrow) _collada_data_transformation_lookat(data, data + 3, data + 6);

    ASSERT_DEBUG_SYNC(new_transformation_ptr != NULL && new_transformation_data_ptr != NULL, "Out of memory");
    if (new_transformation_ptr == NULL || new_transformation_data_ptr == NULL)
    {
        goto end;
    }

    new_transformation_ptr->data = new_transformation_ptr;
    new_transformation_ptr->sid  = system_hashed_ansi_string_create(element_ptr->Attribute("sid") );

end:
    return (collada_data_transformation) new_transformation_ptr;
}

/** Please see header for specification */
PUBLIC collada_data_transformation collada_data_transformation_create_matrix(__in           __notnull tinyxml2::XMLElement* element_ptr,
                                                                             __in_ecount(16) __notnull const float*          data)
{
    _collada_data_transformation*        new_transformation_ptr      = new (std::nothrow) _collada_data_transformation       (COLLADA_DATA_TRANSFORMATION_TYPE_MATRIX);
    _collada_data_transformation_matrix* new_transformation_data_ptr = new (std::nothrow) _collada_data_transformation_matrix(data);

    ASSERT_DEBUG_SYNC(new_transformation_ptr != NULL && new_transformation_data_ptr != NULL, "Out of memory");
    if (new_transformation_ptr == NULL || new_transformation_data_ptr == NULL)
    {
        goto end;
    }

    new_transformation_ptr->data = new_transformation_data_ptr;
    new_transformation_ptr->sid  = system_hashed_ansi_string_create(element_ptr->Attribute("sid") );

end:
    return (collada_data_transformation) new_transformation_ptr;
}

/** Please see header for specification */
PUBLIC collada_data_transformation collada_data_transformation_create_rotate(__in           __notnull tinyxml2::XMLElement* element_ptr,
                                                                             __in_ecount(4) __notnull const float*          data)
{
    _collada_data_transformation*        new_transformation_ptr      = new (std::nothrow) _collada_data_transformation       (COLLADA_DATA_TRANSFORMATION_TYPE_ROTATE);
    _collada_data_transformation_rotate* new_transformation_data_ptr = new (std::nothrow) _collada_data_transformation_rotate(data, *(data + 3) );

    ASSERT_DEBUG_SYNC(new_transformation_ptr != NULL && new_transformation_data_ptr != NULL, "Out of memory");
    if (new_transformation_ptr == NULL || new_transformation_data_ptr == NULL)
    {
        goto end;
    }

    new_transformation_ptr->data = new_transformation_data_ptr;
    new_transformation_ptr->sid  = system_hashed_ansi_string_create(element_ptr->Attribute("sid") );

end:
    return (collada_data_transformation) new_transformation_ptr;
}

/** Please see header for specification */
PUBLIC collada_data_transformation collada_data_transformation_create_scale(__in            __notnull tinyxml2::XMLElement* element_ptr,
                                                                             __in_ecount(3) __notnull const float*          data)
{
    _collada_data_transformation*       new_transformation_ptr      = new (std::nothrow) _collada_data_transformation      (COLLADA_DATA_TRANSFORMATION_TYPE_SCALE);
    _collada_data_transformation_scale* new_transformation_data_ptr = new (std::nothrow) _collada_data_transformation_scale(data);

    ASSERT_DEBUG_SYNC(new_transformation_ptr != NULL && new_transformation_data_ptr != NULL, "Out of memory");
    if (new_transformation_ptr == NULL || new_transformation_data_ptr == NULL)
    {
        goto end;
    }

    new_transformation_ptr->data = new_transformation_data_ptr;
    new_transformation_ptr->sid  = system_hashed_ansi_string_create(element_ptr->Attribute("sid") );

end:
    return (collada_data_transformation) new_transformation_ptr;
}

/** Please see header for specification */
PUBLIC collada_data_transformation collada_data_transformation_create_skew(__in            __notnull tinyxml2::XMLElement* element_ptr,
                                                                           __in_ecount(7) __notnull const float*          data)
{
    _collada_data_transformation*      new_transformation_ptr      = new (std::nothrow) _collada_data_transformation     (COLLADA_DATA_TRANSFORMATION_TYPE_SKEW);
    _collada_data_transformation_skew* new_transformation_data_ptr = new (std::nothrow) _collada_data_transformation_skew(*data, data + 1, data + 4);

    ASSERT_DEBUG_SYNC(new_transformation_ptr != NULL && new_transformation_data_ptr != NULL, "Out of memory");
    if (new_transformation_ptr == NULL || new_transformation_data_ptr == NULL)
    {
        goto end;
    }

    new_transformation_ptr->data = new_transformation_data_ptr;
    new_transformation_ptr->sid  = system_hashed_ansi_string_create(element_ptr->Attribute("sid") );

end:
    return (collada_data_transformation) new_transformation_ptr;
}

/** Please see header for specification */
PUBLIC collada_data_transformation collada_data_transformation_create_translate(__in           __notnull tinyxml2::XMLElement* element_ptr,
                                                                                __in_ecount(3) __notnull const float*          data)
{
    _collada_data_transformation*           new_transformation_ptr      = new (std::nothrow) _collada_data_transformation          (COLLADA_DATA_TRANSFORMATION_TYPE_TRANSLATE);
    _collada_data_transformation_translate* new_transformation_data_ptr = new (std::nothrow) _collada_data_transformation_translate(data);

    ASSERT_DEBUG_SYNC(new_transformation_ptr != NULL && new_transformation_data_ptr != NULL, "Out of memory");
    if (new_transformation_ptr == NULL || new_transformation_data_ptr == NULL)
    {
        goto end;
    }

    new_transformation_ptr->data = new_transformation_data_ptr;
    new_transformation_ptr->sid  = system_hashed_ansi_string_create(element_ptr->Attribute("sid") );

end:
    return (collada_data_transformation) new_transformation_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_transformation_get_matrix_properties(__in                 __notnull collada_data_transformation transformation,
                                                                          __out_ecount_opt(16)           collada_value*              out_data)
{
    _collada_data_transformation* transformation_ptr = (_collada_data_transformation*) transformation;

    if (out_data != NULL)
    {
        ASSERT_DEBUG_SYNC(transformation_ptr->type == COLLADA_DATA_TRANSFORMATION_TYPE_MATRIX,
                          "Requested matrix data from a non-matrix transformation object");

        _collada_data_transformation_matrix* data_ptr = (_collada_data_transformation_matrix*) transformation_ptr->data;

        memcpy(out_data,
               data_ptr->data,
               sizeof(data_ptr->data) );
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_transformation_get_property(__in  __notnull collada_data_transformation          transformation,
                                                                 __in  __notnull collada_data_transformation_property property,
                                                                 __out __notnull void*                                out_result)
{
    _collada_data_transformation* transformation_ptr = (_collada_data_transformation*) transformation;

    switch (property)
    {
        case COLLADA_DATA_TRANSFORMATION_PROPERTY_DATA_HANDLE:
        {
            *((void**) out_result) = transformation_ptr->data;

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_PROPERTY_SID:
        {
            *((system_hashed_ansi_string*) out_result) = transformation_ptr->sid;

            break;
        }

        case COLLADA_DATA_TRANSFORMATION_PROPERTY_TYPE:
        {
            *(_collada_data_transformation_type*) out_result = transformation_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_transformation_property value");
        }
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_transformation_get_rotate_properties(__in            __notnull collada_data_transformation transformation,
                                                                          __out_ecount(3) __notnull collada_value*              axis_vector,
                                                                          __out           __notnull collada_value*              angle)
{
    _collada_data_transformation*        transformation_ptr = (_collada_data_transformation*)        transformation;
    _collada_data_transformation_rotate* rotate_data_ptr    = (_collada_data_transformation_rotate*) transformation_ptr->data;

    ASSERT_DEBUG_SYNC(transformation_ptr->type == COLLADA_DATA_TRANSFORMATION_TYPE_ROTATE,
                          "Requested rotation data from a non-rotate transformation object");

    memcpy(axis_vector,
           rotate_data_ptr->axis,
           sizeof(rotate_data_ptr->axis) );

    *angle = rotate_data_ptr->angle;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_transformation_get_scale_properties(__in            __notnull collada_data_transformation transformation,
                                                                         __out_ecount(3) __notnull collada_value*              scale_vector)
{
    _collada_data_transformation*       transformation_ptr = (_collada_data_transformation*)       transformation;
    _collada_data_transformation_scale* scale_data_ptr     = (_collada_data_transformation_scale*) transformation_ptr->data;

    ASSERT_DEBUG_SYNC(transformation_ptr->type == COLLADA_DATA_TRANSFORMATION_TYPE_SCALE,
                          "Requested scaling data from a non-scale transformation object");

    memcpy(scale_vector,
           scale_data_ptr->data,
           sizeof(scale_data_ptr->data) );
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_transformation_get_translate_properties(__in            __notnull collada_data_transformation transformation,
                                                                             __out_ecount(3) __notnull collada_value*              translation_vector)
{
    _collada_data_transformation*           transformation_ptr = (_collada_data_transformation*)           transformation;
    _collada_data_transformation_translate* translate_data_ptr = (_collada_data_transformation_translate*) transformation_ptr->data;

    ASSERT_DEBUG_SYNC(transformation_ptr->type == COLLADA_DATA_TRANSFORMATION_TYPE_TRANSLATE,
                          "Requested translation data from a non-translate transformation object");

    memcpy(translation_vector,
           translate_data_ptr->data,
           sizeof(translate_data_ptr->data) );
}

/* Please see header for spec */
PUBLIC EMERALD_API void collada_data_transformation_release(__in __notnull __post_invalid collada_data_transformation transformation)
{
    delete (_collada_data_transformation*) transformation;

    transformation = NULL;
}