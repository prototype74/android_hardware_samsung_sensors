LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := sensors.msm8916
LOCAL_MODULE_TAGS := optional

ifdef TARGET_2ND_ARCH
  LOCAL_MODULE_RELATIVE_PATH := hw
else
  LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
endif

LOCAL_CFLAGS += -DLOG_TAG=\"Sensors\"
LOCAL_CFLAGS += -DSENSORS_DEVICE_API_VERSION_1_3

LOCAL_SRC_FILES := \
    SensorBase.cpp \
    InputEventReader.cpp \
    AccelerometerSensor.cpp \
    ProximitySensor.cpp \
    MetaEvent.cpp \
    sensors.cpp

LOCAL_SHARED_LIBRARIES := liblog libcutils libutils

LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

include $(BUILD_SHARED_LIBRARY)
