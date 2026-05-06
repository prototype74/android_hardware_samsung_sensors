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

#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include "SensorBase.h"
#include "InputEventReader.h"

#define HANDLE_LIGHT 4

class LightSensor : public SensorBase {
    InputEventReader mInputReader;
    sensors_event_t mPendingEvent;
    bool mHasPendingEvent;
    int mEnabled;
    int64_t mTimestamp;
    int64_t mTimestampHi;

public:
    LightSensor();
    virtual ~LightSensor();

    virtual int enable(int handle, int enabled);
    virtual int setDelay(int handle, int64_t ns);
    virtual int readEvents(sensors_event_t *data, int count);
    virtual int hasPendingEvents() const { return mHasPendingEvent ? 1 : 0; }
    virtual int addSensorList(sensor_t *list, int count);
};

#endif
