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
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <utils/Log.h>
#include <utils/SystemClock.h>

#include "SensorBase.h"

uint32_t SensorBase::flush_state = 0;

SensorBase::SensorBase(const char *sensorName)
    : mSensorName(sensorName), mDevFd(-1), mDataFd(-1), mSysfsPathLen(0)
{
    memset(mInputName, 0, sizeof(mInputName));
    memset(mChipName, 0, sizeof(mChipName));
    memset(mSysfsPath, 0, sizeof(mSysfsPath));

    if (sensorName == NULL)
        return;

    // Try Samsung symlink path first
    mDataFd = openLink(sensorName);
    if (mDataFd < 0) {
        // Fallback: scan /dev/input/
        mDataFd = openInput(sensorName);
        if (mDataFd < 0) {
            ALOGE("Couldn't open %s.", sensorName);
        }
    }

    if (mDataFd >= 0) {
        // Read chip name from /sys/class/sensors/<sensorName>/name
        char path[512];
        snprintf(path, sizeof(path), "%s%s/name", SYSFS_SENSORS_CLASS, sensorName);
        ALOGD("sensor class chip path: %s", path);

        FILE *f = fopen(path, "r");
        if (f) {
            if (fscanf(f, "%s", mChipName) == 1) {
                ALOGD("sensor class chip name: %s", mChipName);
            } else {
                ALOGE("SensorBase, chip name File read error.");
            }
            fclose(f);
        }

        // Build sysfs path: /sys/class/input/<eventX>/device/
        snprintf(mSysfsPath, sizeof(mSysfsPath), "%s%s/device/",
                 SYSFS_INPUT_CLASS, mInputName);
        mSysfsPathLen = strlen(mSysfsPath);
    }
}

SensorBase::~SensorBase()
{
    if (mDataFd >= 0)
        close(mDataFd);
    if (mDevFd >= 0)
        close(mDevFd);
}

int SensorBase::openLink(const char *name)
{
    char linkPath[512];
    snprintf(linkPath, sizeof(linkPath), "%s%s", SYSFS_SENSOR_EVENT_SYMLINK, name);

    DIR *dir = opendir(linkPath);
    if (dir == NULL)
        return -1;

    // Find the eventX entry in the symlink directory
    char fullPath[512];
    int len = strlen(linkPath);
    snprintf(fullPath, sizeof(fullPath), "%s/", linkPath);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            // Open /dev/input/<eventX>
            char devPath[256];
            snprintf(devPath, sizeof(devPath), "/dev/input/%s", entry->d_name);

            int fd = open(devPath, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                ALOGE("couldn't find '%s' input device", name);
                return -1;
            }

            // Verify name matches
            char devName[80] = {0};
            ioctl(fd, EVIOCGNAME(sizeof(devName) - 1), devName);
            if (strcmp(devName, name) == 0) {
                strlcpy(mInputName, entry->d_name, sizeof(mInputName));
                closedir(dir);
                return fd;
            }

            close(fd);
            closedir(dir);
            ALOGE("couldn't find '%s' input device", name);
            return -1;
        }
    }

    closedir(dir);
    ALOGE("couldn't find '%s' input device", name);
    return -1;
}

int SensorBase::openInput(const char *name)
{
    DIR *dir = opendir("/dev/input");
    if (dir == NULL)
        return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        char devPath[256];
        snprintf(devPath, sizeof(devPath), "/dev/input/%s", entry->d_name);

        int fd = open(devPath, O_RDONLY);
        if (fd < 0)
            continue;

        char devName[80] = {0};
        ioctl(fd, EVIOCGNAME(sizeof(devName) - 1), devName);
        if (strcmp(devName, name) == 0) {
            strlcpy(mInputName, entry->d_name, sizeof(mInputName));
            closedir(dir);
            return fd;
        }

        close(fd);
    }

    closedir(dir);
    ALOGE("couldn't find '%s' input device", name);
    return -1;
}

int SensorBase::getFd() const
{
    return mDataFd;
}

int SensorBase::batch(int handle, int /*flags*/, int64_t sample_ns, int64_t /*latency_ns*/)
{
    return setDelay(handle, sample_ns);
}

int SensorBase::flush(int handle)
{
    ALOGI("SensorBase::flush handle(%d)", handle);

    int fd = open(SYSFS_FLUSH_PATH, O_WRONLY);
    if (fd < 0)
        return -errno;

    char buf[10];
    snprintf(buf, sizeof(buf), "%d", handle);
    flush_state |= (1 << handle);

    int ret = write(fd, buf, strlen(buf) + 1);
    close(fd);

    if (ret < 0) {
        flush_state &= ~(1 << handle);
        ALOGE("SensorBase::failed flush write. handle(%d), ret(%d)", handle, ret);
        return -errno;
    }

    return 0;
}

int64_t SensorBase::getTimestamp()
{
    return android::elapsedRealtimeNano();
}
