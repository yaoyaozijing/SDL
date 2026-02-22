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

#ifdef SDL_JOYSTICK_HIDAPI_ZHIDONG

// Define this if you want to log all packets from the controller
#if 0
#define DEBUG_ZHIDONG_PROTOCOL
#endif

#ifndef SDL_HINT_JOYSTICK_HIDAPI_ZHIDONG
#define SDL_HINT_JOYSTICK_HIDAPI_ZHIDONG "SDL_JOYSTICK_HIDAPI_ZHIDONG"
#endif

#define ZHIDONG_COLLECTION_BUTTONS 2

typedef struct
{
    bool last_state_initialized;
    Uint8 last_state[USB_PACKET_LENGTH];
} SDL_DriverZhidong_Context;

static bool HIDAPI_DriverZhidong_IsSupportedVIDPID(Uint16 vendor_id, Uint16 product_id)
{
    if ((vendor_id == USB_VENDOR_ZHIDONG_USB_XINPUT && product_id == USB_PRODUCT_ZHIDONG_USB_XINPUT) ||
        (vendor_id == USB_VENDOR_ZHIDONG_USB_DINPUT && product_id == USB_PRODUCT_ZHIDONG_USB_DINPUT) ||
        (vendor_id == USB_VENDOR_ZHIDONG_24G && product_id == USB_PRODUCT_ZHIDONG_24G_XINPUT) ||
        (vendor_id == USB_VENDOR_ZHIDONG_24G && product_id == USB_PRODUCT_ZHIDONG_24G_DINPUT)) {
        return true;
    }

    /* Some packet docs list PID/VID in reverse column order; accept both to avoid false negatives. */
    if ((vendor_id == USB_PRODUCT_ZHIDONG_USB_XINPUT && product_id == USB_VENDOR_ZHIDONG_USB_XINPUT) ||
        (vendor_id == USB_PRODUCT_ZHIDONG_USB_DINPUT && product_id == USB_VENDOR_ZHIDONG_USB_DINPUT) ||
        (vendor_id == USB_PRODUCT_ZHIDONG_24G_XINPUT && product_id == USB_VENDOR_ZHIDONG_24G) ||
        (vendor_id == USB_PRODUCT_ZHIDONG_24G_DINPUT && product_id == USB_VENDOR_ZHIDONG_24G)) {
        return true;
    }

    return false;
}

static void HIDAPI_DriverZhidong_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_ZHIDONG, callback, userdata);
}

static void HIDAPI_DriverZhidong_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_ZHIDONG, callback, userdata);
}

static bool HIDAPI_DriverZhidong_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_ZHIDONG, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static bool HIDAPI_DriverZhidong_PathHasCollection(const char *path, int collection)
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

static bool HIDAPI_DriverZhidong_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    if (!HIDAPI_DriverZhidong_IsSupportedVIDPID(vendor_id, product_id)) {
        return false;
    }

    if (!device) {
        return true;
    }

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)
    return HIDAPI_DriverZhidong_PathHasCollection(device->path, ZHIDONG_COLLECTION_BUTTONS);
#else
    return true;
#endif
}

static bool HIDAPI_DriverZhidong_InitDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverZhidong_Context *ctx = (SDL_DriverZhidong_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return false;
    }

    device->context = ctx;

    HIDAPI_SetDeviceName(device, "Zhidong Controller");
    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverZhidong_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverZhidong_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static bool HIDAPI_DriverZhidong_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverZhidong_Context *ctx = (SDL_DriverZhidong_Context *)device->context;

    SDL_AssertJoysticksLocked();

    SDL_zeroa(ctx->last_state);
    ctx->last_state_initialized = false;

    joystick->nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;
    joystick->nhats = 1;

    return true;
}

static bool HIDAPI_DriverZhidong_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverZhidong_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverZhidong_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    return 0;
}

static bool HIDAPI_DriverZhidong_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverZhidong_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverZhidong_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    return SDL_Unsupported();
}

