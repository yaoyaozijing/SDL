/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#ifdef SDL_JOYSTICK_HIDAPI

#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"

#ifdef SDL_JOYSTICK_HIDAPI_BEITONG

// Define this if you want to log all packets from the controller
#if 0
#define DEBUG_BEITONG_PROTOCOL
#endif

#ifndef DEG2RAD
#define DEG2RAD(x) ((float)(x) * (float)(SDL_PI_F / 180.0f))
#endif

#define BEITONG_COLLECTION_BUTTONS   4
#define BEITONG_COLLECTION_IMU       5

#define BEITONG_IMU_RATE_HZ          500.0f

typedef struct
{
    bool sensors_enabled;
    bool last_state_initialized;
    Uint8 last_state[USB_PACKET_LENGTH];
    SDL_hid_device *imu_dev;
    Uint64 sensor_timestamp_ns;
    float accel_scale;
    float gyro_scale;
} SDL_DriverBeitong_Context;

static void HIDAPI_DriverBeitong_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_BEITONG, callback, userdata);
}

static void HIDAPI_DriverBeitong_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_BEITONG, callback, userdata);
}

static bool HIDAPI_DriverBeitong_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_BEITONG, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static bool HIDAPI_DriverBeitong_PathHasCollection(const char *path, int collection)
{
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)
    char col_marker[8];

    if (!path) {
        return false;
    }
    SDL_snprintf(col_marker, sizeof(col_marker), "col%02d", collection);
    return (SDL_strcasestr(path, col_marker) != NULL);
#else
    (void)path;
    (void)collection;
    return false;
#endif
}

static bool HIDAPI_DriverBeitong_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    if (vendor_id != USB_VENDOR_BEITONG || product_id != USB_PRODUCT_BEITONG_ZEUS2) {
        return false;
    }

    if (!device) {
        return true;
    }

    /* On Windows this controller exposes separate HID collections.
     * Bind only Col04 as primary and read Col05 via secondary handle.
     */
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)
    return HIDAPI_DriverBeitong_PathHasCollection(device->path, BEITONG_COLLECTION_BUTTONS);
#else
    return true;
#endif
}

static bool HIDAPI_DriverBeitong_MatchCollectionPath(const char *a, const char *b)
{
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)
    const char *a_col = SDL_strcasestr(a, "&col");
    const char *b_col = SDL_strcasestr(b, "&col");
    if (a_col && b_col) {
        size_t a_prefix = (size_t)(a_col - a);
        size_t b_prefix = (size_t)(b_col - b);
        if (a_prefix == b_prefix && SDL_strncasecmp(a, b, a_prefix) == 0) {
            return true;
        }
    }
#endif
    return (SDL_strcmp(a, b) == 0);
}

static SDL_hid_device *HIDAPI_DriverBeitong_OpenIMUHandle(SDL_HIDAPI_Device *device)
{
    SDL_hid_device *imu_dev = NULL;
    struct SDL_hid_device_info *devs;

    if (!device->path) {
        return NULL;
    }
    if (!HIDAPI_DriverBeitong_PathHasCollection(device->path, BEITONG_COLLECTION_BUTTONS)) {
        return NULL;
    }

    devs = SDL_hid_enumerate(device->vendor_id, device->product_id);
    for (struct SDL_hid_device_info *info = devs; info; info = info->next) {
        if (!info->path) {
            continue;
        }
        if (!HIDAPI_DriverBeitong_PathHasCollection(info->path, BEITONG_COLLECTION_IMU)) {
            continue;
        }
        if (HIDAPI_DriverBeitong_MatchCollectionPath(device->path, info->path)) {
            imu_dev = SDL_hid_open_path(info->path);
            if (imu_dev) {
                break;
            }
        }
    }
    SDL_hid_free_enumeration(devs);

    return imu_dev;
}

static bool HIDAPI_DriverBeitong_InitDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverBeitong_Context *ctx = (SDL_DriverBeitong_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return false;
    }

    device->context = ctx;

    ctx->accel_scale = 2.0f * SDL_STANDARD_GRAVITY / 32768.0f;
    ctx->gyro_scale = DEG2RAD(1.0f / 16.0f);
    ctx->imu_dev = HIDAPI_DriverBeitong_OpenIMUHandle(device);

    HIDAPI_SetDeviceName(device, "Beitong Zeus 2");
    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverBeitong_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverBeitong_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static bool HIDAPI_DriverBeitong_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverBeitong_Context *ctx = (SDL_DriverBeitong_Context *)device->context;
    int i;

    SDL_AssertJoysticksLocked();

    SDL_zeroa(ctx->last_state);
    ctx->last_state_initialized = false;
    ctx->sensor_timestamp_ns = SDL_GetTicksNS();

    joystick->nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;
    joystick->nhats = 1;

    SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, BEITONG_IMU_RATE_HZ);
    SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, BEITONG_IMU_RATE_HZ);

    /* Keep sensor stream active by default so test tools can read data immediately. */
    ctx->sensors_enabled = true;
    joystick->nsensors_enabled = joystick->nsensors;
    for (i = 0; i < joystick->nsensors; ++i) {
        joystick->sensors[i].enabled = true;
    }

    return true;
}

