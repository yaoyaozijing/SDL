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
#include "SDL_hidapi_rumble.h"
#include "SDL_hidapi_simple_profile.h"

#ifdef SDL_JOYSTICK_HIDAPI_SIMPLE_PROFILE

// Define this if you want to log all packets from the controller
#if 0
#define DEBUG_SIMPLE_PROFILE_PROTOCOL
#endif

#ifndef SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE
#define SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE "SDL_JOYSTICK_HIDAPI_SIMPLE_PROFILE"
#endif

typedef struct
{
    const SDL_HIDAPI_SimpleDeviceProfile *profile;
    SDL_hid_device *sensor_input_handle;
    bool sensors_enabled;
    Uint64 sensor_timestamp_ns;
    Uint8 sensor_report_counter;
    bool sensor_report_counter_initialized;
    bool last_state_initialized;
    Uint8 last_state[USB_PACKET_LENGTH];
} SDL_DriverSimpleProfile_Context;

static bool HIDAPI_Simple_ProfileMatchesVIDPID(const SDL_HIDAPI_SimpleDeviceProfile *profile, Uint16 vendor_id, Uint16 product_id)
{
    if (!profile) {
        return false;
    }

    return (profile->vendor_id == vendor_id && profile->product_id == product_id);
}

const SDL_HIDAPI_SimpleDeviceProfile *HIDAPI_Simple_GetDeviceProfile(Uint16 vendor_id, Uint16 product_id)
{
    int i;

    for (i = 0; i < (int)SDL_arraysize(SDL_hidapi_simple_profiles); ++i) {
        const SDL_HIDAPI_SimpleDeviceProfile *profile = &SDL_hidapi_simple_profiles[i];
        if (HIDAPI_Simple_ProfileMatchesVIDPID(profile, vendor_id, product_id)) {
            return profile;
        }
    }

    return NULL;
}

bool HIDAPI_Simple_IsSupportedVIDPID(Uint16 vendor_id, Uint16 product_id)
{
    return (HIDAPI_Simple_GetDeviceProfile(vendor_id, product_id) != NULL);
}

const char *HIDAPI_Simple_GetMappingStringSuffix(Uint16 vendor_id, Uint16 product_id)
{
    const SDL_HIDAPI_SimpleDeviceProfile *profile = HIDAPI_Simple_GetDeviceProfile(vendor_id, product_id);
    if (!profile) {
        return NULL;
    }
    return profile->mapping_string_suffix;
}

static void HIDAPI_DriverSimpleProfile_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE, callback, userdata);
}

static void HIDAPI_DriverSimpleProfile_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE, callback, userdata);
}

static bool HIDAPI_DriverSimpleProfile_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static bool HIDAPI_DriverSimpleProfile_PathHasCollection(const char *path, int collection)
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

static bool HIDAPI_DriverSimpleProfile_MatchCollectionPath(const char *a, const char *b)
{
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)
    const char *a_col;
    const char *b_col;

    if (!a || !b) {
        return false;
    }

    a_col = SDL_strcasestr(a, "&col");
    b_col = SDL_strcasestr(b, "&col");
    if (a_col && b_col) {
        size_t a_prefix = (size_t)(a_col - a);
        size_t b_prefix = (size_t)(b_col - b);
        if (a_prefix == b_prefix && SDL_strncasecmp(a, b, a_prefix) == 0) {
            return true;
        }
    }
#endif
    return (a && b && SDL_strcmp(a, b) == 0);
}

static SDL_hid_device *HIDAPI_DriverSimpleProfile_OpenCollectionHandle(SDL_HIDAPI_Device *device, int collection)
{
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)
    SDL_hid_device *handle = NULL;
    struct SDL_hid_device_info *devs;
    struct SDL_hid_device_info *info;

    if (!device || !device->path || collection <= 0) {
        return NULL;
    }

    devs = SDL_hid_enumerate(device->vendor_id, device->product_id);
    for (info = devs; info; info = info->next) {
        if (!info->path) {
            continue;
        }
        if (!HIDAPI_DriverSimpleProfile_PathHasCollection(info->path, collection)) {
            continue;
        }
        if (!HIDAPI_DriverSimpleProfile_MatchCollectionPath(device->path, info->path)) {
            continue;
        }
        if (SDL_strcmp(info->path, device->path) == 0) {
            continue;
        }
        handle = SDL_hid_open_path(info->path);
        if (handle) {
            break;
        }
    }

    SDL_hid_free_enumeration(devs);
    return handle;
