#############################################################################
# NB: This makefile comes from CRM project.
# If an issue is detected here, please, port the fix to CRM makefile, as well.
#############################################################################

TCS_PATH := $(call my-dir)/..

# Extract commit id
COMMIT_ID := $(shell git --git-dir=$(TCS_PATH)/.git \
        --work-tree=$(call my-dir) log --oneline -n1 \
        | sed -e 's:\s\{1,\}:\\ :g' -e 's:["&{}()]::g' \
        -e "s:'::g")

TCS_DEFAULT_CFLAGS := -Wall -Wvla -Wextra -Werror -pthread -DGIT_COMMIT_ID=\"$(COMMIT_ID)\"
ifneq ($(TCS_LANG), CPP)
TCS_DEFAULT_CFLAGS += -std=gnu99
endif

TCS_HOST_CFLAGS := -O0 -ggdb -pthread -DHOST_BUILD -DCLOCK_BOOTTIME=7
TCS_HOST_LDFLAGS := -lrt -ldl -rdynamic -lz

TCS_DEFAULT_INCS := $(TCS_PATH)/inc $(TARGET_OUT_HEADERS)/telephony

##############################################################
#      ANDROID TARGET
##############################################################
ifneq ($(TCS_DISABLE_ANDROID_TARGET), true)
include $(CLEAR_VARS)

LOCAL_MODULE := $(TCS_NAME)
LOCAL_MODULE_OWNER := intel
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SRC_FILES := $(TCS_SRC)
LOCAL_C_INCLUDES := $(TCS_DEFAULT_INCS) $(TCS_INCS)

LOCAL_CFLAGS := $(TCS_DEFAULT_CFLAGS) $(TCS_CFLAGS)

LOCAL_SHARED_LIBRARIES := $(TCS_SHARED_LIBS_ANDROID_ONLY) $(TCS_SHARED_LIBS)
LOCAL_STATIC_LIBRARIES := $(TCS_STATIC_LIBS_ANDROID_ONLY) $(TCS_STATIC_LIBS)

LOCAL_COPY_HEADERS := $(TCS_COPY_HEADERS)
LOCAL_COPY_HEADERS_TO := $(TCS_COPY_HEADERS_TO)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(TCS_EXPORT_C_INCLUDE_DIRS)

LOCAL_REQUIRED_MODULES := $(TCS_REQUIRED_MODULES)

include $(TCS_TARGET)
endif

##############################################################
#      HOST TARGET
##############################################################
ifneq ($(TCS_DISABLE_HOST_TARGET), true)
ifeq ($(tcs_host), true)
include $(CLEAR_VARS)

LOCAL_MODULE := $(TCS_NAME)
LOCAL_MODULE_OWNER := intel

LOCAL_SRC_FILES := $(TCS_SRC)
LOCAL_C_INCLUDES := $(TCS_DEFAULT_INCS) $(TCS_INCS)

LOCAL_CFLAGS := $(TCS_DEFAULT_CFLAGS) $(TCS_HOST_CFLAGS) $(TCS_CFLAGS)
LOCAL_LDFLAGS := $(TCS_HOST_LDFLAGS)

LOCAL_SHARED_LIBRARIES := $(TCS_SHARED_LIBS) $(TCS_SHARED_LIBS_HOST_ONLY)
LOCAL_STATIC_LIBRARIES := $(TCS_STATIC_LIBS_HOST_ONLY) $(TCS_STATIC_LIBS)

LOCAL_REQUIRED_MODULES := $(TCS_REQUIRED_MODULES)

ifeq ($(TCS_TARGET),$(BUILD_STATIC_LIBRARY))
    TCS_TARGET_HOST := $(BUILD_HOST_STATIC_LIBRARY)
else ifeq ($(TCS_TARGET),$(BUILD_SHARED_LIBRARY))
    TCS_TARGET_HOST := $(BUILD_HOST_SHARED_LIBRARY)
else ifeq ($(TCS_TARGET),$(BUILD_EXECUTABLE))
    TCS_TARGET_HOST := $(BUILD_HOST_EXECUTABLE)
endif

include $(TCS_TARGET_HOST)
endif
endif
