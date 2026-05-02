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

#include <string.h>
#include <utils/Log.h>

#include "MetaEvent.h"

MetaEvent::MetaEvent()
    : SensorBase("meta_event"),
      mInputReader(30),
      mHasPendingEvent(false),
      mFlushTimestamp(0),
      mFlushHandle(0)
{
    memset(&mPendingEvent, 0, sizeof(mPendingEvent));
    mPendingEvent.version = META_DATA_VERSION;
    mPendingEvent.type = SENSOR_TYPE_META_DATA;
}

MetaEvent::~MetaEvent()
{
}

int MetaEvent::readEvents(sensors_event_t *data, int count)
{
    if (count < 1) {
        ALOGE("MetaEvent: count is small(count=%d)", count);
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
            ALOGE("MetaEvent: wrong fill(%d)", numRead);
        else
            ALOGE("MetaEvent: no fill(%d)", numRead);
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
            if (code == REL_DIAL) {  // code 7 - flush complete flag (always 1)
                mFlushTimestamp = event->value;
            } else if (code == REL_HWHEEL) {  // code 6 - sensor_type + 1
                mFlushHandle = event->value - 1;
            } else {
                ALOGE("MetaEvent: unknown code (code=%d)", code);
            }
        } else if (type == EV_SYN) {
            // Clamp handle
            if (mFlushHandle < 0)
                mFlushHandle = 0;

            mPendingEvent.meta_data.sensor = mFlushHandle;
            mPendingEvent.meta_data.what = META_DATA_FLUSH_COMPLETE;
            mPendingEvent.timestamp = 0;

            *data++ = mPendingEvent;
            count--;
            numEvents++;

            // Clear flush state for this handle
            flush_state &= ~(1 << mFlushHandle);
        } else {
            ALOGE("MetaEvent: unknown event (type=%d, code=%d)", type, code);
        }

        mInputReader.next();
    }

    return numEvents;
}
