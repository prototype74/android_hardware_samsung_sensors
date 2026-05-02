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
#include <string.h>
#include <unistd.h>
#include <utils/Log.h>

#include "AccelerometerSensor.h"

// K2HH at ±4g, 12-bit: 9.80665 / 8192 ≈ 0.001197101
#define ACCEL_SCALE 0.001197101f

// Sensor handle for accelerometer
#define HANDLE_ACCELEROMETER 0

AccelerometerSensor::AccelerometerSensor()
    : SensorBase("accelerometer_sensor"),
      mInputReader(36),
      mHasPendingEvent(false),
      mEnabled(0),
      mDropEvent(false),
      mTimestamp(0),
      mTimestampHi(0)
{
    memset(&mPendingEvent, 0, sizeof(mPendingEvent));
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = HANDLE_ACCELEROMETER;
    mPendingEvent.type = SENSOR_TYPE_ACCELEROMETER;
}

AccelerometerSensor::~AccelerometerSensor()
{
    if (mEnabled)
        enable(HANDLE_ACCELEROMETER, 0);
}

int AccelerometerSensor::enable(int handle, int en)
{
    ALOGI("AccelerometerSensor enable: mEnabled %d, handle %d, en %d", mEnabled, handle, en);

    if (mEnabled == en)
        return 0;

    if (en)
        mInputReader.resetBuffer();

    char path[512];
    snprintf(path, sizeof(path), "%senable", mSysfsPath);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        ALOGE("AccelerometerSensor enable: open fail %d", fd);
        return -errno;
    }

    mEnabled = en;
    char buf[2] = {0};
    snprintf(buf, sizeof(buf), "%d", en);
    write(fd, buf, strlen(buf) + 1);
    close(fd);

    return 0;
}

int AccelerometerSensor::setDelay(int handle, int64_t ns)
{
    ALOGI("AccelerometerSensor(%d) setDelay : %lld(ns)", handle, (long long)ns);

    char path[512];
    snprintf(path, sizeof(path), "%spoll_delay", mSysfsPath);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        ALOGE("AccelerometerSensor setDelay: open fail %d", fd);
        return -errno;
    }

    char buf[80] = {0};
    snprintf(buf, sizeof(buf), "%lld", (long long)ns);
    write(fd, buf, strlen(buf) + 1);
    close(fd);

    return 0;
}

int AccelerometerSensor::readEvents(sensors_event_t *data, int count)
{
    if (count < 1) {
        ALOGE("AccelerometerSensor: count is small(count=%d)", count);
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
            ALOGE("AccelerometerSensor: wrong fill(%d)", numRead);
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
            case REL_X:
                mPendingEvent.acceleration.x = (float)event->value * ACCEL_SCALE;
                break;
            case REL_Y:
                mPendingEvent.acceleration.y = (float)event->value * ACCEL_SCALE;
                break;
            case REL_Z:
                mPendingEvent.acceleration.z = (float)event->value * ACCEL_SCALE;
                break;
            case REL_DIAL:  // code 7 - timestamp HIGH 32 bits
                mTimestampHi = (int64_t)event->value << 32;
                break;
            case REL_MISC:  // code 9 - timestamp LOW 32 bits
                mTimestamp = (int64_t)(uint32_t)event->value;
                break;
            default:
                ALOGE("AccelerometerSensor: EVENT_TYPE_ACCEL, unknown code (code=%d)", code);
                break;
            }
        } else if (type == EV_SYN) {
            if (code == SYN_REPORT) {
                if (mDropEvent) {
                    mDropEvent = false;
                } else if (mEnabled) {
                    int64_t ts = mTimestampHi | mTimestamp;
                    if (ts == 0)
                        ts = getTimestamp();
                    mPendingEvent.timestamp = ts;
                    *data++ = mPendingEvent;
                    count--;
                    numEvents++;
                }
            } else if (code == SYN_DROPPED) {
                mDropEvent = true;
                ALOGI("AccelerometerSensor: events dropped (mEnabled=%d)", mEnabled);
            } else {
                ALOGE("AccelerometerSensor: unknown code (type=%d, code=%d)", type, code);
            }
        } else {
            ALOGE("AccelerometerSensor: unknown event (type=%d, code=%d)", type, code);
        }

        mInputReader.next();
    }

    return numEvents;
}

static const sensor_t sSensorAccelK2HH = {
    .name = "K2HH Acceleration",
    .vendor = "STM",
    .version = 1,
    .handle = HANDLE_ACCELEROMETER,
    .type = SENSOR_TYPE_ACCELEROMETER,
    .maxRange = 39.2266f,   // ±4g in m/s²
    .resolution = ACCEL_SCALE,
    .power = 0.13f,
    .minDelay = 10000,      // 100 Hz
    .fifoReservedEventCount = 0,
    .fifoMaxEventCount = 0,
    .stringType = SENSOR_STRING_TYPE_ACCELEROMETER,
    .requiredPermission = NULL,
    .maxDelay = 200000,     // 5 Hz
    .flags = SENSOR_FLAG_CONTINUOUS_MODE,
    .reserved = {},
};

int AccelerometerSensor::addSensorList(sensor_t *list, int count)
{
    if (strcmp(mChipName, "K2HH") == 0) {
        list[count] = sSensorAccelK2HH;
        count++;
    } else {
        ALOGE("AccelerometerSensor: undefined chip spec(%s)", mChipName);
    }

    return count;
}