static Uint8 HIDAPI_DriverZhidong_GetHat(Uint8 dpad_byte)
{
    const bool up = ((dpad_byte & 0x40) != 0);
    const bool down = ((dpad_byte & 0x80) != 0);
    const bool left = ((dpad_byte & 0x10) != 0);
    const bool right = ((dpad_byte & 0x20) != 0);

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

static void HIDAPI_DriverZhidong_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverZhidong_Context *ctx, const Uint8 *data, int size)
{
    Uint64 timestamp = SDL_GetTicksNS();
    const bool initial = !ctx->last_state_initialized;
    const Uint8 *last = ctx->last_state;
    Sint16 axis;

    if (size < 12) {
        return;
    }

    if (initial || last[4] != data[4]) {
        SDL_SendJoystickHat(timestamp, joystick, 0, HIDAPI_DriverZhidong_GetHat(data[4]));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, ((data[4] & 0x04) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, ((data[4] & 0x08) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_STICK, ((data[4] & 0x02) != 0));
    }

    if (initial || last[3] != data[3]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, ((data[3] & 0x10) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, ((data[3] & 0x80) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK, ((data[3] & 0x40) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_SOUTH, ((data[3] & 0x02) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_EAST, ((data[3] & 0x01) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_WEST, ((data[3] & 0x08) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_NORTH, ((data[3] & 0x04) != 0));
    }

    if (initial || last[5] != data[5]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, ((data[5] & 0x10) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC1, ((data[5] & 0x20) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC2, ((data[5] & 0x04) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC3, ((data[5] & 0x08) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, ((data[5] & 0x02) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, ((data[5] & 0x01) != 0));
    }

#define READ_STICK_AXIS(offset) \
    (data[offset] == 0x80 ? 0 : (Sint16)HIDAPI_RemapVal((float)((int)data[offset] - 0x80), -0x80, 0x7f, SDL_MIN_SINT16, SDL_MAX_SINT16))

    if (initial || last[6] != data[6]) {
        axis = READ_STICK_AXIS(6);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, axis);
    }
    if (initial || last[7] != data[7]) {
        axis = READ_STICK_AXIS(7);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTY, axis);
    }
    if (initial || last[8] != data[8]) {
        axis = READ_STICK_AXIS(8);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, axis);
    }
    if (initial || last[9] != data[9]) {
        axis = READ_STICK_AXIS(9);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, axis);
    }
#undef READ_STICK_AXIS

#define READ_TRIGGER_AXIS(offset) \
    (Sint16)HIDAPI_RemapVal((float)data[offset], 0.0f, 255.0f, (float)SDL_MIN_SINT16, (float)SDL_MAX_SINT16)

    if (initial || last[10] != data[10]) {
        axis = READ_TRIGGER_AXIS(10);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, axis);
    }
    if (initial || last[11] != data[11]) {
        axis = READ_TRIGGER_AXIS(11);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, axis);
    }
#undef READ_TRIGGER_AXIS

    SDL_memcpy(ctx->last_state, data, SDL_min(size, (int)sizeof(ctx->last_state)));
    ctx->last_state_initialized = true;
}

static bool HIDAPI_DriverZhidong_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverZhidong_Context *ctx = (SDL_DriverZhidong_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size = 0;

    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromID(device->joysticks[0]);
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_ZHIDONG_PROTOCOL
        HIDAPI_DumpPacket("Zhidong packet: size = %d", data, size);
#endif
        if (!joystick) {
            continue;
        }
        HIDAPI_DriverZhidong_HandleStatePacket(joystick, ctx, data, size);
    }

    if (size < 0 && device->num_joysticks > 0) {
        // Read error, device is disconnected
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return (size >= 0);
}

static void HIDAPI_DriverZhidong_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverZhidong_FreeDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverZhidong_Context *ctx = (SDL_DriverZhidong_Context *)device->context;
    if (ctx) {
        SDL_free(ctx);
        device->context = NULL;
    }
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverZhidong = {
    SDL_HINT_JOYSTICK_HIDAPI_ZHIDONG,
    true,
    HIDAPI_DriverZhidong_RegisterHints,
    HIDAPI_DriverZhidong_UnregisterHints,
    HIDAPI_DriverZhidong_IsEnabled,
    HIDAPI_DriverZhidong_IsSupportedDevice,
    HIDAPI_DriverZhidong_InitDevice,
    HIDAPI_DriverZhidong_GetDevicePlayerIndex,
    HIDAPI_DriverZhidong_SetDevicePlayerIndex,
    HIDAPI_DriverZhidong_UpdateDevice,
    HIDAPI_DriverZhidong_OpenJoystick,
    HIDAPI_DriverZhidong_RumbleJoystick,
    HIDAPI_DriverZhidong_RumbleJoystickTriggers,
    HIDAPI_DriverZhidong_GetJoystickCapabilities,
    HIDAPI_DriverZhidong_SetJoystickLED,
    HIDAPI_DriverZhidong_SendJoystickEffect,
    HIDAPI_DriverZhidong_SetJoystickSensorsEnabled,
    HIDAPI_DriverZhidong_CloseJoystick,
    HIDAPI_DriverZhidong_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_ZHIDONG

#endif // SDL_JOYSTICK_HIDAPI