#else
    (void)device;
    (void)collection;
    return NULL;
#endif
}

static bool HIDAPI_DriverSimpleProfile_IsExpectedCollection(SDL_HIDAPI_Device *device, const SDL_HIDAPI_SimpleDeviceProfile *profile)
{
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)
    if (!profile || profile->collection <= 0) {
        return true;
    }
    return HIDAPI_DriverSimpleProfile_PathHasCollection(device->path, profile->collection);
#else
    (void)device;
    (void)profile;
    return true;
#endif
}

static bool HIDAPI_DriverSimpleProfile_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    const SDL_HIDAPI_SimpleDeviceProfile *profile = HIDAPI_Simple_GetDeviceProfile(vendor_id, product_id);

    if (!profile) {
        return false;
    }

    if (!device) {
        return true;
    }

    return HIDAPI_DriverSimpleProfile_IsExpectedCollection(device, profile);
}

static bool HIDAPI_DriverSimpleProfile_InitDevice(SDL_HIDAPI_Device *device)
{
    const SDL_HIDAPI_SimpleDeviceProfile *profile = HIDAPI_Simple_GetDeviceProfile(device->vendor_id, device->product_id);
    const SDL_HIDAPI_SimpleSensorBinding *sensors = profile ? profile->sensors : NULL;
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)SDL_calloc(1, sizeof(*ctx));

    if (!profile) {
        return false;
    }
    if (!ctx) {
        return false;
    }

    ctx->profile = profile;
    device->context = ctx;

    if (sensors && sensors->collection > 0) {
        ctx->sensor_input_handle = HIDAPI_DriverSimpleProfile_OpenCollectionHandle(device, sensors->collection);
        if (ctx->sensor_input_handle) {
            SDL_hid_set_nonblocking(ctx->sensor_input_handle, 1);
        }
    }

    HIDAPI_SetDeviceName(device, profile->name ? profile->name : "Generic HID Controller");
    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverSimpleProfile_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverSimpleProfile_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static bool HIDAPI_DriverSimpleProfile_IsTouchpadAxisConfigValid(Uint8 high_byte_index, Uint8 high_byte_mask, Uint8 low_byte_index)
{
    if (low_byte_index == SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE) {
        return false;
    }
    if (high_byte_index != SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE && high_byte_mask == 0) {
        return false;
    }
    return true;
}

static bool HIDAPI_DriverSimpleProfile_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    const SDL_HIDAPI_SimpleReportLayout *layout = ctx->profile ? ctx->profile->layout : NULL;
    const SDL_HIDAPI_SimpleTouchpadBinding *touchpads = ctx->profile ? ctx->profile->touchpads : NULL;
    const int num_touchpads = ctx->profile ? ctx->profile->num_touchpads : 0;
    const SDL_HIDAPI_SimpleSensorBinding *sensors = ctx->profile ? ctx->profile->sensors : NULL;
    int i;

    SDL_AssertJoysticksLocked();

    SDL_zeroa(ctx->last_state);
    ctx->last_state_initialized = false;

    joystick->nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;
    joystick->nhats = (layout &&
                       (layout->dpad.up_mask || layout->dpad.down_mask || layout->dpad.left_mask || layout->dpad.right_mask)) ? 1 : 0;

    if (touchpads && num_touchpads > 0) {
        for (i = 0; i < num_touchpads; ++i) {
            if (HIDAPI_DriverSimpleProfile_IsTouchpadAxisConfigValid(touchpads[i].x_high_byte_index, touchpads[i].x_high_byte_mask, touchpads[i].x_low_byte_index) &&
                HIDAPI_DriverSimpleProfile_IsTouchpadAxisConfigValid(touchpads[i].y_high_byte_index, touchpads[i].y_high_byte_mask, touchpads[i].y_low_byte_index) &&
                touchpads[i].pressure_value <= 1 &&
                (touchpads[i].pressure_mask == 0 || touchpads[i].pressure_byte_index != SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE) &&
                touchpads[i].x_resolution > 0 &&
                touchpads[i].y_resolution > 0) {
                SDL_PrivateJoystickAddTouchpad(joystick, 1);
            }
        }
    }

    if (sensors) {
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, sensors->report_rate_hz);
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, sensors->report_rate_hz);

        /* Keep sensor stream active by default so test tools can read data immediately. */
        ctx->sensors_enabled = true;
        ctx->sensor_timestamp_ns = SDL_GetTicksNS();
        ctx->sensor_report_counter = 0;
        ctx->sensor_report_counter_initialized = false;
        joystick->nsensors_enabled = joystick->nsensors;
        for (i = 0; i < joystick->nsensors; ++i) {
            joystick->sensors[i].enabled = true;
        }
    }

    return true;
}

