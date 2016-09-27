LOCAL_PATH := $(call my-dir)

##############################################################
#      LIBRARY
##############################################################
include $(LOCAL_PATH)/../makefiles/tcs_clear.mk
TCS_NAME := libtcs2

TCS_SRC := $(call all-c-files-under, src)
TCS_INCS := $(LOCAL_PATH)/inc \
    external/libxml2/include \
    external/webkit/Source/WebCore/ic \
    external/icu/icu4c/source/common

TCS_SHARED_LIBS_ANDROID_ONLY := libc libcutils liblog libxml2 libicuuc
TCS_STATIC_LIBS_HOST_ONLY := libxml2
TCS_SHARED_LIBS_HOST_ONLY := libicuuc-host

TCS_COPY_HEADERS := inc/tcs.h
TCS_COPY_HEADERS_TO := telephony/libtcs2

TCS_REQUIRED_MODULES := tcs2_hw_xml

TCS_TARGET := $(BUILD_SHARED_LIBRARY)
include $(LOCAL_PATH)/../makefiles/tcs_make.mk

##############################################################
#      TESTU
##############################################################
include $(LOCAL_PATH)/../makefiles/tcs_clear.mk
TCS_NAME := tcs2_test

TCS_SRC := $(call all-c-files-under, test)

TCS_SHARED_LIBS_ANDROID_ONLY := libc libcutils
TCS_SHARED_LIBS := libtcs2

TCS_TARGET := $(BUILD_EXECUTABLE)
include $(LOCAL_PATH)/../makefiles/tcs_make.mk

