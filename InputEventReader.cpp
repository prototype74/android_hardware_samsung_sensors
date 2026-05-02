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
#include <unistd.h>
#include <errno.h>

#include "InputEventReader.h"

InputEventReader::InputEventReader(int numEvents)
{
    int totalEvents = numEvents + 2;  // matches stock blob behavior
    mBuffer = new struct input_event[totalEvents];
    mBufferEnd = mBuffer + numEvents;
    mHead = mBuffer;
    mCurr = mBuffer;
    mFreeSpace = numEvents;
}

InputEventReader::~InputEventReader()
{
    delete[] mBuffer;
}

void InputEventReader::organizeBuffer()
{
    if (mCurr != mHead) {
        int remaining = mHead - mCurr;
        if (remaining <= 0)
            remaining = mBufferEnd - mCurr;
        memmove(mBuffer, mCurr, remaining * sizeof(struct input_event));
    }
    mCurr = mBuffer;
    mHead = mBufferEnd + (mFreeSpace * -1);  // simplified
    // Recalculate properly:
    mHead = mBuffer + (mBufferEnd - mBuffer - mFreeSpace);
}

int InputEventReader::fill(int fd)
{
    if (mFreeSpace == 0)
        return 0;

    organizeBuffer();

    ssize_t nread = read(fd, mHead, mFreeSpace * sizeof(struct input_event));
    if (nread < 0)
        return -errno;

    if (nread % sizeof(struct input_event) != 0)
        return -EINVAL;

    int numEvents = nread / sizeof(struct input_event);
    mFreeSpace -= numEvents;
    mHead += numEvents;
    if (mHead >= mBufferEnd)
        mHead = mBuffer;

    return numEvents;
}

bool InputEventReader::readEvent(const struct input_event **event)
{
    *event = mCurr;
    int totalCapacity = mBufferEnd - mBuffer;
    return (mFreeSpace != totalCapacity);
}

void InputEventReader::next()
{
    mCurr++;
    mFreeSpace++;
    if (mCurr >= mBufferEnd)
        mCurr = mBuffer;
}

void InputEventReader::resetBuffer()
{
    mHead = mBuffer;
    mCurr = mBuffer;
    mFreeSpace = mBufferEnd - mBuffer;
}