static bool HIDAPI_DriverSimpleProfile_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    const SDL_HIDAPI_SimpleRumbleBinding *rumble = (ctx && ctx->profile) ? ctx->profile->rumble : NULL;
    Uint8 rumble_packet[2 * USB_PACKET_LENGTH];
    int rumble_packet_size;

    if (!rumble || !rumble->packet_data || rumble->packet_size == 0) {
        return SDL_Unsupported();
    }

    rumble_packet_size = rumble->packet_size;
    if (rumble_packet_size > (int)sizeof(rumble_packet) ||
        rumble->low_frequency_byte_index >= rumble_packet_size ||
        rumble->high_frequency_byte_index >= rumble_packet_size) {
        return SDL_Unsupported();
    }

    SDL_memcpy(rumble_packet, rumble->packet_data, rumble_packet_size);
    rumble_packet[rumble->low_frequency_byte_index] = (Uint8)(low_frequency_rumble >> 8);
    rumble_packet[rumble->high_frequency_byte_index] = (Uint8)(high_frequency_rumble >> 8);

    if (SDL_HIDAPI_SendRumble(device, rumble_packet, rumble_packet_size) != rumble_packet_size) {
        return SDL_SetError("Couldn't send rumble packet");
    }
    return true;
}

static bool HIDAPI_DriverSimpleProfile_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    const SDL_HIDAPI_SimpleTriggerRumbleBinding *trigger_rumble = (ctx && ctx->profile) ? ctx->profile->trigger_rumble : NULL;
    Uint8 rumble_packet[2 * USB_PACKET_LENGTH];
    int rumble_packet_size;

    if (!trigger_rumble || !trigger_rumble->packet_data || trigger_rumble->packet_size == 0) {
        return SDL_Unsupported();
    }

    rumble_packet_size = trigger_rumble->packet_size;
    if (rumble_packet_size > (int)sizeof(rumble_packet) ||
        trigger_rumble->left_trigger_byte_index >= rumble_packet_size ||
        trigger_rumble->right_trigger_byte_index >= rumble_packet_size) {
        return SDL_Unsupported();
    }

    SDL_memcpy(rumble_packet, trigger_rumble->packet_data, rumble_packet_size);
    rumble_packet[trigger_rumble->left_trigger_byte_index] = (Uint8)(left_rumble >> 8);
    rumble_packet[trigger_rumble->right_trigger_byte_index] = (Uint8)(right_rumble >> 8);

    if (SDL_HIDAPI_SendRumble(device, rumble_packet, rumble_packet_size) != rumble_packet_size) {
        return SDL_SetError("Couldn't send trigger rumble packet");
    }
    return true;
}

static Uint32 HIDAPI_DriverSimpleProfile_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    const SDL_HIDAPI_SimpleRumbleBinding *rumble = (ctx && ctx->profile) ? ctx->profile->rumble : NULL;
    const SDL_HIDAPI_SimpleTriggerRumbleBinding *trigger_rumble = (ctx && ctx->profile) ? ctx->profile->trigger_rumble : NULL;
    Uint32 caps = 0;

    if (rumble && rumble->packet_data && rumble->packet_size > 0) {
        caps |= SDL_JOYSTICK_CAP_RUMBLE;
    }
    if (trigger_rumble && trigger_rumble->packet_data && trigger_rumble->packet_size > 0) {
        caps |= SDL_JOYSTICK_CAP_TRIGGER_RUMBLE;
    }
    return caps;
}

