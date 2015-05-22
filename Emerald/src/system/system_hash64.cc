/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_hash64.h"

/** Please see header for specification */
PUBLIC system_hash64 system_hash64_calculate(__in __notnull const char* text,
                                             __in           uint32_t    length)
{
	system_hash64 result = 0;

	for (uint32_t n_character = 0;
                  n_character < length;
                  n_character++)
	{
		result = (31 * result + text[n_character]);
	}

	return result;
}
