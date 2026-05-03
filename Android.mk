LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := sensors.msm8916
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_VENDOR_MODULE := true

LOCAL_CFLAGS += -DLOG_TAG=\"Sensors\"

LOCAL_SRC_FILES := \
    SensorBase.cpp \
    InputEventReader.cpp \
    AccelerometerSensor.cpp \
    ProximitySensor.cpp \
    MetaEvent.cpp \
    sensors.cpp

ifeq ($(TARGET_USES_GRIP_SENSOR),true)
LOCAL_CFLAGS += -DGRIP_SENSOR
LOCAL_SRC_FILES += GripSensor.cpp
endif

LOCAL_SHARED_LIBRARIES := liblog libcutils libutils

LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

include $(BUILD_SHARED_LIBRARY)