static bool HIDAPI_DriverSimpleProfile_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSimpleProfile_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSimpleProfile_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    const SDL_HIDAPI_SimpleSensorBinding *sensors = (ctx && ctx->profile) ? ctx->profile->sensors : NULL;

    if (!sensors) {
        return SDL_Unsupported();
    }

    ctx->sensors_enabled = enabled;
    if (enabled) {
        ctx->sensor_timestamp_ns = SDL_GetTicksNS();
        ctx->sensor_report_counter = 0;
        ctx->sensor_report_counter_initialized = false;
    }
    return true;
}

static Uint8 HIDAPI_DriverSimpleProfile_GetHat(const SDL_HIDAPI_SimpleDPadBinding *dpad, const Uint8 *data, int size)
{
    Uint8 dpad_byte;
    bool up, down, left, right;

    if (!dpad || dpad->byte_index >= size) {
        return SDL_HAT_CENTERED;
    }

    dpad_byte = data[dpad->byte_index];
    up = ((dpad_byte & dpad->up_mask) != 0);
    down = ((dpad_byte & dpad->down_mask) != 0);
    left = ((dpad_byte & dpad->left_mask) != 0);
    right = ((dpad_byte & dpad->right_mask) != 0);

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

static Sint16 HIDAPI_DriverSimpleProfile_DecodeAxis(const SDL_HIDAPI_SimpleAxisBinding *binding, Uint8 raw_value)
{
    Sint16 value = 0;

    switch (binding->encoding) {
    case SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80:
        value = (raw_value == 0x80) ? 0 : (Sint16)HIDAPI_RemapVal((float)((int)raw_value - 0x80), -0x80, 0x7f, SDL_MIN_SINT16, SDL_MAX_SINT16);
        break;
    case SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255:
        value = (Sint16)HIDAPI_RemapVal((float)raw_value, 0.0f, 255.0f, (float)SDL_MIN_SINT16, (float)SDL_MAX_SINT16);
        break;
    default:
        break;
    }

    if (binding->invert) {
        value = (value == SDL_MIN_SINT16) ? SDL_MAX_SINT16 : (Sint16)-value;
    }
    return value;
}

static bool HIDAPI_DriverSimpleProfile_IsTouchpadAxisBytesValid(Uint8 high_byte_index, Uint8 low_byte_index, int size)
{
    if (low_byte_index >= size) {
        return false;
    }
    if (high_byte_index != SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE &&
        high_byte_index >= size) {
        return false;
    }
    return true;
}

static bool HIDAPI_DriverSimpleProfile_IsTouchpadAxisChanged(const Uint8 *last, const Uint8 *data, Uint8 high_byte_index, Uint8 high_byte_mask, Uint8 low_byte_index)
{
    if (last[low_byte_index] != data[low_byte_index]) {
        return true;
    }
    if (high_byte_index != SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE &&
        ((last[high_byte_index] & high_byte_mask) != (data[high_byte_index] & high_byte_mask))) {
        return true;
    }
    return false;
}

static bool HIDAPI_DriverSimpleProfile_IsTouchpadChanged(const Uint8 *last, const Uint8 *data, bool initial, Uint8 pressure_byte_index, Uint8 pressure_mask, const SDL_HIDAPI_SimpleTouchpadBinding *touchpad)
{
    if (initial) {
        return true;
    }
    if (pressure_mask != 0 &&
        ((last[pressure_byte_index] & pressure_mask) != (data[pressure_byte_index] & pressure_mask))) {
        return true;
    }
    if (HIDAPI_DriverSimpleProfile_IsTouchpadAxisChanged(last, data, touchpad->x_high_byte_index, touchpad->x_high_byte_mask, touchpad->x_low_byte_index)) {
        return true;
    }
    return HIDAPI_DriverSimpleProfile_IsTouchpadAxisChanged(last, data, touchpad->y_high_byte_index, touchpad->y_high_byte_mask, touchpad->y_low_byte_index);
}

static float HIDAPI_DriverSimpleProfile_NormalizeTouchpadCoordinate(Uint16 value, Uint16 resolution)
{
    float normalized;

    if (resolution == 0) {
        return 0.0f;
    }

    normalized = (float)value / (float)resolution;
    return SDL_clamp(normalized, 0.0f, 1.0f);
}

static bool HIDAPI_DriverSimpleProfile_DecodeTouchpadAxis(const Uint8 *data, int size, Uint8 high_byte_index, Uint8 high_byte_mask, Uint8 low_byte_index, Uint16 *out_value)
{
    Uint16 value;

    if (!out_value || !HIDAPI_DriverSimpleProfile_IsTouchpadAxisBytesValid(high_byte_index, low_byte_index, size)) {
        return false;
    }

    value = data[low_byte_index];
    if (high_byte_index != SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE) {
        value = (Uint16)((((Uint16)(data[high_byte_index] & high_byte_mask)) << 8) | data[low_byte_index]);
    }

    *out_value = value;
    return true;
}

static void HIDAPI_DriverSimpleProfile_HandleTouchpadPacket(SDL_Joystick *joystick, SDL_DriverSimpleProfile_Context *ctx, const Uint8 *data, int size, Uint64 timestamp, const Uint8 *last, bool initial)
{
    const SDL_HIDAPI_SimpleTouchpadBinding *touchpads = ctx->profile ? ctx->profile->touchpads : NULL;
    const int num_touchpads = ctx->profile ? ctx->profile->num_touchpads : 0;
    int touchpad_index;

    if (!touchpads || num_touchpads <= 0 || joystick->ntouchpads <= 0) {
        return;
    }

    for (touchpad_index = 0; touchpad_index < num_touchpads; ++touchpad_index) {
        const SDL_HIDAPI_SimpleTouchpadBinding *touchpad = &touchpads[touchpad_index];
        Uint8 pressure_byte_index = touchpad->pressure_byte_index;
        Uint8 pressure_mask = touchpad->pressure_mask;
        Uint8 pressure_value = touchpad->pressure_value;
        Uint8 x_high_byte_index = touchpad->x_high_byte_index;
        Uint8 x_high_byte_mask = touchpad->x_high_byte_mask;
        Uint8 x_low_byte_index = touchpad->x_low_byte_index;
        Uint8 y_high_byte_index = touchpad->y_high_byte_index;
        Uint8 y_high_byte_mask = touchpad->y_high_byte_mask;
        Uint8 y_low_byte_index = touchpad->y_low_byte_index;
        Uint16 x_resolution = touchpad->x_resolution;
        Uint16 y_resolution = touchpad->y_resolution;
        bool touch_active;
        bool previous_touch_active;
        bool pressure_on;
        float normalized_x;
        float normalized_y;
        float pressure;
        Uint16 x;
        Uint16 y;
        Uint16 previous_x = 0;
        Uint16 previous_y = 0;

        if (!HIDAPI_DriverSimpleProfile_IsTouchpadAxisConfigValid(x_high_byte_index, x_high_byte_mask, x_low_byte_index) ||
            !HIDAPI_DriverSimpleProfile_IsTouchpadAxisConfigValid(y_high_byte_index, y_high_byte_mask, y_low_byte_index) ||
            pressure_value > 1 ||
            (pressure_mask != 0 && pressure_byte_index == SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE) ||
            x_resolution == 0 || y_resolution == 0) {
            continue;
        }

        if ((pressure_mask != 0 && pressure_byte_index >= size) ||
            !HIDAPI_DriverSimpleProfile_IsTouchpadAxisBytesValid(x_high_byte_index, x_low_byte_index, size) ||
            !HIDAPI_DriverSimpleProfile_IsTouchpadAxisBytesValid(y_high_byte_index, y_low_byte_index, size)) {
            continue;
        }
        if (!HIDAPI_DriverSimpleProfile_IsTouchpadChanged(last, data, initial, pressure_byte_index, pressure_mask, touchpad)) {
            continue;
        }
        if (!HIDAPI_DriverSimpleProfile_DecodeTouchpadAxis(data, size, x_high_byte_index, x_high_byte_mask, x_low_byte_index, &x) ||
            !HIDAPI_DriverSimpleProfile_DecodeTouchpadAxis(data, size, y_high_byte_index, y_high_byte_mask, y_low_byte_index, &y)) {
            continue;
        }

        touch_active = (x != 0 || y != 0);
        if (pressure_mask != 0) {
            pressure_on = (((data[pressure_byte_index] & pressure_mask) != 0) == (pressure_value != 0));
        } else {
            /* Device has no pressure capability: keep active touch at full normalized pressure. */
            pressure_on = true;
        }
        pressure = pressure_on ? 1.0f : 0.0f;
        normalized_x = HIDAPI_DriverSimpleProfile_NormalizeTouchpadCoordinate(x, x_resolution);
        normalized_y = HIDAPI_DriverSimpleProfile_NormalizeTouchpadCoordinate(y, y_resolution);

        previous_touch_active = false;
        if (!initial &&
            HIDAPI_DriverSimpleProfile_DecodeTouchpadAxis(last, size, x_high_byte_index, x_high_byte_mask, x_low_byte_index, &previous_x) &&
            HIDAPI_DriverSimpleProfile_DecodeTouchpadAxis(last, size, y_high_byte_index, y_high_byte_mask, y_low_byte_index, &previous_y)) {
            previous_touch_active = (previous_x != 0 || previous_y != 0);
        }

        if (!touch_active) {
            if (previous_touch_active) {
                /* XY transition from non-zero to zero means finger lifted. */
                SDL_SendJoystickTouchpad(timestamp, joystick, touchpad_index, 0, false,
                                         0.0f, 0.0f, 0.0f);
            }
            continue;
        }

        SDL_SendJoystickTouchpad(timestamp, joystick, touchpad_index, 0, true,
                                 normalized_x, normalized_y, pressure);
    }
}

static void HIDAPI_DriverSimpleProfile_HandleBatteryPacket(SDL_Joystick *joystick, SDL_DriverSimpleProfile_Context *ctx, const Uint8 *data, int size, const Uint8 *last, bool initial)
{
    const SDL_HIDAPI_SimpleBatteryBinding *battery = ctx->profile ? ctx->profile->battery : NULL;
    Uint8 byte_index;
    int percent;

    if (!battery) {
        return;
    }

    byte_index = battery->byte_index;
    if (byte_index >= size) {
        return;
    }
    if (!initial && last[byte_index] == data[byte_index]) {
        return;
    }

    percent = SDL_clamp((int)data[byte_index], 0, 100);
    SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_ON_BATTERY, percent);
}