static bool HIDAPI_DriverBeitong_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverBeitong_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverBeitong_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    return 0;
}

static bool HIDAPI_DriverBeitong_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverBeitong_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverBeitong_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    SDL_DriverBeitong_Context *ctx = (SDL_DriverBeitong_Context *)device->context;

    ctx->sensors_enabled = enabled;
    if (enabled) {
        ctx->sensor_timestamp_ns = SDL_GetTicksNS();
    }
    return true;
}

static Uint8 HIDAPI_DriverBeitong_GetHat(Uint8 dpad_byte)
{
    const bool up = ((dpad_byte & 0x01) != 0);
    const bool down = ((dpad_byte & 0x02) != 0);
    const bool left = ((dpad_byte & 0x04) != 0);
    const bool right = ((dpad_byte & 0x08) != 0);

    if (up && !down) {
        if (right && !left) {
            return SDL_HAT_RIGHTUP;
        } else if (left && !right) {
            return SDL_HAT_LEFTUP;
        }
        return SDL_HAT_UP;
    }

    if (down && !up) {
        if (right && !left) {
            return SDL_HAT_RIGHTDOWN;
        } else if (left && !right) {
            return SDL_HAT_LEFTDOWN;
        }
        return SDL_HAT_DOWN;
    }

    if (left && !right) {
        return SDL_HAT_LEFT;
    } else if (right && !left) {
        return SDL_HAT_RIGHT;
    }

    return SDL_HAT_CENTERED;
}

static void HIDAPI_DriverBeitong_HandleButtonPacket(SDL_Joystick *joystick, SDL_DriverBeitong_Context *ctx, const Uint8 *data, int size)
{
    Uint64 timestamp = SDL_GetTicksNS();
    const bool initial = !ctx->last_state_initialized;
    const Uint8 *last = ctx->last_state;
    Sint16 axis;

    if (size < 11) {
        return;
    }

    if (initial || last[8] != data[8]) {
        SDL_SendJoystickHat(timestamp, joystick, 0, HIDAPI_DriverBeitong_GetHat(data[8]));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, ((data[8] & 0x10) != 0));      // menu
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, ((data[8] & 0x20) != 0));       // view
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK, ((data[8] & 0x40) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_STICK, ((data[8] & 0x80) != 0));
    }

    if (initial || last[9] != data[9]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, ((data[9] & 0x01) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, ((data[9] & 0x02) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_SOUTH, ((data[9] & 0x10) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_EAST, ((data[9] & 0x20) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_WEST, ((data[9] & 0x40) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_NORTH, ((data[9] & 0x80) != 0));
    }

    if (initial || last[10] != data[10]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, ((data[10] & 0x40) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC2, ((data[10] & 0x20) != 0));     // left small shoulder
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC3, ((data[10] & 0x10) != 0));     // right small shoulder
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, ((data[10] & 0x04) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, ((data[10] & 0x08) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, ((data[10] & 0x01) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, ((data[10] & 0x02) != 0));
    }

#define READ_STICK_AXIS(offset) \
    (data[offset] == 0x80 ? 0 : (Sint16)HIDAPI_RemapVal((float)((int)data[offset] - 0x80), -0x80, 0x7f, SDL_MIN_SINT16, SDL_MAX_SINT16))

    if (initial || last[2] != data[2]) {
        axis = READ_STICK_AXIS(2);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, axis);
    }
    if (initial || last[3] != data[3]) {
        axis = -READ_STICK_AXIS(3);
        if (axis <= SDL_MIN_SINT16) {
            axis = SDL_MAX_SINT16;
        }
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTY, axis);
    }
    if (initial || last[4] != data[4]) {
        axis = READ_STICK_AXIS(4);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, axis);
    }
    if (initial || last[5] != data[5]) {
        axis = -READ_STICK_AXIS(5);
        if (axis <= SDL_MIN_SINT16) {
            axis = SDL_MAX_SINT16;
        }
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, axis);
    }
#undef READ_STICK_AXIS

#define READ_TRIGGER_AXIS(offset) \
    (Sint16)HIDAPI_RemapVal((float)data[offset], 0.0f, 255.0f, (float)SDL_MIN_SINT16, (float)SDL_MAX_SINT16)

    if (initial || last[6] != data[6]) {
        axis = READ_TRIGGER_AXIS(6);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, axis);
    }
    if (initial || last[7] != data[7]) {
        axis = READ_TRIGGER_AXIS(7);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, axis);
    }
