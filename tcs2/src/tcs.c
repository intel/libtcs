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


#ifndef HOST_BUILD
#include <cutils/properties.h>
#endif

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "tcs.h"

/* ASSERT macro */
#define xstr(s) str(s)
#define str(s) #s

#ifdef __GNUC__
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

#define DASSERT(exp, format, ...) do { \
        if (unlikely(!(exp))) { \
            if (unlikely(format[0] != '\0')) \
                LOGE("AssertionLog " format, ## __VA_ARGS__); \
            LOGE("%s:%d Assertion '" xstr(exp) "'", __FILE__, __LINE__); \
            abort(); \
        } \
} while (0)

#define ASSERT(exp) DASSERT(exp, "")

/* XML tags */
#define ATTR_KEY ((const xmlChar *)"key")
#define ATTR_NAME ((const xmlChar *)"name")
#define ATTR_OVERLAY_MODE ((const xmlChar *)"overlay")

#define TAG_GROUP ((const xmlChar *)"group")
#define TAG_CONFIG ((const xmlChar *)"config")
#define TAG_LIST ((const xmlChar *)"list")
#define TAG_STRING ((const xmlChar *)"string")
#define TAG_INT ((const xmlChar *)"int")
#define TAG_BOOL ((const xmlChar *)"bool")

#define GROUP_SEPARATOR '.'

/* FILESYSTEM */
#define TCS_XML_FOLDER "/system/vendor/etc/telephony/tcs"
#define TCS_SYSFS_CONFIG_NAME "/sys/kernel/telephony/config_name"
#define TCS_OVERLAY_FOLDER "/system/vendor/etc/telephony/catalog"

/* PROPERTIES */
#define TCS_KEY_ANDROID_BUILD "ro.build.type"
#define TCS_KEY_HW_FILENAME "ro.telephony.tcs.hw_name"    // set by MIXIN for platforms with no BIOS
#define TCS_KEY_SW_FOLDER "ro.telephony.tcs.sw_folder"    // set by MIXIN

/* DEBUG PROPERTIES */
// set by user (in debug mode) to force HW configuration file
#define TCS_KEY_DBG_HW_FILENAME "persist.tcs.hw_filename"
// set by user (in debug mode) to force overlay folder
#define TCS_KEY_DBG_SW_FOLDER "persist.tcs.sw_folder"
// set by HOST test apps
#define TCS_KEY_DBG_HOST_HW_FOLDER "tcs.dbg.host.hw_folder"
#define TCS_KEY_DBG_HOST_OVERLAY_FOLDER "tcs.dbg.host.overlay_folder"

/* Log functions: */
#ifndef HOST_BUILD

#include <utils/Log.h>

#define VERBOSE ANDROID_LOG_VERBOSE
#define DEBUG ANDROID_LOG_DEBUG
#define ERROR ANDROID_LOG_ERROR

