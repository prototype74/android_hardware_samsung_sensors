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

#include <hardware/sensors.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <utils/Log.h>

#include "SensorBase.h"
#include "AccelerometerSensor.h"
#include "ProximitySensor.h"
#ifdef LIGHT_SENSOR
#include "LightSensor.h"
#endif
#ifdef GRIP_SENSOR
#include "GripSensor.h"
#endif
#include "MetaEvent.h"

#define MAX_SENSOR_LIST    8

static sensor_t g_sensor_list[MAX_SENSOR_LIST];
static int g_sensor_count = 0;

/*****************************************************************************/

struct sensors_poll_context_t {
    struct sensors_poll_device_1 device;  // must be first

    sensors_poll_context_t();
    ~sensors_poll_context_t();

    int activate(int handle, int enabled);
    int setDelay(int handle, int64_t ns);
    int pollEvents(sensors_event_t *data, int count);
    int batch(int handle, int flags, int64_t sample_ns, int64_t latency_ns);
    int flush(int handle);

private:
    enum {
        accel = 0,
        proximity,
#ifdef LIGHT_SENSOR
        light,
#endif
#ifdef GRIP_SENSOR
        grip,
#endif
        meta,
        numSensors
    };

    SensorBase *mSensors[numSensors];
    struct pollfd mPollFds[numSensors + 1];  // +1 for wake pipe
    int mWritePipeFd;

    int handleToDriver(int handle) const;
};

/*****************************************************************************/

int sensors_poll_context_t::handleToDriver(int handle) const
{
    switch (handle) {
    case 0:  // HANDLE_ACCELEROMETER
        return accel;
    case HANDLE_PROXIMITY:
        return proximity;
#ifdef LIGHT_SENSOR
    case HANDLE_LIGHT:
        return light;
#endif
#ifdef GRIP_SENSOR
    case HANDLE_GRIP:
        return grip;
#endif
    default:
        return -1;
    }
}

sensors_poll_context_t::sensors_poll_context_t()
{
    g_sensor_count = 0;

    // Create sensor drivers
    mSensors[accel] = new AccelerometerSensor();
    mSensors[proximity] = new ProximitySensor();
#ifdef LIGHT_SENSOR
    mSensors[light] = new LightSensor();
#endif
#ifdef GRIP_SENSOR
    mSensors[grip] = new GripSensor();
#endif
    mSensors[meta] = new MetaEvent();

    // Build sensor list from each driver
    for (int i = 0; i < numSensors; i++) {
        g_sensor_count = mSensors[i]->addSensorList(g_sensor_list, g_sensor_count);
    }

    ALOGI("sensors_poll_context_t: sensor list: %d", g_sensor_count);

    // Setup pollfds for each sensor
    for (int i = 0; i < numSensors; i++) {
        mPollFds[i].fd = mSensors[i]->getFd();
        mPollFds[i].events = POLLIN;
        mPollFds[i].revents = 0;
    }

    // Wake pipe
    int wakeFds[2];
    int result = pipe(wakeFds);
    ALOGE_IF(result < 0, "error creating wake pipe (%s)", strerror(errno));
    fcntl(wakeFds[0], F_SETFL, O_NONBLOCK);
    fcntl(wakeFds[1], F_SETFL, O_NONBLOCK);

    mPollFds[numSensors].fd = wakeFds[0];
    mPollFds[numSensors].events = POLLIN;
    mPollFds[numSensors].revents = 0;
    mWritePipeFd = wakeFds[1];
}

sensors_poll_context_t::~sensors_poll_context_t()
{
    for (int i = 0; i < numSensors; i++) {
        delete mSensors[i];
    }
    close(mPollFds[numSensors].fd);
    close(mWritePipeFd);
}

int sensors_poll_context_t::activate(int handle, int enabled)
{
    int drv = handleToDriver(handle);
    if (drv < 0)
        return -EINVAL;

    int err = mSensors[drv]->enable(handle, enabled);

    if (enabled && !err) {
        const char wakeMessage = 'W';
        int result = write(mWritePipeFd, &wakeMessage, 1);
        ALOGE_IF(result < 0, "error sending wake message (%s)", strerror(errno));
    }

    return err;
}

int sensors_poll_context_t::setDelay(int handle, int64_t ns)
{
    int drv = handleToDriver(handle);
    if (drv < 0)
        return -EINVAL;

    return mSensors[drv]->setDelay(handle, ns);
}

