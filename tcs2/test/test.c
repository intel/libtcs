#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "libtcs2/tcs.h"

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
                printf("AssertionLog " format, ## __VA_ARGS__); \
            printf("%s:%d Assertion '" xstr(exp) "'", __FILE__, __LINE__); \
            abort(); \
        } \
} while (0)

#define ASSERT(exp) DASSERT(exp, "")
/* *INDENT-OFF* */
#define XML_CONFIG \
"<config> \
    <group name=\"common\"> \
           <int key=\"test\">5</int> \
    </group> \
    <group name=\"modules\"> \
        <string key=\"crm1\">crm_test.xml</string> \
        <string key=\"streamline1\">streamline_test.xml</string> \
    </group> \
</config>"

#define XML_CONFIG_OVERLAY \
"<config> \
    <group name=\"common\"> \
           <int key=\"test\">0x20</int> \
    </group> \
</config>"


#define XML_CRM \
"<group name=\"crm1\"> \
    <group name=\"firmware_elector\"> \
        <int key=\"toto\">2</int> \
    </group> \
    <group name=\"hal\"> \
        <int key=\"ping_timeout\">5200</int> \
        <string key=\"hello_text\">hello world</string> \
        <bool key=\"boolean_true\">false</bool> \
        <bool key=\"boolean_false\">true</bool> \
    </group> \
</group>"

#define XML_CRM1_OVERLAY \
"<group name=\"crm1\"> \
    <group name=\"firmware_elector\"> \
        <int key=\"toto\">5</int> \
    </group> \
    <group name=\"hal\"> \
        <int key=\"ping_timeout\">5200</int> \
        <bool key=\"boolean_true\">true</bool> \
        <bool key=\"boolean_false\">false</bool> \
        <int key=\"new_value\">567</int> \
        <int key=\"bad_int\">1abc</int> \
        <bool key=\"bad_bool\">abc</bool> \
    </group> \
    <!-- add new group --> \
    <group name=\"new_group\"> \
        <int key=\"toto\">97264</int> \
    </group> \
</group>"

#define XML_CRM2_OVERLAY \
"<group name=\"crm2\"> \
    <group name=\"firmware_elector\"> \
        <int key=\"toto\">47145836</int> \
    </group> \
</group>"


#define XML_STREAMLINE \
"<group name=\"streamline1\"> \
    <list name=\"tlvs\"> \
        <string>TLV1</string> \
        <string>TLV2</string> \
        <string>TLV3</string> \
    </list> \
</group>"

#define XML_STREAMLINE_OVERLAY_APPEND \
"<group name=\"streamline1\"> \
    <list name=\"tlvs\" overlay=\"append\"> \
        <string>TLV4</string> \
        <string>TLV5</string> \
        <string>TLV6</string> \
    </list> \
</group>"

#define XML_STREAMLINE_OVERLAY_APPEND_DEFAULT \
"<group name=\"streamline1\"> \
    <list name=\"tlvs\"> \
        <string>TLV4</string> \
        <string>TLV5</string> \
        <string>TLV6</string> \
    </list> \
</group>"

#define XML_STREAMLINE_OVERLAY_OVERWRITE \
"<group name=\"streamline1\"> \
    <list name=\"tlvs\" overlay=\"overwrite\"> \
        <string>TLV_OVERWRITE_1</string> \
        <string>TLV_OVERWRITE_2</string> \
        <string>TLV_OVERWRITE_3</string> \
    </list> \
</group>"

#define XML_STREAMLINE_OVERLAY_OVERWRITE_EMPTY \
"<group name=\"streamline1\"> \
    <list name=\"tlvs\" overlay=\"overwrite\"> \
    </list> \
</group>"


/* *INDENT-ON* */

#define XML_ROOT_FOLDER "/tmp/tcs"
#define XML_HW_FOLDER XML_ROOT_FOLDER "/hw"
#define XML_OVERLAY_FOLDER XML_ROOT_FOLDER "/overlay"

#define XML_HW_CONFIG_FOLDER XML_HW_FOLDER "/config"
#define XML_HW_CRM_FOLDER XML_HW_FOLDER "/crm"
#define XML_HW_STREAMLINE_FOLDER XML_HW_FOLDER "/streamline"