#define TCS_LOG(level, format, ...) \
    do { __android_log_buf_print(LOG_ID_RADIO, level, "TCS2", format, ## __VA_ARGS__); } while (0)

#else

#define DEBUG 'D'
#define VERBOSE 'V'
#define ERROR 'E'

#define TCS_LOG(level, format, ...) do { tcs_host_log("%c: " format, level,  ## __VA_ARGS__); \
} while (0)

#endif

#define LOGD(format, ...) TCS_LOG(DEBUG, "%-30s: " format "\n", __FUNCTION__, ## __VA_ARGS__)
#define LOGE(format, ...) TCS_LOG(ERROR, "%-30s: " format "\n", __FUNCTION__, ## __VA_ARGS__)
#define LOGV(format, ...) TCS_LOG(VERBOSE, format "\n", ## __VA_ARGS__)

typedef struct tcs_internal_ctx {
    tcs_ctx_t ctx; // Must be first

    xmlDocPtr doc;
    xmlNodePtr root_node;          // Node pointing to root tree
    xmlNodePtr select_group_node;  // Node pointing to selected group
    xmlNodePtr default_group_node; // Node pointing to the group provided at init

    char *select_group_name;       // Only for logging purpose

    char *hw_xml_folder;
    char *overlay_xml_folder;
} tcs_internal_ctx_t;

#ifdef HOST_BUILD

#define PROPERTY_VALUE_MAX 92

int property_get(const char *key, char *value, const char *default_value)
{
    char *tmp = getenv(key);

    if (tmp)
        snprintf(value, PROPERTY_VALUE_MAX, "%s", tmp);
    else if (default_value)
        snprintf(value, PROPERTY_VALUE_MAX, "%s", default_value);
    else
        *value = '\0';

    return strlen(value);
}

void tcs_host_log(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
#endif

static inline xmlNodePtr next_node(xmlNodePtr cur)
{
    do
        cur = cur->next;
    while ((cur != NULL) && (cur->type != XML_ELEMENT_NODE));
    return cur;
}

static void print_node(xmlNodePtr node, int level)
{
    if (!node)
        return;

    for (; node; node = next_node(node)) {
        if (!xmlStrcmp(node->name, TAG_GROUP)) {
            xmlChar *name = xmlGetProp(node, ATTR_NAME);
            LOGV("%*s====== Group: %s ======", level, " ", name);
            xmlFree(name);
            print_node(next_node(node->children), level + 4);
        } else if (!xmlStrcmp(node->name, TAG_LIST)) {
            xmlChar *name = xmlGetProp(node, ATTR_NAME);
            LOGV("%*s====== List: %s ======", level, " ", name);
            xmlFree(name);
            print_node(next_node(node->children), level + 4);
        } else {
            xmlChar *content = xmlNodeGetContent(node);
            xmlChar *key = xmlGetProp(node, ATTR_KEY);
            /* sanity test */
            ASSERT(!xmlStrcmp(node->name, TAG_STRING) ||
                   !xmlStrcmp(node->name, TAG_INT) ||
                   !xmlStrcmp(node->name, TAG_BOOL));
            LOGV("%*s<%-6s> {%-35s} (%s)", level, " ", node->name, key, content);
            xmlFree(content);
            xmlFree(key);
        }
    }
}

/**
 * @see tcs.h
 */
static void print(tcs_ctx_t *ctx)
{
    tcs_internal_ctx_t *i_ctx = (tcs_internal_ctx_t *)ctx;

    ASSERT(i_ctx);

    print_node(next_node(i_ctx->root_node->children), 0);
}

static xmlNodePtr search_node(xmlNodePtr node, const xmlChar *tag, const xmlChar *prop,
                              const xmlChar *key)
{
    ASSERT(node);
    ASSERT(tag);
    ASSERT(prop);
    ASSERT(key);

    for (; node; node = next_node(node)) {
        if (!xmlStrcmp(node->name, tag)) {
            xmlChar *attr = xmlGetProp(node, prop);
            ASSERT(attr);
            if (!xmlStrcmp(key, attr)) {
                xmlFree(attr);
                return node;
            }
            xmlFree(attr);
        }
    }

    return NULL;
}

static inline xmlNodePtr search_group(xmlNodePtr node, const xmlChar *name)
{
    return search_node(node, TAG_GROUP, ATTR_NAME, name);
}

static inline xmlNodePtr search_list(xmlNodePtr node, const xmlChar *name)
{
    return search_node(node, TAG_LIST, ATTR_NAME, name);
}

static inline xmlNodePtr search_property(xmlNodePtr node, const xmlChar *tag, const xmlChar *key)
{
    return search_node(node, tag, ATTR_KEY, key);
}

static void parse_overlay_group(xmlNodePtr overlay_node, xmlNodePtr root_node)
{
    ASSERT(overlay_node);
    ASSERT(root_node);

    root_node = next_node(root_node->children);
    ASSERT(root_node);
    overlay_node = next_node(overlay_node->children);

    for (; overlay_node; overlay_node = next_node(overlay_node)) {
        if (!xmlStrcmp(overlay_node->name, TAG_GROUP)) {
            xmlChar *group_name = xmlGetProp(overlay_node, ATTR_NAME);
            ASSERT(group_name);

            xmlNodePtr dest_node = search_group(root_node, group_name);
            xmlFree(group_name);

            if (dest_node) {
                /* group already exists. Update it */
                parse_overlay_group(overlay_node, dest_node);
            } else {
                /* group doesn't exist. Add it */
                dest_node = root_node->parent;
                ASSERT(dest_node);

                xmlNodePtr new_node = xmlCopyNodeList(overlay_node);
                ASSERT(new_node);
                ASSERT(xmlAddChildList(dest_node, new_node));
            }
        } else if (!xmlStrcmp(overlay_node->name, TAG_LIST)) {
            xmlChar *list_name = xmlGetProp(overlay_node, ATTR_NAME);
            ASSERT(list_name);

            xmlNodePtr src_node;
            xmlNodePtr dest_node = search_list(root_node, list_name);
            xmlFree(list_name);

            if (dest_node) {
                /* List already exists */
                xmlChar *overlay_mode = xmlGetProp(overlay_node, ATTR_OVERLAY_MODE);
                /* overlay_mode can be NULL */

                /* default behavior: append */
                if (overlay_mode && !xmlStrcmp(overlay_mode, (const xmlChar *)"overwrite")) {
                    /* OVERWRITE case: remove all current nodes of the list */
                    xmlNodePtr cur = dest_node->children;
                    do {
                        xmlNodePtr next = next_node(cur);
                        xmlUnlinkNode(cur);
                        xmlFreeNode(cur);
                        cur = next;
                    } while (cur);
                }
                src_node = next_node(overlay_node->children);
                if (!overlay_mode || xmlStrcmp(overlay_mode, (const xmlChar *)"overwrite"))
                    /* Assert only on empty list in append mode */
                    ASSERT(src_node);
                xmlFree(overlay_mode);
            } else {
                /* List doesn't exist */
                src_node = overlay_node;
                dest_node = root_node->parent;
                ASSERT(dest_node);
            }

            if (src_node) {
                xmlNodePtr new_node = xmlCopyNodeList(src_node);
                ASSERT(new_node);
                ASSERT(xmlAddChildList(dest_node, new_node));
            }
        } else {
            ASSERT(!xmlStrcmp(overlay_node->name, TAG_STRING) ||
                   !xmlStrcmp(overlay_node->name, TAG_INT) ||
                   !xmlStrcmp(overlay_node->name, TAG_BOOL));

            xmlChar *key = xmlGetProp(overlay_node, ATTR_KEY);
            xmlChar *value = xmlNodeGetContent(overlay_node);
            ASSERT(key);
            ASSERT(value);

            xmlNodePtr dest_node = search_property(root_node, overlay_node->name, key);
            if (dest_node) {
                /* Property exists. Overwrite it */
                xmlNodeSetContent(dest_node, value);
            } else {
                /* Property doesn't exist. Add it */
                xmlNodePtr new_node = xmlNewNode(NULL, overlay_node->name);
                ASSERT(new_node);
                xmlNewProp(new_node, ATTR_KEY, key);
                xmlNodeSetContent(new_node, value);
                ASSERT(xmlAddChild(root_node->parent, new_node));
            }

            xmlFree(value);
            xmlFree(key);
        }
    }
}

/**
 * @see tcs.h
 */
static int select_group(tcs_ctx_t *ctx, const char *group_name)
{
    tcs_internal_ctx_t *i_ctx = (tcs_internal_ctx_t *)ctx;

    ASSERT(i_ctx);
    ASSERT(group_name);

    const char *cur = group_name;
    if (*cur == GROUP_SEPARATOR) {
        if (!i_ctx->default_group_node) {
            LOGE("Group (%s) not found. No default group provided", group_name);
            return -1;
        }
        i_ctx->select_group_node = i_ctx->default_group_node;
        cur = (char *)group_name + 1;
    } else {
        i_ctx->select_group_node = i_ctx->root_node;
    }

    free(i_ctx->select_group_name);
    i_ctx->select_group_name = strdup(group_name);

    for (;; ) {
        char group_name[40];
        char *tmp = strchr(cur, GROUP_SEPARATOR);
        size_t len;
        if (tmp)
            len = (size_t)(tmp - cur + 1);
        else
            len = strlen(cur) + 1;
        ASSERT((len > 0) && (len <= sizeof(group_name)));
        snprintf(group_name, len, "%s", cur);

        i_ctx->select_group_node = search_group(next_node(i_ctx->select_group_node->children),
                                                (xmlChar *)group_name);
        if (!i_ctx->select_group_node)
            return -1;

        if (!tmp)
            break;
        else
            cur = tmp + 1;
    }

    i_ctx->select_group_node = next_node(i_ctx->select_group_node->children);
    if (!i_ctx->select_group_node) {
        LOGD("Group (%s) is empty", group_name);
        return -1;
    }

    return 0;
}

/**
 * @see tcs.h
 */
static int get_bool(tcs_ctx_t *ctx, const char *key, bool *value)
{
    tcs_internal_ctx_t *i_ctx = (tcs_internal_ctx_t *)ctx;
    int ret = -1;

    ASSERT(i_ctx);
    ASSERT(i_ctx->select_group_node);
    ASSERT(key);
    ASSERT(value);

    xmlNodePtr node = search_property(i_ctx->select_group_node, TAG_BOOL, (const xmlChar *)key);
    if (node) {
        xmlChar *str = xmlNodeGetContent(node);
        if (str) {
            if (!strcmp((char *)str, "true")) {
                *value = true;
                ret = 0;
            } else if (!strcmp((char *)str, "false")) {
                *value = false;
                ret = 0;
            } else {
                LOGE("Conversion failure for key (%s) group (%s)", key, i_ctx->select_group_name);
            }
            xmlFree(str);
        }
    }

    return ret;
}

/**
 * @see tcs.h
 */
static int get_int(tcs_ctx_t *ctx, const char *key, int *value)
{
    tcs_internal_ctx_t *i_ctx = (tcs_internal_ctx_t *)ctx;
    int ret = -1;

    ASSERT(i_ctx);
    ASSERT(i_ctx->select_group_node);
    ASSERT(key);
    ASSERT(value);

    xmlNodePtr node = search_property(i_ctx->select_group_node, TAG_INT, (const xmlChar *)key);
    if (node) {
        xmlChar *str = xmlNodeGetContent(node);
        if (str) {
            errno = 0;
            char *end_ptr = NULL;
            *value = strtol((char *)str, &end_ptr, 0);
            if ((errno != 0) || (end_ptr == (char *)str) || (*end_ptr != '\0'))
                LOGE("Conversion failure for key (%s) group (%s)", key, i_ctx->select_group_name);
            else
                ret = 0;

            xmlFree(str);
        }
    }

    return ret;
}

/**
 * @see tcs.h
 */
static char *get_string(tcs_ctx_t *ctx, const char *key)
{
    tcs_internal_ctx_t *i_ctx = (tcs_internal_ctx_t *)ctx;
    char *value = NULL;

    ASSERT(i_ctx);
    ASSERT(i_ctx->select_group_node);
    ASSERT(key);

    xmlNodePtr node = search_property(i_ctx->select_group_node, TAG_STRING, (const xmlChar *)key);
    if (node) {
        xmlChar *data = xmlNodeGetContent(node);
        ASSERT(data);
        value = strdup((char *)data);
        ASSERT(value);
        xmlFree(data);
    }

    return value;
}

/**
 * @see tcs.h
 */
static char **get_string_array(tcs_ctx_t *ctx, const char *list_name, int *nb)
{
    tcs_internal_ctx_t *i_ctx = (tcs_internal_ctx_t *)ctx;
    char **array = NULL;

    ASSERT(i_ctx);
    ASSERT(i_ctx->select_group_node);
    ASSERT(list_name);
    ASSERT(nb);

    *nb = 0;
    xmlNodePtr node = search_list(i_ctx->select_group_node, (xmlChar *)list_name);
    if (node) {
        node = next_node(node->children);
        if (!node) {
            LOGD("List (%s) is empty", list_name);
            return NULL;
        }

        xmlNodePtr tmp = node;
        do
            (*nb)++;
        while ((tmp = next_node(tmp)) != NULL);
        array = malloc(*nb * sizeof(char *));
        ASSERT(array);
        for (int i = 0; i < *nb; i++) {
            xmlChar *data = xmlNodeGetContent(node);
            ASSERT(data);
            array[i] = strdup((char *)data);
            ASSERT(array[i]);
            xmlFree(data);
            node = next_node(node);
        }
    }

    return array;
}

static bool is_user_build(void)
{
    char build[PROPERTY_VALUE_MAX] = { "\0" };

    property_get(TCS_KEY_ANDROID_BUILD, build, "");

    return strcmp(build, "user") == 0;
}

static void get_config_from_sysfs(char *name, size_t len)
{
    ASSERT(name != NULL);
    ASSERT(len > 0);

    int fd = open(TCS_SYSFS_CONFIG_NAME, O_RDONLY);
    DASSERT(fd >= 0, "Failed to open: %s. Reason: %s", TCS_SYSFS_CONFIG_NAME, strerror(errno));
    size_t read_size = read(fd, name, len);
    if ((close(fd) == 0) && (read_size > 0)) {
        ASSERT(read_size != len);
        name[read_size - 1] = '\0'; /* \n removal */
    } else {
        *name = '\0';
    }
}

static int get_config_file(char *name, size_t len)
{
    int ret = 0;
    char *type = NULL;

    ASSERT(name != NULL);
    ASSERT(len == PROPERTY_VALUE_MAX);

    *name = '\0';
    if (!is_user_build() && property_get(TCS_KEY_DBG_HW_FILENAME, name, "") > 0) {
        type = "DEBUG";
    } else if (property_get(TCS_KEY_HW_FILENAME, name, "") > 0) {
        type = "ANDROID PROPERTY";
    } else {
        get_config_from_sysfs(name, PROPERTY_VALUE_MAX);
        type = "SYSFS";
    }

    if (*name == '\0') {
        LOGE("Platform not detected");
        ret = -1;
    } else {
        LOGV("Platform (%s) set by (%s)", name, type);
    }

    return ret;
}

static char *get_overlay_folder(void)
{
#ifdef HOST_BUILD
    char value[PROPERTY_VALUE_MAX];
    property_get(TCS_KEY_DBG_HOST_OVERLAY_FOLDER, value, "");
    char *folder = strdup(value);
    ASSERT(folder);
    return folder;
#else /* HOST_BUILD */
    char *path = NULL;
    char *property = NULL;

    char tmp[PROPERTY_VALUE_MAX] = { "\0" };

    if (!is_user_build() && property_get(TCS_KEY_DBG_SW_FOLDER, tmp, "") > 0)
        property = TCS_KEY_DBG_SW_FOLDER;
    else if (property_get(TCS_KEY_SW_FOLDER, tmp, "") > 0)
        property = TCS_KEY_SW_FOLDER;

    if (property) {
        size_t size = strlen(tmp) + strlen(TCS_OVERLAY_FOLDER) + 2;
        path = malloc(sizeof(char) * size);
        ASSERT(path);
        snprintf(path, size, "%s/%s", TCS_OVERLAY_FOLDER, tmp);
        LOGV("overlay folder: %s set by %s", path, property);
    }

    return path;
#endif  /* HOST_BUILD */
}

static char *get_hw_config_folder()
{
    char *path = NULL;

#ifdef HOST_BUILD
    char value[PROPERTY_VALUE_MAX];
    property_get(TCS_KEY_DBG_HOST_HW_FOLDER, value, "");
    path = strdup(value);
    ASSERT(path);
#else /* HOST_BUILD */
    path = strdup(TCS_XML_FOLDER);
    ASSERT(path);
#endif /* HOST_BUILD */

    return path;
}

static void parse_overlay(tcs_internal_ctx_t *i_ctx, const char *group_name, bool config)
{
    ASSERT(i_ctx);
    ASSERT(group_name);

    if (!i_ctx->overlay_xml_folder)
        return;

    char *group = strdup(group_name);
    ASSERT(group);
    group = strsep(&group, "0123456789");

    char folder[256];
    snprintf(folder, sizeof(folder), "%s/%s", i_ctx->overlay_xml_folder, group);
    free(group);

    struct dirent **list = NULL;
    int nb = scandir(folder, &list, NULL, alphasort);
    for (int i = 0; i < nb; i++) {
        if (*list[i]->d_name == '.') {
            free(list[i]);
            continue;
        }

        char xml_file[256];
        snprintf(xml_file, sizeof(xml_file), "%s/%s", folder, list[i]->d_name);
        free(list[i]);

        xmlDocPtr doc = xmlReadFile(xml_file, NULL, XML_PARSE_NOENT);
        DASSERT(doc != NULL, "xml file (%s) not parsed correctly (%s)", xml_file,
                xmlGetLastError()->message);

        xmlNodePtr overlay_node = xmlDocGetRootElement(doc);
        xmlNodePtr dest_node = NULL;

        if (!config) {
            xmlNodePtr node = search_group(overlay_node, (xmlChar *)group_name);
            if (node) {
                dest_node = search_group(i_ctx->root_node->children, (xmlChar *)group_name);
                ASSERT(dest_node);
            }
        } else {
            if (!xmlStrcmp(overlay_node->name, TAG_CONFIG))
                dest_node = i_ctx->root_node;
            else
                LOGE("Tag (%s) not found in file (%s)", TAG_CONFIG, xml_file);
        }

        if (dest_node) {
            LOGD("overlay file: %s", xml_file);
            parse_overlay_group(overlay_node, dest_node);
        }
        xmlFreeDoc(doc);
    }
    free(list);
}

static xmlNodePtr priv_add_group(tcs_internal_ctx_t *i_ctx, const char *group_name,
                                 bool print_group)
{
    ASSERT(i_ctx);
    ASSERT(group_name);

    xmlNodePtr group_node = search_group(next_node(i_ctx->root_node->children),
                                         (xmlChar *)"modules");
    DASSERT(group_node, "Group (modules) not found");

    /* get XML name */
    group_node = search_property(next_node(group_node->children), TAG_STRING,
                                 (const xmlChar *)group_name);
    DASSERT(group_node, "Group (%s) not found", group_name);
    xmlChar *xml_name = xmlNodeGetContent(group_node);
    ASSERT(xml_name);

    char *module = strdup(group_name);
    ASSERT(module);
    module = strsep(&module, "0123456789");
    char path[256];
    snprintf(path, sizeof(path), "%s/%s/%s", i_ctx->hw_xml_folder, module, xml_name);
    free(module);
    xmlFree(xml_name);

    /* Add XML content */
    LOGD("xml file (%s) for group (%s)", path, group_name);
    xmlDocPtr doc = xmlReadFile(path, NULL, XML_PARSE_NOENT);
    DASSERT(doc != NULL, "xml file (%s) not parsed correctly (%s)", path,
            xmlGetLastError()->message);

    xmlNodePtr node = xmlDocGetRootElement(doc);
    ASSERT(xmlStrcmp(node->name, TAG_GROUP) == 0);

    xmlNodePtr new_node = xmlCopyNodeList(node);
    ASSERT(new_node);
    ASSERT(xmlAddChildList(i_ctx->root_node, new_node));

    xmlFreeDoc(doc);

    parse_overlay(i_ctx, group_name, false);

    node = search_group(i_ctx->root_node->children, (xmlChar *)group_name);
    ASSERT(node);
    if (print_group)
        print_node(node, 0);

    return node;
}

/**
 * @see tcs.h
 */
static void add_group(tcs_ctx_t *ctx, const char *group_name, bool print_group)
{
    priv_add_group((tcs_internal_ctx_t *)ctx, group_name, print_group);
}

static int parse_config(tcs_internal_ctx_t *i_ctx)
{
    ASSERT(i_ctx);

    char xml_file[PROPERTY_VALUE_MAX];

    int ret = get_config_file(xml_file, sizeof(xml_file));
    if (!ret) {
        char path[256];
        /* @TODO: XML files are TCS2_ prefixed because we cannot export two different XML files
         * with the same name. Remove this HACK once TCS is merged. Or maybe find another solution ?
         **/
        snprintf(path, sizeof(path), "%s/config/TCS2_%s.xml", i_ctx->hw_xml_folder, xml_file);

        LOGD("configuration file: %s", path);
        i_ctx->doc = xmlReadFile(path, NULL, XML_PARSE_NOENT);
        DASSERT(i_ctx->doc != NULL, "xml file (%s) not parsed correctly (%s)", path,
                xmlGetLastError()->message);

        i_ctx->root_node = xmlDocGetRootElement(i_ctx->doc);
        ASSERT(xmlStrcmp(i_ctx->root_node->name, TAG_CONFIG) == 0);

        parse_overlay(i_ctx, "config", true);
    }

    return ret;
}

/**
 * @see tcs.h
 */
static void dispose(tcs_ctx_t *ctx)
{
    tcs_internal_ctx_t *i_ctx = (tcs_internal_ctx_t *)ctx;

    ASSERT(i_ctx != NULL);

    xmlFreeDoc(i_ctx->doc);
    xmlCleanupParser();

    free(i_ctx->hw_xml_folder);
    free(i_ctx->overlay_xml_folder);
    free(i_ctx->select_group_name);

    free(i_ctx);
}

/**
 * @see tcs.h
 */
tcs_ctx_t *tcs2_init(const char *optional_group)
{
    tcs_internal_ctx_t *i_ctx = calloc(1, sizeof(tcs_internal_ctx_t));

    ASSERT(i_ctx != NULL);
    /* optional_group can be NULL */

    i_ctx->ctx.dispose = dispose;
    i_ctx->ctx.select_group = select_group;
    i_ctx->ctx.get_string = get_string;
    i_ctx->ctx.get_string_array = get_string_array;
    i_ctx->ctx.get_int = get_int;
    i_ctx->ctx.get_bool = get_bool;
    i_ctx->ctx.print = print;
    i_ctx->ctx.add_group = add_group;

    i_ctx->hw_xml_folder = get_hw_config_folder();
    i_ctx->overlay_xml_folder = get_overlay_folder();

    if (!parse_config(i_ctx)) {
        if (optional_group) {
            i_ctx->default_group_node = priv_add_group(i_ctx, optional_group, false);
            i_ctx->select_group_node = next_node(i_ctx->default_group_node->children);
            i_ctx->select_group_name = strdup(optional_group);
        }
        return &i_ctx->ctx;
    } else {
        dispose((tcs_ctx_t *)i_ctx);
        return NULL;
    }
}
