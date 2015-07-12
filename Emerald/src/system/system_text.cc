/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_text.h"

/** Please see header for spec */
PUBLIC EMERALD_API bool system_text_get_float_from_text(const char* data,
                                                        float*      out_result)
{
    unsigned int              current_location         = 0;
    bool                      is_negative              = false;
    unsigned int              quotient                 = 0;
    unsigned int              n_exponent_digits        = 0;
    unsigned int              n_quotient_digits        = 0;
    unsigned int              n_remainder_digits       = 0;
    unsigned int              remainder                = 0;
    bool                      result                   = true;
    const static unsigned int tens[10]                 = {1,
                                                          10,
                                                          100,
                                                          1000,
                                                          10000,
                                                          100000,
                                                          1000000,
                                                          10000000,
                                                          100000000,
                                                          1000000000};
    const static unsigned int n_tens                   = sizeof(tens) / sizeof(tens[0]);
    unsigned int              exponent_digits[n_tens]  = {0};
    unsigned int              quotient_digits[n_tens]  = {0};
    unsigned int              remainder_digits[n_tens] = {0};

    enum
    {
        STAGE_QUOTIENT,
        STAGE_REMAINDER,
        STAGE_EXPONENT
    } current_stage = STAGE_QUOTIENT;

    enum
    {
        EXPONENT_NEGATIVE,
        EXPONENT_POSITIVE
    } exponent_type = EXPONENT_POSITIVE;

    if (data == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input argument is NULL");

        result = false;
        goto end;
    }

    /* Ignore trailing spaces */
    while (*data == ' '  ||
           *data == '\r' ||
           *data == '\n')
    {
        data++;
    }

    /* Is this a negative FP? */
    if (*data == '-')
    {
        is_negative = true;

        data++;
    }

    /* Loop over character stream until we hit \n or a space */
    while (true)
    {
        char current_character = *data;

        /* Shall we bail out? */
        if (current_character == ' '  ||
            current_character == '\n' ||
            current_character == 0x00)
        {
            break;
        }

        /* Are we switching to the remainder? */
        if (current_character == '.')
        {
            ASSERT_DEBUG_SYNC(current_stage == STAGE_QUOTIENT,
                              "Excessive . character encountered");

            current_stage    = STAGE_REMAINDER;
            current_location = 0;
        }
        else
        /* Is this 'e' ? If so, exponent data follows.
         */
        if (current_character == 'e')
        {
            /* Next character must either be - or a + */
            data++;
            current_character = *data;

            if (current_character == '-')
            {
                exponent_type = EXPONENT_NEGATIVE;
            }
            else
            {
                ASSERT_DEBUG_SYNC(current_character == '+',
                                  "Invalid character found after 'e'");
            }

            /* Reset current stage */
            current_stage = STAGE_EXPONENT;

            /* Reset current location */
            current_location = 0;
        }
        else
        /* Must be a digit */
        {
            ASSERT_DEBUG_SYNC(current_character >= '0' &&
                              current_character <= '9',
                              "Unrecognized character encountered");

            unsigned int digit = current_character - '0';

            if (current_stage == STAGE_QUOTIENT)
            {
                ASSERT_DEBUG_SYNC(current_location < n_tens,
                                  "Too many digits for the quotient part");

                n_quotient_digits                 ++;
                quotient_digits[current_location] = digit;
            }
            else
            if (current_stage == STAGE_REMAINDER)
            {
                ASSERT_DEBUG_SYNC(current_stage == STAGE_REMAINDER,
                                  "Unrecognized stage");

                n_remainder_digits                 ++;
                remainder_digits[current_location] = digit;
            }
            else
            {
                ASSERT_DEBUG_SYNC(current_stage == STAGE_EXPONENT,
                                  "Unrecognized stage");

                n_exponent_digits                 ++;
                exponent_digits[current_location] = digit;
            }

            current_location++;
        }

        /* Iterate */
        data++;
    }; /* while (true) */

    /* Convert the quotient digits we've cached into an actual number */
    for (unsigned int n_quotient_digit = 0;
                      n_quotient_digit < n_quotient_digits;
                    ++n_quotient_digit)
    {
        quotient += quotient_digits[n_quotient_digit] * tens[n_quotient_digits - n_quotient_digit - 1];
    }

    /* Convert the remainder digits we've cached into an actual number */
    for (unsigned int n_remainder_digit = 0;
                      n_remainder_digit < n_remainder_digits;
                    ++n_remainder_digit)
    {
        remainder += remainder_digits[n_remainder_digit] * tens[n_remainder_digits - n_remainder_digit - 1];
    }

    /* Use the quotient & remainder values to create the result FP value */
    *out_result = (float) quotient;

    if (n_remainder_digits > 0)
    {
        *out_result += (float) (remainder) / (float) tens[n_remainder_digits];
    }

    /* Is the scientific notation used? */
    if (n_exponent_digits > 0)
    {
        double exponent_fp    = 0.0f;
        int    exponent_value = 0;

        for (unsigned int n_exponent_digit = 0;
                          n_exponent_digit < n_exponent_digits;
                        ++n_exponent_digit)
        {
            exponent_value += exponent_digits[n_exponent_digit] * tens[n_exponent_digits - n_exponent_digit - 1];
        }

        if (exponent_type == EXPONENT_NEGATIVE)
        {
            exponent_value *= -1;
        }

        exponent_fp  = pow(10.0, exponent_value);
        *out_result *= exponent_fp;
    }

    /* Is this a negative value? */
    if (is_negative)
    {
        *out_result *= -1.0f;
    }

end:
    /* All done */
    return result;
}
