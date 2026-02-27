#ifndef SDL_hidapi_simple_profile_controller_zhidong_h_
#define SDL_hidapi_simple_profile_controller_zhidong_h_

#define ZHIDONG_MAPPING_SUFFIX "misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,misc2:b21,misc3:b22,"

static const SDL_HIDAPI_SimpleButtonBinding Zhidong_S_layout_buttons[] = {
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
    { SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,  5, 0x80 },
    { SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,   5, 0x40 },
};

static const SDL_HIDAPI_SimpleAxisBinding Zhidong_S_layout_axes[] = {
    { SDL_GAMEPAD_AXIS_LEFTX,         6, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_LEFTY,         7, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_RIGHTX,        8, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_RIGHTY,        9, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_LEFT_TRIGGER,  10, SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255, false },
    { SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 11, SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255, false },
};

static const SDL_HIDAPI_SimpleReportLayout Zhidong_S_layout = {
    { 4, 0x40, 0x80, 0x10, 0x20 },
    Zhidong_S_layout_buttons, (int)SDL_arraysize(Zhidong_S_layout_buttons),
    Zhidong_S_layout_axes, (int)SDL_arraysize(Zhidong_S_layout_axes),
    NULL, 0,
    NULL,
};

static const Uint8 Zhidong_S_rumble_packet[] = { 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const SDL_HIDAPI_SimpleRumbleBinding Zhidong_S_rumble = {
    Zhidong_S_rumble_packet,
    (Uint8)SDL_arraysize(Zhidong_S_rumble_packet),
    3,                                              /* low frequency motor */
    4,                                              /* high frequency motor */
    NULL,                                           /* no trigger rumble packet */
    0,
    0,
    0,
#if defined(SDL_PLATFORM_WIN32)
    1,
#else
    0,
#endif
};

#define SDL_HIDAPI_SIMPLE_PROFILE_CONTROLLER_ENTRIES_ZHIDONG \
    { 0x11c0, 0x5505, 1, 2, "Zhidong O+ USB XINPUT", ZHIDONG_MAPPING_SUFFIX, &Zhidong_S_layout, &Zhidong_S_rumble, NULL, false }, \
    { 0x1949, 0x00c1, 1, 2, "Zhidong O+ USB DINPUT", ZHIDONG_MAPPING_SUFFIX, &Zhidong_S_layout, NULL, NULL, false }, \
    { 0x2345, 0xe023, 1, 2, "Zhidong O+ 2.4G XINPUT", ZHIDONG_MAPPING_SUFFIX, &Zhidong_S_layout, &Zhidong_S_rumble, NULL, false }, \
    { 0x2345, 0xe024, 1, 2, "Zhidong O+ 2.4G DINPUT", ZHIDONG_MAPPING_SUFFIX, &Zhidong_S_layout, NULL, NULL, false },

#endif
