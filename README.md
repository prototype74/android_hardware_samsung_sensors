# Samsung MSM8916 Sensors HAL

A custom-built Sensors HAL (API 1.3) for Samsung MSM8916 devices, created through reverse engineering of the stock Sensors HAL binary and verification against the kernel source.
Samsung's stock sensors HAL for MSM8916 devices uses API version 1.0, which is incompatible with Android 8.0+ (requires API 1.3 minimum). This from-scratch reimplementation:

- Maintains the same Samsung-specific architecture (direct input events, Samsung sysfs paths)
- Implements the required HAL 1.3 interface (`batch`, `flush` support)
- Dynamically detects sensor hardware via chip name matching

### Reverse Engineering Notes

The stock Sensors HAL was analyzed using Ghidra to understand:
- Sensor discovery mechanism (Samsung's `/sys/class/sensor_event/symlink/` path)
- Input event parsing (event types, codes, scale factors)
- Flush/meta event handling via dedicated `meta_event` input device
- sysfs control paths for enable/disable and poll delay

All findings were cross-referenced with the kernel driver source (J510FNXXS3BTI6) to verify correctness.

## Supported Sensors

| Sensor | Chip | Input Device | Event Type |
|--------|------|-------------|------------|
| Accelerometer | K2HH (STM) | `accelerometer_sensor` | EV_REL (X/Y/Z) |
| Proximity | CM36672P, GP2A, STK3013, CM36686 | `proximity_sensor` | EV_ABS (ABS_DISTANCE) |
| Light | CM36686 (Capella) | `light_sensor` | EV_REL (REL_DIAL) |
| Grip | SX9310 (SEMTECH) | `grip_sensor` | EV_REL (REL_MISC) |

## Architecture

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                 sensors.cpp (HAL entry, poll loop, API 1.3)                  │
├──────────────────────────────────────────────────────────────────────────────┤
│ AccelerometerSensor │ ProximitySensor │ LightSensor │ GripSensor │ MetaEvent │
├──────────────────────────────────────────────────────────────────────────────┤
│                SensorBase (input device discovery, sysfs I/O)                │
├──────────────────────────────────────────────────────────────────────────────┤
│                InputEventReader (ring buffer for input_event)                │
└──────────────────────────────────────────────────────────────────────────────┘
                  │                          │
                  ▼                          ▼
         /sys/class/sensor_event/symlink/   /dev/input/eventX
         /sys/class/sensors/*/name          /sys/class/sensors/sensor_dev/flush
```

## Setup

### 1. Add to local manifest

Create or edit `.repo/local_manifests/sensors.xml`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
    <project path="hardware/samsung/sensors"
             name="prototype74/android_hardware_samsung_sensors"
             revision="android-8.0-msm8916" />
</manifest>
```

Then sync:

```
repo sync hardware/samsung/sensors
```

### 2. Add to device makefile

In `device/samsung/<variant>/device.mk`:

```makefile
PRODUCT_PACKAGES += sensors.msm8916
```

## Adding New Devices

To support a new Samsung MSM8916 device:

1. Check which sensor chips are present:
   ```bash
   cat /sys/class/sensors/*/name
   ```

2. If the chip is already in the supported list, it works out of the box.

3. For a new chip, add a `static const sensor_t` entry in the corresponding sensor class and add the chip name check in `addSensorList()`.

## License

Apache License 2.0
