/*
 * Copyright (C) Intel 2016
 *
 * TCS has been designed by:
 *  - Cesar De Oliveira <cesar.de.oliveira@intel.com>
 *  - Lionel Ulmer <lionel.ulmer@intel.com>
 *  - Marc Bellanger <marc.bellanger@intel.com>
 *
 * Original TCS contributor is:
 *  - Cesar De Oliveira <cesar.de.oliveira@intel.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __TCS_2_HEADER__
#define __TCS_2_HEADER__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct tcs_ctx tcs_ctx_t;

/******************************************************************************
*                               IMPORTANT NOTE                               *
******************************************************************************
*                                                                            *
*                        TCS API is not thread safe                          *
*                                                                            *
* Using the same context in a multiple thread application is not safe.       *
* Getter functions may fail as selected group may have changed.              *
*                                                                            *
* You should use one instance per thread or get all your parameters first    *
* before starting your threads.                                              *
*                                                                            *
******************************************************************************/

/**
 * Initializes TCS
 *
 * Detects the running configuration, parses all XML files and dynamically
 * builds the list of parameters suitable for this platform
 *
 * @param [in] optional_group Name of the group to load. If NULL, no optional group will be loaded
 *                            If non-NULL, this group will be selected by default
 *
 * @return a valid handle. Must be freed by calling the dispose function
 */
tcs_ctx_t *tcs2_init(const char *optional_group);

struct tcs_ctx {
    /**
     * Disposes the module
     *
     * @param [in] ctx Module context
     */
    void (*dispose)(tcs_ctx_t *ctx);

    /**
     * Adds a new group
     *
     * @param [in] ctx         Module context
     * @param [in] group_name  Name of the group
     * @param [in] print_group Prints group content if true
     */
    void (*add_group)(tcs_ctx_t *ctx, const char *group_name, bool print_group);

    /**
     * Prints current configuration (common part, optional group chosen at init and groups added
     * by calling add_group() function)
     *
     * @param [in]  ctx   Module context
     *
     */
    void (*print)(tcs_ctx_t *ctx);

    /**
     * Selects the group to point to. Group must be selected before retrieving parameters with
     * getter functions
     *
     * @param [in] ctx         Module context
     * @param [in] group_path  Path of the group to point to. Use a . as separator.
     *                         If path starts with ., it starts at the root of the optional group
     *                         given during init.
     *                         full group path must be provided.
     *                         If path doesn't start with ., full group path must be provided.
     *                         Example: If "crm0" has been provided as optional group, writing
     *                         "crm0.hal" is the same than ".hal".
     *
     * @return 0 if successful
     */
    int (*select_group)(tcs_ctx_t *ctx, const char *group_path);


    /**
     * Selects the array group to point to. Group array must be selected before retrieving
     * parameters with getter functions. next_group_array must be called to move from one element
     * to next one
     *
     * @param [in] ctx        Module context
     * @param [in] group_path Path of the array group to point to. (@see select_group for details)
     *
     * @return the number of elements in the array
     * @return -1 if the group array is not found or in case of error
     */
    int (*select_group_array)(tcs_ctx_t *ctx, const char *group_name);

    /**
     * Moves to next element of the current array group
     *
     * @param [in] ctx  Module context
     *
     * @return 0 if successful
     */
    int (*next_group_array)(tcs_ctx_t *ctx);

    /**
     * Gets boolean value of current section
     *
     * @param [in]  ctx   Module context
     * @param [in]  key   Name of the key
     *
     * @return 0 if successful
     */
    int (*get_bool)(tcs_ctx_t *ctx, const char *key, bool *value);

    /**
     * Gets integer value of current section
     *
     * @param [in]  ctx  Module context
     * @param [in]  key  Name of the key
     *
     * @return 0 if successful
     */
    int (*get_int)(tcs_ctx_t *ctx, const char *key, int *value);

    /**
     * Gets string value of current section
     *
     * @param [in]  ctx   Module context
     * @param [in]  key   Name of the key
     *
     * @return valid pointer or NULL. Pointer must be freed by caller
     */
    char * (*get_string)(tcs_ctx_t *ctx, const char *key);

    /**
     * Gets boolean value of current section
     *
     * @param [in]  ctx  Module context
     * @param [in]  key  Name of the key
     * @param [out] nb   Number of strings
     *
     * @return valid pointers of a string array or NULL. Pointers must be freed by caller
     */
    char ** (*get_string_array)(tcs_ctx_t *ctx, const char *key, int *nb);
};

#ifdef __cplusplus
}
#endif
#endif  /* __TCS_2_HEADER__ */