#undef READ_TRIGGER_AXIS

    SDL_memcpy(ctx->last_state, data, SDL_min(size, (int)sizeof(ctx->last_state)));
    ctx->last_state_initialized = true;
}

static void HIDAPI_DriverBeitong_HandleIMUPacket(SDL_Joystick *joystick, SDL_DriverBeitong_Context *ctx, const Uint8 *data, int size)
{
    float values[3];
    Uint64 timestamp;
    Uint64 sensor_timestamp;
    Sint16 accel_x, accel_y, accel_z;
    Sint16 gyro_x, gyro_y, gyro_z;

    if (!ctx->sensors_enabled) {
        return;
    }

    /* Strictly follow Beitong reference:
     * byte[0] is report header, accel at byte[1..6], gyro at byte[7..12].
     */
    if (size < 13) {
        return;
    }

    timestamp = SDL_GetTicksNS();
    sensor_timestamp = ctx->sensor_timestamp_ns;
    if (!sensor_timestamp) {
        sensor_timestamp = timestamp;
    }
    ctx->sensor_timestamp_ns = timestamp;

    gyro_x = LOAD16(data[1], data[2]);
    gyro_y = LOAD16(data[3], data[4]);
    gyro_z = LOAD16(data[5], data[6]);
    accel_x = LOAD16(data[7], data[8]);
    accel_y = LOAD16(data[9], data[10]);
    accel_z = LOAD16(data[11], data[12]);

    values[0] = (float)accel_x * ctx->accel_scale;
    values[1] = (float)accel_y * ctx->accel_scale;
    values[2] = (float)accel_z * ctx->accel_scale;
    SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, sensor_timestamp, values, 3);

    values[0] = (float)gyro_x * ctx->gyro_scale;
    values[1] = (float)gyro_y * ctx->gyro_scale;
    values[2] = (float)gyro_z * ctx->gyro_scale;
    SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, sensor_timestamp, values, 3);
}

static bool HIDAPI_DriverBeitong_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverBeitong_Context *ctx = (SDL_DriverBeitong_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size = 0;
    int imu_size = 0;

    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromID(device->joysticks[0]);
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_BEITONG_PROTOCOL
        HIDAPI_DumpPacket("Beitong packet: size = %d", data, size);
#endif
        if (!joystick) {
            continue;
        }
        HIDAPI_DriverBeitong_HandleButtonPacket(joystick, ctx, data, size);
    }

    if (ctx->imu_dev && joystick) {
        while ((imu_size = SDL_hid_read_timeout(ctx->imu_dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_BEITONG_PROTOCOL
            HIDAPI_DumpPacket("Beitong IMU packet: size = %d", data, imu_size);
#endif
            HIDAPI_DriverBeitong_HandleIMUPacket(joystick, ctx, data, imu_size);
        }
        if (imu_size < 0) {
            SDL_hid_close(ctx->imu_dev);
            ctx->imu_dev = NULL;
        }
    }

    if (size < 0 && device->num_joysticks > 0) {
        // Read error, device is disconnected
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return (size >= 0);
}

static void HIDAPI_DriverBeitong_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverBeitong_FreeDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverBeitong_Context *ctx = (SDL_DriverBeitong_Context *)device->context;
    if (ctx) {
        if (ctx->imu_dev) {
            SDL_hid_close(ctx->imu_dev);
            ctx->imu_dev = NULL;
        }
        SDL_free(ctx);
        device->context = NULL;
    }
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverBeitong = {
    SDL_HINT_JOYSTICK_HIDAPI_BEITONG,
    true,
    HIDAPI_DriverBeitong_RegisterHints,
    HIDAPI_DriverBeitong_UnregisterHints,
    HIDAPI_DriverBeitong_IsEnabled,
    HIDAPI_DriverBeitong_IsSupportedDevice,
    HIDAPI_DriverBeitong_InitDevice,
    HIDAPI_DriverBeitong_GetDevicePlayerIndex,
    HIDAPI_DriverBeitong_SetDevicePlayerIndex,
    HIDAPI_DriverBeitong_UpdateDevice,
    HIDAPI_DriverBeitong_OpenJoystick,
    HIDAPI_DriverBeitong_RumbleJoystick,
    HIDAPI_DriverBeitong_RumbleJoystickTriggers,
    HIDAPI_DriverBeitong_GetJoystickCapabilities,
    HIDAPI_DriverBeitong_SetJoystickLED,
    HIDAPI_DriverBeitong_SendJoystickEffect,
    HIDAPI_DriverBeitong_SetJoystickSensorsEnabled,
    HIDAPI_DriverBeitong_CloseJoystick,
    HIDAPI_DriverBeitong_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_BEITONG

#endif // SDL_JOYSTICK_HIDAPI
