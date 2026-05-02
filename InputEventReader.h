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

#ifndef INPUT_EVENT_READER_H
#define INPUT_EVENT_READER_H

#include <linux/input.h>

class InputEventReader {
    struct input_event *mBuffer;
    struct input_event *mBufferEnd;
    struct input_event *mHead;
    struct input_event *mCurr;
    int mFreeSpace;  // in events

    void organizeBuffer();

public:
    InputEventReader(int numEvents);
    ~InputEventReader();

    int fill(int fd);
    bool readEvent(const struct input_event **event);
    void next();
    void resetBuffer();
};

#endif
