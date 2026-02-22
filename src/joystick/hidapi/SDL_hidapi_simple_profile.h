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

#ifndef SDL_hidapi_simple_profile_h_
#define SDL_hidapi_simple_profile_h_

/*
 * Table-driven HID adaptation framework.
 *
 * Add a new controller by:
 * 1) Defining one SDL_HIDAPI_SimpleReportLayout
 * 2) Adding one SDL_HIDAPI_SimpleDeviceProfile entry to SDL_hidapi_simple_profiles
 */

typedef enum
{
    SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80,
    SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255,
} SDL_HIDAPI_SimpleAxisEncoding;

typedef struct
{
    SDL_GamepadButton button;
    Uint8 byte_index;
    Uint8 mask;
} SDL_HIDAPI_SimpleButtonBinding;

typedef struct
{
    Uint8 byte_index;
    Uint8 up_mask;
    Uint8 down_mask;
    Uint8 left_mask;
    Uint8 right_mask;
} SDL_HIDAPI_SimpleDPadBinding;

typedef struct
{
    SDL_GamepadAxis axis;
    Uint8 byte_index;
    SDL_HIDAPI_SimpleAxisEncoding encoding;
    bool invert;
} SDL_HIDAPI_SimpleAxisBinding;

typedef struct
{
    Uint8 report_size_min;
    SDL_HIDAPI_SimpleDPadBinding dpad;
    const SDL_HIDAPI_SimpleButtonBinding *buttons;
    int num_buttons;
    const SDL_HIDAPI_SimpleAxisBinding *axes;
    int num_axes;
} SDL_HIDAPI_SimpleReportLayout;

typedef struct
{
    Uint16 vendor_id;
    Uint16 product_id;
    bool allow_swapped_vid_pid;
    int collection;
    const char *name;
    const char *mapping_string_suffix;
    const SDL_HIDAPI_SimpleReportLayout *layout;
} SDL_HIDAPI_SimpleDeviceProfile;

/*
 * Layout v1 ZDS,ZDO+
 * Byte 3: face buttons, shoulders, left stick click
 * Byte 4: dpad, menu/view, right stick click
 * Byte 5: guide/screenshot/small shoulders/back buttons
 * Byte 6-11: analog axes (LX, LY, RX, RY, LT, RT)
 */
static const SDL_HIDAPI_SimpleButtonBinding SDL_hidapi_zhidong_layout_v1_buttons[] = {
    { SDL_GAMEPAD_BUTTON_BACK,           4, 0x04 },
    { SDL_GAMEPAD_BUTTON_START,          4, 0x08 },
    { SDL_GAMEPAD_BUTTON_RIGHT_STICK,    4, 0x02 },

    { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  3, 0x10 },
    { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 3, 0x80 },
    { SDL_GAMEPAD_BUTTON_LEFT_STICK,     3, 0x40 },
    { SDL_GAMEPAD_BUTTON_SOUTH,          3, 0x02 },
    { SDL_GAMEPAD_BUTTON_EAST,           3, 0x01 },
    { SDL_GAMEPAD_BUTTON_WEST,           3, 0x08 },
    { SDL_GAMEPAD_BUTTON_NORTH,          3, 0x04 },

    { SDL_GAMEPAD_BUTTON_GUIDE,          5, 0x10 },
    { SDL_GAMEPAD_BUTTON_MISC1,          5, 0x20 },
    { SDL_GAMEPAD_BUTTON_MISC2,          5, 0x04 },
    { SDL_GAMEPAD_BUTTON_MISC3,          5, 0x08 },
    { SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,  5, 0x02 },
    { SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,   5, 0x01 },
};

static const SDL_HIDAPI_SimpleAxisBinding SDL_hidapi_zhidong_layout_v1_axes[] = {
    { SDL_GAMEPAD_AXIS_LEFTX,         6, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_LEFTY,         7, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_RIGHTX,        8, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_RIGHTY,        9, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_LEFT_TRIGGER,  10, SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255, false },
    { SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 11, SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255, false },
};

static const SDL_HIDAPI_SimpleReportLayout SDL_hidapi_zhidong_layout_v1 = {
    12,
    { 4, 0x40, 0x80, 0x10, 0x20 },
    SDL_hidapi_zhidong_layout_v1_buttons,
    (int)SDL_arraysize(SDL_hidapi_zhidong_layout_v1_buttons),
    SDL_hidapi_zhidong_layout_v1_axes,
    (int)SDL_arraysize(SDL_hidapi_zhidong_layout_v1_axes),
};

/*
 * Controller profiles.
 * The first 4 entries intentionally keep the existing device display name.
 */
static const SDL_HIDAPI_SimpleDeviceProfile SDL_hidapi_simple_profiles[] = {
    { USB_VENDOR_ZHIDONG_USB_XINPUT, USB_PRODUCT_ZHIDONG_USB_XINPUT, true, 2, "Zhidong Controller", "misc1:b15,paddle1:b16,paddle2:b17,misc2:b21,misc3:b22,", &SDL_hidapi_zhidong_layout_v1 },
    { USB_VENDOR_ZHIDONG_USB_DINPUT, USB_PRODUCT_ZHIDONG_USB_DINPUT, true, 2, "Zhidong Controller", "misc1:b15,paddle1:b16,paddle2:b17,misc2:b21,misc3:b22,", &SDL_hidapi_zhidong_layout_v1 },
    { USB_VENDOR_ZHIDONG_24G, USB_PRODUCT_ZHIDONG_24G_XINPUT, true, 2, "Zhidong Controller", "misc1:b15,paddle1:b16,paddle2:b17,misc2:b21,misc3:b22,", &SDL_hidapi_zhidong_layout_v1 },
    { USB_VENDOR_ZHIDONG_24G, USB_PRODUCT_ZHIDONG_24G_DINPUT, true, 2, "Zhidong Controller", "misc1:b15,paddle1:b16,paddle2:b17,misc2:b21,misc3:b22,", &SDL_hidapi_zhidong_layout_v1 },
};

static inline bool HIDAPI_Simple_ProfileMatchesVIDPID(const SDL_HIDAPI_SimpleDeviceProfile *profile, Uint16 vendor_id, Uint16 product_id)
{
    if (!profile) {
        return false;
    }

    if (profile->vendor_id == vendor_id && profile->product_id == product_id) {
        return true;
    }

    if (profile->allow_swapped_vid_pid &&
        profile->vendor_id == product_id && profile->product_id == vendor_id) {
        return true;
    }

    return false;
}

static inline const SDL_HIDAPI_SimpleDeviceProfile *HIDAPI_Simple_GetDeviceProfile(Uint16 vendor_id, Uint16 product_id)
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

static inline bool HIDAPI_Simple_IsSupportedVIDPID(Uint16 vendor_id, Uint16 product_id)
{
    return (HIDAPI_Simple_GetDeviceProfile(vendor_id, product_id) != NULL);
}

static inline const char *HIDAPI_Simple_GetMappingStringSuffix(Uint16 vendor_id, Uint16 product_id)
{
    const SDL_HIDAPI_SimpleDeviceProfile *profile = HIDAPI_Simple_GetDeviceProfile(vendor_id, product_id);
    if (!profile) {
        return NULL;
    }
    return profile->mapping_string_suffix;
}

#endif // SDL_hidapi_simple_profile_h_
