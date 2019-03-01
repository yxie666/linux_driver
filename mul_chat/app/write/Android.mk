LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

BIN_VERSION="1.1"
LOCAL_SRC_FILES:= \
	mul_chat_write.c
LOCAL_CFLAGS := -fno-short-enums -DANDROID_CHANGES -DBIN_VERSION -DCHAPMS=1 -DMPPE=1
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE:= mul_chat_write
LOCAL_C_INCLUDES := $(LOCAL_PATH)\
	$(LOCAL_PATH)/../../include/hd \
	$(LOCAL_PATH)/../../include/platform
LOCAL_SHARED_LIBRARIES := libcutils libcrypto libhd libplat
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