#define XML_OVERLAY_CONFIG_FOLDER XML_OVERLAY_FOLDER "/config"
#define XML_OVERLAY_CRM_FOLDER XML_OVERLAY_FOLDER "/crm"
#define XML_OVERLAY_STREAMLINE_FOLDER XML_OVERLAY_FOLDER "/streamline"

typedef enum overlay_type {
    OVERLAY_APPEND,
    OVERLAY_APPEND_DEFAULT,
    OVERLAY_OVERWRITE,
    OVERLAY_OVERWRITE_EMPTY,
} overlay_type_t;

static void write_xml(const char *path, const char *data)
{
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0666);

    ASSERT(fd >= 0);
    write(fd, data, strlen(data));
    close(fd);
}

static void create_xml_files(overlay_type_t type)
{
    // this is a test app. let's take some shortcuts
    system("rm -fr " XML_ROOT_FOLDER);

    system("mkdir -p " XML_HW_CONFIG_FOLDER);
    system("mkdir -p " XML_HW_CRM_FOLDER);
    system("mkdir -p " XML_HW_STREAMLINE_FOLDER);

    system("mkdir -p " XML_OVERLAY_CONFIG_FOLDER);
    system("mkdir -p " XML_OVERLAY_CRM_FOLDER);
    system("mkdir -p " XML_OVERLAY_STREAMLINE_FOLDER);

    /* @TODO: remove this TCS2_ prefix */
    write_xml(XML_HW_CONFIG_FOLDER "/TCS2_test.xml", XML_CONFIG);
    write_xml(XML_HW_CRM_FOLDER "/crm_test.xml", XML_CRM);
    write_xml(XML_HW_STREAMLINE_FOLDER "/streamline_test.xml", XML_STREAMLINE);

    write_xml(XML_OVERLAY_CONFIG_FOLDER "/overlay_config.xml", XML_CONFIG_OVERLAY);
    write_xml(XML_OVERLAY_CONFIG_FOLDER "/overlay_config2.xml", XML_CRM2_OVERLAY);
    write_xml(XML_OVERLAY_CRM_FOLDER "/crm1_test.xml", XML_CRM1_OVERLAY);
    write_xml(XML_OVERLAY_CRM_FOLDER "/crm2_test.xml", XML_CRM2_OVERLAY);

    switch (type) {
    case OVERLAY_APPEND:
        write_xml(XML_OVERLAY_STREAMLINE_FOLDER "/streamline_test.xml",
                  XML_STREAMLINE_OVERLAY_APPEND);
        break;
    case OVERLAY_APPEND_DEFAULT:
        write_xml(XML_OVERLAY_STREAMLINE_FOLDER "/streamline_test.xml",
                  XML_STREAMLINE_OVERLAY_APPEND_DEFAULT);
        break;
    case OVERLAY_OVERWRITE:
        write_xml(XML_OVERLAY_STREAMLINE_FOLDER "/streamline_test.xml",
                  XML_STREAMLINE_OVERLAY_OVERWRITE);
        break;
    case OVERLAY_OVERWRITE_EMPTY:
        write_xml(XML_OVERLAY_STREAMLINE_FOLDER "/streamline_test.xml",
                  XML_STREAMLINE_OVERLAY_OVERWRITE_EMPTY);
        break;
    default: ASSERT(0);
    }
}