int sensors_poll_context_t::batch(int handle, int flags, int64_t sample_ns, int64_t latency_ns)
{
    int drv = handleToDriver(handle);
    if (drv < 0)
        return -EINVAL;

    return mSensors[drv]->batch(handle, flags, sample_ns, latency_ns);
}

int sensors_poll_context_t::flush(int handle)
{
    int drv = handleToDriver(handle);
    if (drv < 0)
        return -EINVAL;

    return mSensors[drv]->flush(handle);
}

int sensors_poll_context_t::pollEvents(sensors_event_t *data, int count)
{
    int nbEvents = 0;
    int n = 0;

    do {
        for (int i = 0; count && i < numSensors; i++) {
            if ((mPollFds[i].revents & POLLIN) || mSensors[i]->hasPendingEvents()) {
                int nb = mSensors[i]->readEvents(data, count);
                if (nb < 0) {
                    ALOGE("%s, index(%d) readEvents failed nb = %d", "pollEvents", i, nb);
                    return nb;
                }
                if (nb > 0) {
                    data += nb;
                    count -= nb;
                    nbEvents += nb;
                    mPollFds[i].revents = 0;
                }
            }
        }

        if (count) {
            do {
                n = poll(mPollFds, numSensors + 1, nbEvents ? 0 : -1);
            } while (n < 0 && errno == EINTR);

            if (n < 0) {
                ALOGE("poll() failed (%s)", strerror(errno));
                return -errno;
            }

            if (mPollFds[numSensors].revents & POLLIN) {
                char msg;
                int result = read(mPollFds[numSensors].fd, &msg, 1);
                ALOGE_IF(result < 0, "error reading from wake pipe (%s)", strerror(errno));
                ALOGE_IF(msg != 'W', "unknown message on wake queue (0x%02x)", int(msg));
                mPollFds[numSensors].revents = 0;
            }
        }
    } while (n && count);

    return nbEvents;
}

/*****************************************************************************/
// HAL module interface

static int poll__close(struct hw_device_t *dev)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    if (ctx)
        delete ctx;
    return 0;
}

static int poll__activate(struct sensors_poll_device_t *dev, int handle, int enabled)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->activate(handle, enabled);
}

static int poll__setDelay(struct sensors_poll_device_t *dev, int handle, int64_t ns)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->setDelay(handle, ns);
}

static int poll__poll(struct sensors_poll_device_t *dev, sensors_event_t *data, int count)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->pollEvents(data, count);
}

static int poll__batch(struct sensors_poll_device_1 *dev, int handle, int flags,
                       int64_t sample_ns, int64_t latency_ns)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->batch(handle, flags, sample_ns, latency_ns);
}

static int poll__flush(struct sensors_poll_device_1 *dev, int handle)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->flush(handle);
}

static int sensors__get_sensors_list(struct sensors_module_t *, struct sensor_t const **list)
{
    *list = g_sensor_list;
    ALOGI("Get sensor list: %d", g_sensor_count);
    return g_sensor_count;
}

static int open_sensors(const struct hw_module_t *module, const char *,
                        struct hw_device_t **device)
{
    sensors_poll_context_t *dev = new sensors_poll_context_t();

    memset(&dev->device, 0, sizeof(sensors_poll_device_1));

    dev->device.common.tag = HARDWARE_DEVICE_TAG;
    dev->device.common.version = SENSORS_DEVICE_API_VERSION_1_3;
    dev->device.common.module = const_cast<hw_module_t *>(module);
    dev->device.common.close = poll__close;
    dev->device.activate = poll__activate;
    dev->device.setDelay = poll__setDelay;
    dev->device.poll = poll__poll;
    dev->device.batch = poll__batch;
    dev->device.flush = poll__flush;

    *device = &dev->device.common;
    return 0;
}

static struct hw_module_methods_t sensors_module_methods = {
    .open = open_sensors,
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = SENSORS_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = SENSORS_HARDWARE_MODULE_ID,
        .name = "Samsung MSM8916 Sensors Module",
        .author = "prototype74",
        .methods = &sensors_module_methods,
        .dso = NULL,
        .reserved = {0},
    },
    .get_sensors_list = sensors__get_sensors_list,
};
