/*
 * Copyright (C) 2026 prototype74
 * Not a Contribution.
 *
 * Copyright (C) 2014 The Linux Foundation. All rights reserved.
 * Copyright (C) 2008 The Android Open Source Project
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

#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <utils/Log.h>

#include "LightSensor.h"

LightSensor::LightSensor()
    : SensorBase("light_sensor"),
      mInputReader(16),
      mHasPendingEvent(false),
      mEnabled(0),
      mWhiteData(0),
      mTimestamp(0),
      mTimestampHi(0)
{
    memset(&mPendingEvent, 0, sizeof(mPendingEvent));
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = HANDLE_LIGHT;
    mPendingEvent.type = SENSOR_TYPE_LIGHT;
}

LightSensor::~LightSensor()
{
    if (mEnabled)
        enable(HANDLE_LIGHT, 0);
}

int LightSensor::enable(int handle, int en)
{
    ALOGI("LightSensor enable: mEnabled %d, handle %d, en %d", mEnabled, handle, en);

    if (mEnabled == en)
        return 0;

    if (en)
        mInputReader.resetBuffer();

    char path[512];
    snprintf(path, sizeof(path), "%senable", mSysfsPath);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        ALOGE("LightSensor enable: open fail %d", fd);
        return -errno;
    }

    mEnabled = en;
    char buf[2] = {0};
    snprintf(buf, sizeof(buf), "%d", en);
    write(fd, buf, strlen(buf) + 1);
    close(fd);

    return 0;
}

int LightSensor::setDelay(int handle, int64_t ns)
{
    ALOGI("LightSensor(%d) setDelay : %lld(ns)", handle, (long long)ns);

    char path[512];
    snprintf(path, sizeof(path), "%spoll_delay", mSysfsPath);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        ALOGE("LightSensor setDelay: open fail %d", fd);
        return -errno;
    }

    char buf[80] = {0};
    snprintf(buf, sizeof(buf), "%lld", (long long)ns);
    write(fd, buf, strlen(buf) + 1);
    close(fd);

    return 0;
}

int LightSensor::readEvents(sensors_event_t *data, int count)
{
    if (count < 1) {
        ALOGE("LightSensor: count is small(count=%d)", count);
        return 0;
    }

    if (mHasPendingEvent) {
        mHasPendingEvent = false;
        mPendingEvent.timestamp = getTimestamp();
        *data = mPendingEvent;
        return 1;
    }

    int numRead = mInputReader.fill(mDataFd);
    if (numRead <= 0) {
        if (numRead < 0)
            ALOGE("LightSensor: wrong fill(%d)", numRead);
        return 0;
    }

    int numEvents = 0;
    const struct input_event *event;

    while (count > 0) {
        if (!mInputReader.readEvent(&event))
            break;

        int type = event->type;
        int code = event->code;

        if (type == EV_REL) {
            switch (code) {
            case REL_DIAL:  // als_data + 1
                mPendingEvent.light = (float)(event->value - 1);
                break;
            case REL_WHEEL:  // white_data + 1
                mWhiteData = event->value - 1;
                break;
            case REL_X:  // timestamp HIGH 32 bits
                mTimestampHi = (int64_t)event->value << 32;
                break;
            case REL_Y:  // timestamp LOW 32 bits
                mTimestamp = (int64_t)(uint32_t)event->value;
                break;
            default:
                ALOGE("LightSensor: unknown code (code=%d)", code);
                break;
            }
        } else if (type == EV_SYN) {
            if (mEnabled) {
                if (strcmp(mChipName, "CM36686") == 0)
                    mPendingEvent.light = correctLuxCM36686((int)mPendingEvent.light, mWhiteData);
                int64_t ts = mTimestampHi | mTimestamp;
                if (ts == 0)
                    ts = getTimestamp();
                mPendingEvent.timestamp = ts;
                *data++ = mPendingEvent;
                count--;
                numEvents++;
            }
        } else {
            ALOGE("LightSensor: unknown event (type=%d, code=%d)", type, code);
        }

        mInputReader.next();
    }

    return numEvents;
}

// Compute calibrated lux from CM36686 raw ALS and white channel counts.
// Calibration coefficients are optimized for j5xnlte and may not apply to other devices.
float LightSensor::correctLuxCM36686(int als_data, int white_data)
{
    if (als_data < 3)
        return 0.0f;

    float ratio = (float)white_data / (float)als_data;
    double lux;

    /*
     * >= 0.45: incandescent/warm light (more IR content in white channel)
     * <  0.45: fluorescent/daylight (less IR, white channel relatively lower)
     */
    if (ratio >= 0.45f)
        lux = pow((double)white_data, 0.9956) * 0.17683;
    else
        lux = pow((double)white_data, 1.0634) * 0.09516;

    if (lux >= 11000.0) // map very bright conditions to fixed sunlight lux
        lux = 40000.0;

    return (float)lux;
}

static const sensor_t sSensorLightCM36686 = {
    .name = "CM36686 Light Sensor",
    .vendor = "Capella Microsystems, Inc.",
    .version = 1,
    .handle = HANDLE_LIGHT,
    .type = SENSOR_TYPE_LIGHT,
    .maxRange = 65535.0f,
    .resolution = 1.0f,
    .power = 0.75f,
    .minDelay = 0,
    .fifoReservedEventCount = 0,
    .fifoMaxEventCount = 0,
    .stringType = SENSOR_STRING_TYPE_LIGHT,
    .requiredPermission = NULL,
    .maxDelay = 0,
    .flags = SENSOR_FLAG_ON_CHANGE_MODE,
    .reserved = {},
};

int LightSensor::addSensorList(sensor_t *list, int count)
{
    if (strcmp(mChipName, "CM36686") == 0) {
        list[count] = sSensorLightCM36686;
        count++;
    } else {
        ALOGI("LightSensor: no supported chip found (%s), skipping", mChipName);
    }

    return count;
}
