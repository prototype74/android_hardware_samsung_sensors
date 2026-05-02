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

#include "ProximitySensor.h"

// ABS_DISTANCE code from getevent
#define EVENT_TYPE_PROXIMITY ABS_DISTANCE  // 0x19

// indexToValue: value * 8.0 (from stock blob)
#define PROX_SCALE 8.0f

// Max distance value (far = 8.0)
#define PROX_MAX_DISTANCE 8.0f

ProximitySensor::ProximitySensor()
    : SensorBase("proximity_sensor"),
      mInputReader(8),
      mHasPendingEvent(false),
      mEnabled(0)
{
    memset(&mPendingEvent, 0, sizeof(mPendingEvent));
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = HANDLE_PROXIMITY;
    mPendingEvent.type = SENSOR_TYPE_PROXIMITY;

    // Enable on init (stock blob does this)
    if (mDataFd >= 0)
        enable(HANDLE_PROXIMITY, 1);
}

ProximitySensor::~ProximitySensor()
{
    if (mEnabled)
        enable(HANDLE_PROXIMITY, 0);
}

int ProximitySensor::enable(int handle, int en)
{
    ALOGI("ProximitySensor enable: mEnabled %d, handle %d, en %d", mEnabled, handle, en);

    if (mEnabled == en)
        return 0;

    if (en)
        mInputReader.resetBuffer();

    char path[512];
    snprintf(path, sizeof(path), "%senable", mSysfsPath);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        ALOGE("ProximitySensor enable: open fail %d", fd);
        return -errno;
    }

    mEnabled = en;
    if (en) {
        // Report "far" as initial state
        mHasPendingEvent = true;
        mPendingEvent.distance = PROX_MAX_DISTANCE;
    }

    char buf[2] = {0};
    snprintf(buf, sizeof(buf), "%d", en);
    write(fd, buf, strlen(buf) + 1);
    close(fd);

    return 0;
}

int ProximitySensor::setDelay(int handle, int64_t ns)
{
    // CM36672P is interrupt-driven, no poll_delay needed
    ALOGI("ProximitySensor(%d) setDelay : %lld(ns) - ignored (IRQ-driven)", handle, (long long)ns);
    return 0;
}

int ProximitySensor::readEvents(sensors_event_t *data, int count)
{
    if (count < 1) {
        ALOGE("ProximitySensor: count is small(count=%d)", count);
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
            ALOGE("ProximitySensor: wrong fill(%d)", numRead);
        return 0;
    }

    int numEvents = 0;
    const struct input_event *event;

    while (count > 0) {
        if (!mInputReader.readEvent(&event))
            break;

        int type = event->type;
        int code = event->code;

        if (type == EV_ABS) {
            if (code == EVENT_TYPE_PROXIMITY) {
                float distance = (float)((unsigned int)event->value) * PROX_SCALE;
                mPendingEvent.distance = distance;
                ALOGI("ProximitySensor - %d(cm)", (int)distance);
            } else {
                ALOGE("ProximitySensor: unknown code (code=%d)", code);
            }
        } else if (type == EV_SYN) {
            mPendingEvent.timestamp = getTimestamp();
            if (mEnabled) {
                *data++ = mPendingEvent;
                count--;
                numEvents++;
            }
        } else {
            ALOGE("ProximitySensor: unknown event (type=%d, code=%d)", type, code);
        }

        mInputReader.next();
    }

    return numEvents;
}

// Static sensor info structs for supported chips
static const sensor_t sSensorProxCM36672P = {
    .name = "CM36672P Proximity Sensor",
    .vendor = "Capella Microsystems, Inc.",
    .version = 1,
    .handle = HANDLE_PROXIMITY,
    .type = SENSOR_TYPE_PROXIMITY,
    .maxRange = PROX_MAX_DISTANCE,
    .resolution = PROX_SCALE,
    .power = 0.75f,
    .minDelay = 0,
    .fifoReservedEventCount = 0,
    .fifoMaxEventCount = 0,
    .stringType = SENSOR_STRING_TYPE_PROXIMITY,
    .requiredPermission = NULL,
    .maxDelay = 0,
    .flags = SENSOR_FLAG_ON_CHANGE_MODE | SENSOR_FLAG_WAKE_UP,
    .reserved = {},
};

static const sensor_t sSensorProxGP2A = {
    .name = "GP2A Proximity Sensor",
    .vendor = "Sharp",
    .version = 1,
    .handle = HANDLE_PROXIMITY,
    .type = SENSOR_TYPE_PROXIMITY,
    .maxRange = PROX_MAX_DISTANCE,
    .resolution = PROX_SCALE,
    .power = 0.75f,
    .minDelay = 0,
    .fifoReservedEventCount = 0,
    .fifoMaxEventCount = 0,
    .stringType = SENSOR_STRING_TYPE_PROXIMITY,
    .requiredPermission = NULL,
    .maxDelay = 0,
    .flags = SENSOR_FLAG_ON_CHANGE_MODE | SENSOR_FLAG_WAKE_UP,
    .reserved = {},
};

static const sensor_t sSensorProxSTK3013 = {
    .name = "STK3013 Proximity Sensor",
    .vendor = "Sensortek",
    .version = 1,
    .handle = HANDLE_PROXIMITY,
    .type = SENSOR_TYPE_PROXIMITY,
    .maxRange = PROX_MAX_DISTANCE,
    .resolution = PROX_SCALE,
    .power = 0.75f,
    .minDelay = 0,
    .fifoReservedEventCount = 0,
    .fifoMaxEventCount = 0,
    .stringType = SENSOR_STRING_TYPE_PROXIMITY,
    .requiredPermission = NULL,
    .maxDelay = 0,
    .flags = SENSOR_FLAG_ON_CHANGE_MODE | SENSOR_FLAG_WAKE_UP,
    .reserved = {},
};

static const sensor_t sSensorProxCM36686 = {
    .name = "CM36686 Proximity Sensor",
    .vendor = "Capella Microsystems, Inc.",
    .version = 1,
    .handle = HANDLE_PROXIMITY,
    .type = SENSOR_TYPE_PROXIMITY,
    .maxRange = PROX_MAX_DISTANCE,
    .resolution = PROX_SCALE,
    .power = 0.75f,
    .minDelay = 0,
    .fifoReservedEventCount = 0,
    .fifoMaxEventCount = 0,
    .stringType = SENSOR_STRING_TYPE_PROXIMITY,
    .requiredPermission = NULL,
    .maxDelay = 0,
    .flags = SENSOR_FLAG_ON_CHANGE_MODE | SENSOR_FLAG_WAKE_UP,
    .reserved = {},
};

int ProximitySensor::addSensorList(sensor_t *list, int count)
{
    const sensor_t *src = NULL;

    if (strcmp(mChipName, "CM36672P") == 0)
        src = &sSensorProxCM36672P;
    else if (strcmp(mChipName, "GP2A") == 0)
        src = &sSensorProxGP2A;
    else if (strcmp(mChipName, "STK3013") == 0)
        src = &sSensorProxSTK3013;
    else if (strcmp(mChipName, "CM36686") == 0)
        src = &sSensorProxCM36686;

    if (src) {
        list[count] = *src;
        count++;
    } else {
        ALOGE("ProximitySensor: undefined chip spec(%s)", mChipName);
    }

    return count;
}
