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
#include <sys/ioctl.h>
#include <linux/input.h>
#include <utils/Log.h>

#include "GripSensor.h"

// Grip sensor scale: (value - 1) * 5.0
#define GRIP_SCALE 5.0f

// Read initial grip state via standard input ABS ioctl
#define GRIP_IOCTL_GET_INIT EVIOCGABS(ABS_GAS)

// Samsung vendor-defined sensor type for grip
#define SENSOR_TYPE_GRIP 0x10018

GripSensor::GripSensor()
    : SensorBase("grip_sensor"),
      mInputReader(8),
      mHasPendingEvent(false),
      mEnabled(0)
{
    memset(&mPendingEvent, 0, sizeof(mPendingEvent));
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = HANDLE_GRIP;
    mPendingEvent.type = SENSOR_TYPE_GRIP;
}

GripSensor::~GripSensor()
{
    if (mEnabled)
        enable(HANDLE_GRIP, 0);
}

void GripSensor::setInitialState()
{
    int buf[6] = {0};
    int ret = ioctl(mDataFd, GRIP_IOCTL_GET_INIT, buf);
    if (ret == 0) {
        ALOGI("GripSensor: setInitialState(%d)", buf[0]);
        mPendingEvent.data[0] = (float)buf[0];
        mHasPendingEvent = true;
    }
}

int GripSensor::enable(int handle, int en)
{
    ALOGI("GripSensor enable(): mEnabled %d, handle %d, en %d", mEnabled, handle, en);

    if (mEnabled == en)
        return 0;

    if (en)
        mInputReader.resetBuffer();

    char path[512];
    snprintf(path, sizeof(path), "%senable", mSysfsPath);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        ALOGE("GripSensor enable: open fail %d", fd);
        return -errno;
    }

    mEnabled = en;
    char buf[2] = {0};
    snprintf(buf, sizeof(buf), "%d", en);
    write(fd, buf, strlen(buf) + 1);
    close(fd);

    if (mEnabled)
        setInitialState();

    return 0;
}

int GripSensor::setDelay(int handle, int64_t ns)
{
    // SX9310 is interrupt-driven, no poll_delay needed
    ALOGI("GripSensor(%d) setDelay : %lld(ns) - ignored (IRQ-driven)", handle, (long long)ns);
    return 0;
}

int GripSensor::readEvents(sensors_event_t *data, int count)
{
    if (count < 1) {
        ALOGE("GripSensor: count is small(count=%d)", count);
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
            ALOGE("GripSensor: wrong fill(%d)", numRead);
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
            if (code == REL_MISC) {  // code 9: 1=ACTIVE(near), 2=IDLE(far)
                mPendingEvent.data[0] = (float)(event->value - 1) * GRIP_SCALE;
            } else if (code == REL_MAX) {  // flush event from kernel
                // ignored here, handled by MetaEvent
            } else {
                ALOGE("GripSensor: unknown code (code=%d)", code);
            }
        } else if (type == EV_SYN) {
            mPendingEvent.timestamp = getTimestamp();
            if (mEnabled) {
                *data++ = mPendingEvent;
                count--;
                numEvents++;
            }
        } else {
            ALOGE("GripSensor: unknown event (type=%d, code=%d)", type, code);
        }

        mInputReader.next();
    }

    return numEvents;
}

static const sensor_t sSensorGripSX9310 = {
    .name = "SX9310 Grip Sensor",
    .vendor = "SEMTECH",
    .version = 1,
    .handle = HANDLE_GRIP,
    .type = SENSOR_TYPE_GRIP,
    .maxRange = 1.0f,
    .resolution = 1.0f,
    .power = 0.0f,
    .minDelay = 0,
    .fifoReservedEventCount = 0,
    .fifoMaxEventCount = 0,
    .stringType = "com.samsung.sensor.grip",
    .requiredPermission = NULL,
    .maxDelay = 0,
    .flags = SENSOR_FLAG_ON_CHANGE_MODE | SENSOR_FLAG_WAKE_UP,
    .reserved = {},
};

int GripSensor::addSensorList(sensor_t *list, int count)
{
    if (strcmp(mChipName, "SX9310") == 0) {
        list[count] = sSensorGripSX9310;
        count++;
    } else {
        ALOGE("GripSensor: undefined chip spec(%s)", mChipName);
    }

    return count;
}
