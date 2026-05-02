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

#ifndef SENSOR_BASE_H
#define SENSOR_BASE_H

#include <stdint.h>
#include <hardware/sensors.h>
#include <linux/input.h>

#define SYSFS_SENSOR_EVENT_SYMLINK "/sys/class/sensor_event/symlink/"
#define SYSFS_SENSORS_CLASS        "/sys/class/sensors/"
#define SYSFS_INPUT_CLASS          "/sys/class/input/"
#define SYSFS_FLUSH_PATH           "/sys/class/sensors/sensor_dev/flush"

class SensorBase {
protected:
    const char *mSensorName;
    char mInputName[256];       // e.g. "event5"
    char mChipName[256];        // e.g. "K2HH"
    char mSysfsPath[512];       // e.g. "/sys/class/input/event5/device/"
    int  mSysfsPathLen;
    int  mDevFd;
    int  mDataFd;

    static uint32_t flush_state;

    int openLink(const char *name);
    int openInput(const char *name);

public:
    SensorBase(const char *sensorName);
    virtual ~SensorBase();

    virtual int getFd() const;
    virtual int enable(int handle, int enabled) = 0;
    virtual int setDelay(int handle, int64_t ns) = 0;
    virtual int readEvents(sensors_event_t *data, int count) = 0;
    virtual int hasPendingEvents() const = 0;
    virtual int addSensorList(sensor_t *list, int count) = 0;
    virtual int batch(int handle, int flags, int64_t sample_ns, int64_t latency_ns);
    virtual int flush(int handle);

    static int64_t getTimestamp();
};

#endif