static void check_config(const char *group_name, bool default_group, overlay_type_t type)
{
    tcs_ctx_t *tcs = NULL;
    char prefix[10];
    char group[30];

    if (default_group) {
        tcs = tcs2_init(group_name);
        ASSERT(tcs);
        snprintf(prefix, sizeof(prefix), ".");
    } else {
        tcs = tcs2_init(NULL);
        ASSERT(tcs);
        tcs->add_group(tcs, group_name, false);
        snprintf(prefix, sizeof(prefix), "%s.", group_name);
    }

    tcs->print(tcs);

    /* COMMON */
    ASSERT(tcs->select_group(tcs, "common") == 0);
    int test;
    ASSERT(tcs->get_int(tcs, "test", &test) == 0);
    ASSERT(test == 0x20);

    /* FIRMWARE elector */
    int value;
    snprintf(group, sizeof(group), "%sfirmware_elector", prefix);
    ASSERT(tcs->select_group(tcs, group) == 0);
    ASSERT(tcs->get_int(tcs, "toto", &value) == 0);
    ASSERT(value == 5);

    /* HAL */
    snprintf(group, sizeof(group), "%shal", prefix);
    ASSERT(tcs->select_group(tcs, group) == 0);

    ASSERT(tcs->get_int(tcs, "ping_timeout", &value) == 0);
    ASSERT(value == 5200);

    ASSERT(tcs->get_int(tcs, "new_value", &value) == 0);
    ASSERT(value == 567);

    char *str = tcs->get_string(tcs, "hello_text");
    ASSERT(str);
    ASSERT(!strcmp(str, "hello world"));
    free(str);

    bool flag;
    ASSERT(tcs->get_bool(tcs, "boolean_true", &flag) == 0);
    ASSERT(flag == true);
    ASSERT(tcs->get_bool(tcs, "boolean_false", &flag) == 0);
    ASSERT(flag == false);

    /* ERROR handling */
    ASSERT(tcs->get_bool(tcs, "bad_bool", &flag) == -1);
    ASSERT(tcs->get_int(tcs, "bad_int", &value) == -1);
    ASSERT(tcs->get_bool(tcs, "wrong_key", &flag) == -1);
    ASSERT(tcs->get_int(tcs, "wrong_key", &value) == -1);
    str = tcs->get_string(tcs, "wrong_key");
    ASSERT(!str);
    char **array = tcs->get_string_array(tcs, "wrong_key", &value);
    ASSERT(!array && value == 0);
    ASSERT(tcs->select_group(tcs, "wrong_group_name") == -1);

    /* new group addition */
    snprintf(group, sizeof(group), "%snew_group", prefix);
    ASSERT(tcs->select_group(tcs, group) == 0);
    ASSERT(tcs->get_int(tcs, "toto", &value) == 0);
    ASSERT(value == 97264);

    /* DYNAMIC group LOADING */
    int nb = 0;
    tcs->add_group(tcs, "streamline1", true);
    ASSERT(tcs->select_group(tcs, "streamline1") == 0);
    char **tlvs = tcs->get_string_array(tcs, "tlvs", &nb);

    /* TCS cleanup */
    tcs->dispose(tcs);

    /* Make sure that data is still available and list is correct */
    if (type == OVERLAY_OVERWRITE_EMPTY) {
        ASSERT(nb == 0 && tlvs == NULL);
    } else {
        for (int i = 0; i < nb; i++) {
            char tlv[20];
            switch (type) {
            case OVERLAY_APPEND:
            case OVERLAY_APPEND_DEFAULT:
                ASSERT((nb == 6) && tlvs);
                snprintf(tlv, sizeof(tlv), "TLV%d", i + 1);
                break;
            case OVERLAY_OVERWRITE:
                ASSERT((nb == 3) && tlvs);
                snprintf(tlv, sizeof(tlv), "TLV_OVERWRITE_%d", i + 1);
                break;
            default: ASSERT(0);
            }
            ASSERT(strcmp(tlvs[i], tlv) == 0);
            free(tlvs[i]);
        }
    }
    free(tlvs);
}

int main()
{
    /* Configure TCS inputs */
    setenv("tcs.dbg.host.hw_folder", XML_HW_FOLDER, 1);
    setenv("tcs.dbg.host.overlay_folder", XML_OVERLAY_FOLDER, 1);
    setenv("ro.telephony.tcs.sw_folder", XML_OVERLAY_FOLDER, 1);
    setenv("ro.telephony.tcs.hw_name", "test", 1);

    create_xml_files(OVERLAY_APPEND);
    check_config("crm1", false, OVERLAY_APPEND);
    check_config("crm1", true, OVERLAY_APPEND);

    create_xml_files(OVERLAY_APPEND_DEFAULT);
    check_config("crm1", true, OVERLAY_APPEND_DEFAULT);

    create_xml_files(OVERLAY_OVERWRITE);
    check_config("crm1", true, OVERLAY_OVERWRITE);

    create_xml_files(OVERLAY_OVERWRITE_EMPTY);
    check_config("crm1", true, OVERLAY_OVERWRITE_EMPTY);

    printf("\n\n*** SUCCESS ***\n");
    return 0;
}