static void HIDAPI_DriverSimpleProfile_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverSimpleProfile_Context *ctx, const Uint8 *data, int size)
{
    const SDL_HIDAPI_SimpleReportLayout *layout = ctx->profile ? ctx->profile->layout : NULL;
    Uint64 timestamp = SDL_GetTicksNS();
    const bool initial = !ctx->last_state_initialized;
    const Uint8 *last = ctx->last_state;
    int i;

    if (!layout) {
        return;
    }

    if (joystick->nhats > 0 &&
        layout->dpad.byte_index < size &&
        (initial || last[layout->dpad.byte_index] != data[layout->dpad.byte_index])) {
        SDL_SendJoystickHat(timestamp, joystick, 0, HIDAPI_DriverSimpleProfile_GetHat(&layout->dpad, data, size));
    }

    for (i = 0; i < layout->num_buttons; ++i) {
        const SDL_HIDAPI_SimpleButtonBinding *binding = &layout->buttons[i];
        Uint8 byte_index = binding->byte_index;

        if (byte_index >= size) {
            continue;
        }
        if (initial || last[byte_index] != data[byte_index]) {
            SDL_SendJoystickButton(timestamp, joystick, binding->button, ((data[byte_index] & binding->mask) != 0));
        }
    }

    for (i = 0; i < layout->num_axes; ++i) {
        const SDL_HIDAPI_SimpleAxisBinding *binding = &layout->axes[i];
        Uint8 byte_index = binding->byte_index;
        bool changed;
        Uint8 raw_value;

        if (byte_index >= size) {
            continue;
        }
        changed = (initial || last[byte_index] != data[byte_index]);
        raw_value = data[byte_index];

        if (changed) {
            Sint16 axis = HIDAPI_DriverSimpleProfile_DecodeAxis(binding, raw_value);
            SDL_SendJoystickAxis(timestamp, joystick, binding->axis, axis);
        }
    }

    HIDAPI_DriverSimpleProfile_HandleTouchpadPacket(joystick, ctx, data, size, timestamp, last, initial);
    HIDAPI_DriverSimpleProfile_HandleBatteryPacket(joystick, ctx, data, size, last, initial);

    SDL_zeroa(ctx->last_state);
    SDL_memcpy(ctx->last_state, data, SDL_min(size, (int)sizeof(ctx->last_state)));
    ctx->last_state_initialized = true;
}

