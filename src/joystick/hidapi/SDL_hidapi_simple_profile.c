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
    SDL_hid_device *rumble_output_handle;
    bool last_state_initialized;
    Uint8 last_state[USB_PACKET_LENGTH];
} SDL_DriverSimpleProfile_Context;

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

static SDL_hid_device *HIDAPI_DriverSimpleProfile_OpenGamepadUsageHandle(SDL_HIDAPI_Device *device)
{
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)
    const Uint16 USAGE_PAGE_GENERIC_DESKTOP = 0x0001;
    const Uint16 USAGE_GENERIC_GAMEPAD = 0x0005;
    SDL_hid_device *handle = NULL;
    struct SDL_hid_device_info *devs;
    struct SDL_hid_device_info *info;

    if (!device || !device->path) {
        return NULL;
    }

    devs = SDL_hid_enumerate(device->vendor_id, device->product_id);
    for (info = devs; info; info = info->next) {
        if (!info->path) {
            continue;
        }
        if (info->usage_page != USAGE_PAGE_GENERIC_DESKTOP || info->usage != USAGE_GENERIC_GAMEPAD) {
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

    // Fallback: pick any gamepad usage interface with the same VID/PID.
    if (!handle) {
        for (info = devs; info; info = info->next) {
            if (!info->path) {
                continue;
            }
            if (info->usage_page != USAGE_PAGE_GENERIC_DESKTOP || info->usage != USAGE_GENERIC_GAMEPAD) {
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
    }

    SDL_hid_free_enumeration(devs);
    return handle;
#else
    (void)device;
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
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)SDL_calloc(1, sizeof(*ctx));

    if (!profile) {
        return false;
    }
    if (!ctx) {
        return false;
    }

    ctx->profile = profile;
    device->context = ctx;

    if (profile->rumble &&
        (device->usage_page != 0x0001 || device->usage != 0x0005)) {
        ctx->rumble_output_handle = HIDAPI_DriverSimpleProfile_OpenGamepadUsageHandle(device);
        if (ctx->rumble_output_handle) {
            SDL_hid_set_nonblocking(ctx->rumble_output_handle, 1);
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

static bool HIDAPI_DriverSimpleProfile_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    const SDL_HIDAPI_SimpleReportLayout *layout = ctx->profile ? ctx->profile->layout : NULL;

    SDL_AssertJoysticksLocked();

    SDL_zeroa(ctx->last_state);
    ctx->last_state_initialized = false;

    joystick->nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;
    joystick->nhats = (layout &&
                       (layout->dpad.up_mask || layout->dpad.down_mask || layout->dpad.left_mask || layout->dpad.right_mask)) ? 1 : 0;

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
        (rumble->set_command_byte && rumble->command_byte_index >= rumble_packet_size) ||
        rumble->low_frequency_byte_index >= rumble_packet_size ||
        rumble->high_frequency_byte_index >= rumble_packet_size) {
        return SDL_Unsupported();
    }

    SDL_memcpy(rumble_packet, rumble->packet_data, rumble_packet_size);
    if (rumble->set_command_byte) {
        rumble_packet[rumble->command_byte_index] = rumble->command_byte_value;
    }
    rumble_packet[rumble->low_frequency_byte_index] = (Uint8)(low_frequency_rumble >> 8);
    rumble_packet[rumble->high_frequency_byte_index] = (Uint8)(high_frequency_rumble >> 8);

    if (ctx && ctx->rumble_output_handle) {
        if (SDL_hid_write(ctx->rumble_output_handle, rumble_packet, rumble_packet_size) != rumble_packet_size) {
            return SDL_SetError("Couldn't send rumble packet");
        }
    } else {
        if (SDL_HIDAPI_SendRumble(device, rumble_packet, rumble_packet_size) != rumble_packet_size) {
            return SDL_SetError("Couldn't send rumble packet");
        }
    }
    return true;
}

static bool HIDAPI_DriverSimpleProfile_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverSimpleProfile_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    const SDL_HIDAPI_SimpleRumbleBinding *rumble = (ctx && ctx->profile) ? ctx->profile->rumble : NULL;

    if (rumble && rumble->packet_data && rumble->packet_size > 0 &&
        (!rumble->set_command_byte || rumble->command_byte_index < rumble->packet_size) &&
        rumble->low_frequency_byte_index < rumble->packet_size &&
        rumble->high_frequency_byte_index < rumble->packet_size) {
        return SDL_JOYSTICK_CAP_RUMBLE;
    }
    return 0;
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
    return SDL_Unsupported();
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

static void HIDAPI_DriverSimpleProfile_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverSimpleProfile_Context *ctx, const Uint8 *data, int size)
{
    const SDL_HIDAPI_SimpleReportLayout *layout = ctx->profile ? ctx->profile->layout : NULL;
    Uint64 timestamp = SDL_GetTicksNS();
    const bool initial = !ctx->last_state_initialized;
    const Uint8 *last = ctx->last_state;
    int i;

    if (!layout || size < layout->report_size_min) {
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

        if (byte_index >= size) {
            continue;
        }
        if (initial || last[byte_index] != data[byte_index]) {
            Sint16 axis = HIDAPI_DriverSimpleProfile_DecodeAxis(binding, data[byte_index]);
            SDL_SendJoystickAxis(timestamp, joystick, binding->axis, axis);
        }
    }

    SDL_zeroa(ctx->last_state);
    SDL_memcpy(ctx->last_state, data, SDL_min(size, (int)sizeof(ctx->last_state)));
    ctx->last_state_initialized = true;
}

static bool HIDAPI_DriverSimpleProfile_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size = 0;

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
        if (ctx->rumble_output_handle) {
            SDL_hid_close(ctx->rumble_output_handle);
            ctx->rumble_output_handle = NULL;
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