static void HIDAPI_DriverSimpleProfile_HandleSensorPacket(SDL_Joystick *joystick, SDL_DriverSimpleProfile_Context *ctx, const Uint8 *data, int size)
{
    const SDL_HIDAPI_SimpleSensorBinding *sensors = ctx->profile ? ctx->profile->sensors : NULL;
    Uint64 timestamp;
    Uint64 sensor_timestamp;
    float sample_interval_ns = 0.0f;
    bool has_report_counter = false;
    Uint8 report_counter = 0;
    float values[3];
    Sint16 accel_x, accel_y, accel_z;
    Sint16 gyro_x, gyro_y, gyro_z;

    if (!sensors || !ctx->sensors_enabled) {
        return;
    }
    if (sensors->gyro_x_byte_index == SDL_HIDAPI_SIMPLE_PROFILE_SENSOR_BYTE_NONE ||
        sensors->gyro_y_byte_index == SDL_HIDAPI_SIMPLE_PROFILE_SENSOR_BYTE_NONE ||
        sensors->gyro_z_byte_index == SDL_HIDAPI_SIMPLE_PROFILE_SENSOR_BYTE_NONE ||
        sensors->accel_x_byte_index == SDL_HIDAPI_SIMPLE_PROFILE_SENSOR_BYTE_NONE ||
        sensors->accel_y_byte_index == SDL_HIDAPI_SIMPLE_PROFILE_SENSOR_BYTE_NONE ||
        sensors->accel_z_byte_index == SDL_HIDAPI_SIMPLE_PROFILE_SENSOR_BYTE_NONE) {
        return;
    }
    if ((sensors->gyro_x_byte_index + 1) >= size || (sensors->gyro_y_byte_index + 1) >= size || (sensors->gyro_z_byte_index + 1) >= size ||
        (sensors->accel_x_byte_index + 1) >= size || (sensors->accel_y_byte_index + 1) >= size || (sensors->accel_z_byte_index + 1) >= size) {
        return;
    }
    if (sensors->report_rate_hz > 0.0f) {
        sample_interval_ns = 1000000000.0f / sensors->report_rate_hz;
    }
    if (sensors->timestamp_byte_index != SDL_HIDAPI_SIMPLE_PROFILE_SENSOR_BYTE_NONE &&
        sensors->timestamp_byte_index < size &&
        sensors->report_rate_hz > 0.0f) {
        has_report_counter = true;
        report_counter = data[sensors->timestamp_byte_index];
    }

    timestamp = SDL_GetTicksNS();
    if (has_report_counter) {
        sensor_timestamp = ctx->sensor_timestamp_ns;
        if (!sensor_timestamp) {
            sensor_timestamp = timestamp;
        }
        if (ctx->sensor_report_counter_initialized) {
            Uint8 delta = (Uint8)(report_counter - ctx->sensor_report_counter);
            if (delta > 0) {
                sensor_timestamp += (Uint64)((float)delta * sample_interval_ns);
            }
        }
        ctx->sensor_report_counter = report_counter;
        ctx->sensor_report_counter_initialized = true;
        ctx->sensor_timestamp_ns = sensor_timestamp;
    } else {
        sensor_timestamp = ctx->sensor_timestamp_ns;
        if (!sensor_timestamp) {
            sensor_timestamp = timestamp;
        }
        if (sample_interval_ns > 0.0f) {
            // Simulate the controller sensor clock using the known report cadence.
            ctx->sensor_timestamp_ns = sensor_timestamp + (Uint64)sample_interval_ns;
        } else {
            // Fallback for profiles without a valid report rate.
            ctx->sensor_timestamp_ns = timestamp;
        }
    }

    gyro_x = LOAD16(data[sensors->gyro_x_byte_index + 0], data[sensors->gyro_x_byte_index + 1]);
    gyro_y = LOAD16(data[sensors->gyro_y_byte_index + 0], data[sensors->gyro_y_byte_index + 1]);
    gyro_z = LOAD16(data[sensors->gyro_z_byte_index + 0], data[sensors->gyro_z_byte_index + 1]);
    accel_x = LOAD16(data[sensors->accel_x_byte_index + 0], data[sensors->accel_x_byte_index + 1]);
    accel_y = LOAD16(data[sensors->accel_y_byte_index + 0], data[sensors->accel_y_byte_index + 1]);
    accel_z = LOAD16(data[sensors->accel_z_byte_index + 0], data[sensors->accel_z_byte_index + 1]);

    values[0] = (float)accel_x * sensors->accel_scale;
    values[1] = (float)accel_y * sensors->accel_scale;
    values[2] = (float)accel_z * sensors->accel_scale;
    SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, sensor_timestamp, values, 3);

    values[0] = (float)gyro_x * sensors->gyro_scale;
    values[1] = (float)gyro_y * sensors->gyro_scale;
    values[2] = (float)gyro_z * sensors->gyro_scale;
    SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, sensor_timestamp, values, 3);
}

static bool HIDAPI_DriverSimpleProfile_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    const SDL_HIDAPI_SimpleSensorBinding *sensors = (ctx && ctx->profile) ? ctx->profile->sensors : NULL;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size = 0;
    int sensor_size = 0;

    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromID(device->joysticks[0]);
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_SIMPLE_PROFILE_PROTOCOL
        HIDAPI_DumpPacket("Simple profile packet: size = %d", data, size);
#endif
        if (!joystick) {
            continue;
        }
        HIDAPI_DriverSimpleProfile_HandleStatePacket(joystick, ctx, data, size);
        if (sensors && sensors->collection <= 0) {
            HIDAPI_DriverSimpleProfile_HandleSensorPacket(joystick, ctx, data, size);
        }
    }

    if (ctx->sensor_input_handle && joystick) {
        while ((sensor_size = SDL_hid_read_timeout(ctx->sensor_input_handle, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_SIMPLE_PROFILE_PROTOCOL
            HIDAPI_DumpPacket("Simple profile IMU packet: size = %d", data, sensor_size);
#endif
            HIDAPI_DriverSimpleProfile_HandleSensorPacket(joystick, ctx, data, sensor_size);
        }
        if (sensor_size < 0) {
            SDL_hid_close(ctx->sensor_input_handle);
            ctx->sensor_input_handle = NULL;
        }
    }

    if (size < 0 && device->num_joysticks > 0) {
        // Read error, device is disconnected
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return (size >= 0);
}

static void HIDAPI_DriverSimpleProfile_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverSimpleProfile_FreeDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    if (ctx) {
        if (ctx->sensor_input_handle) {
            SDL_hid_close(ctx->sensor_input_handle);
            ctx->sensor_input_handle = NULL;
        }
        SDL_free(ctx);
        device->context = NULL;
    }
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSimpleProfile = {
    SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE,
    true,
    HIDAPI_DriverSimpleProfile_RegisterHints,
    HIDAPI_DriverSimpleProfile_UnregisterHints,
    HIDAPI_DriverSimpleProfile_IsEnabled,
    HIDAPI_DriverSimpleProfile_IsSupportedDevice,
    HIDAPI_DriverSimpleProfile_InitDevice,
    HIDAPI_DriverSimpleProfile_GetDevicePlayerIndex,
    HIDAPI_DriverSimpleProfile_SetDevicePlayerIndex,
    HIDAPI_DriverSimpleProfile_UpdateDevice,
    HIDAPI_DriverSimpleProfile_OpenJoystick,
    HIDAPI_DriverSimpleProfile_RumbleJoystick,
    HIDAPI_DriverSimpleProfile_RumbleJoystickTriggers,
    HIDAPI_DriverSimpleProfile_GetJoystickCapabilities,
    HIDAPI_DriverSimpleProfile_SetJoystickLED,
    HIDAPI_DriverSimpleProfile_SendJoystickEffect,
    HIDAPI_DriverSimpleProfile_SetJoystickSensorsEnabled,
    HIDAPI_DriverSimpleProfile_CloseJoystick,
    HIDAPI_DriverSimpleProfile_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_SIMPLE_PROFILE

#endif // SDL_JOYSTICK_HIDAPI
