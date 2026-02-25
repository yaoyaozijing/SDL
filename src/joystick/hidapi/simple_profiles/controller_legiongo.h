#ifndef SDL_hidapi_simple_profile_controller_legiongo_h_
#define SDL_hidapi_simple_profile_controller_legiongo_h_

#define USB_VENDOR_LENOVO 0x17ef

#define USB_PRODUCT_LEGIONGO_XINPUT 0x61eb
#define USB_PRODUCT_LEGIONGO_DINPUT 0x61ec

#define LEGIONGO_MAPPING_SUFFIX "misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,misc2:b21,misc3:b22,misc4:b23,misc5:b24,"

static const SDL_HIDAPI_SimpleAxisBinding LegionGo_layout_axes[] = {
    { SDL_GAMEPAD_AXIS_LEFTX,         12, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_LEFTY,         13, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_RIGHTX,        14, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_RIGHTY,        15, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_LEFT_TRIGGER,  21, SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255, false },
    { SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 20, SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255, false },
};

static const SDL_HIDAPI_SimpleButtonBinding LegionGo_layout_buttons[] = {
    /* byte 16: [0x01 -> 0x80] Right, Left, Down, Up, R3, L3, Right panel menu, Home */
    { SDL_GAMEPAD_BUTTON_RIGHT_STICK,    16, 0x10 },
    { SDL_GAMEPAD_BUTTON_LEFT_STICK,     16, 0x20 },
    { SDL_GAMEPAD_BUTTON_MISC1,          16, 0x40 },
    { SDL_GAMEPAD_BUTTON_GUIDE,          16, 0x80 },

    /* byte 17: [0x01 -> 0x80] RT click, RB, LT click, LB, Y, X, B, A */
    { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 17, 0x02 },
    { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  17, 0x08 },
    { SDL_GAMEPAD_BUTTON_NORTH,          17, 0x10 },
    { SDL_GAMEPAD_BUTTON_WEST,           17, 0x20 },
    { SDL_GAMEPAD_BUTTON_EAST,           17, 0x40 },
    { SDL_GAMEPAD_BUTTON_SOUTH,          17, 0x80 },

    /* byte 18: [0x01 -> 0x80] Menu, View, R back2, M2, M1, R back1, L back2, L back1 */
    { SDL_GAMEPAD_BUTTON_START,          18, 0x01 },
    { SDL_GAMEPAD_BUTTON_BACK,           18, 0x02 },
    { SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,  18, 0x04 },
    { SDL_GAMEPAD_BUTTON_MISC3,          18, 0x08 },
    { SDL_GAMEPAD_BUTTON_MISC2,          18, 0x10 },
    { SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,  18, 0x20 },
    { SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,   18, 0x40 },
    { SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,   18, 0x80 },

    { SDL_GAMEPAD_BUTTON_MISC4,          19, 0x04 },
    { SDL_GAMEPAD_BUTTON_MISC5,          19, 0x02 },
};

static const SDL_HIDAPI_SimpleTouchpadBinding LegionGo_touchpads[] = {
    SDL_HIDAPI_SIMPLE_TOUCHPAD_BINDING(
        SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE, 0x00, 1,
        24, 0x03, 25,
        26, 0x03, 27,
        1023, 1023
    ),
};

static const SDL_HIDAPI_SimpleSensorBinding LegionGo_sensor = {
    0,
    54, /* gyro x */
    56, /* gyro y */
    52, /* gyro z */
    48, /* accel x */
    50, /* accel y */
    46, /* accel z */
    45, /* report timestamp/counter */
    500.0f,
    (2.0f * SDL_STANDARD_GRAVITY) / 32768.0f,
    ((float)(SDL_PI_F / 180.0f)) / 16.0f,
};

static const SDL_HIDAPI_SimpleReportLayout LegionGo_layout = {
    { 16, 0x08, 0x04, 0x02, 0x01 },
    LegionGo_layout_buttons, (int)SDL_arraysize(LegionGo_layout_buttons),
    LegionGo_layout_axes, (int)SDL_arraysize(LegionGo_layout_axes),
};

#define SDL_HIDAPI_SIMPLE_PROFILE_CONTROLLER_ENTRIES_LEGIONGO \
    { USB_VENDOR_LENOVO, USB_PRODUCT_LEGIONGO_XINPUT, 0, "Lenovo Legion Go", LEGIONGO_MAPPING_SUFFIX, &LegionGo_layout, NULL, NULL, LegionGo_touchpads, 1, NULL, &LegionGo_sensor }, \
    { USB_VENDOR_LENOVO, USB_PRODUCT_LEGIONGO_DINPUT, 0, "Lenovo Legion Go", LEGIONGO_MAPPING_SUFFIX, &LegionGo_layout, NULL, NULL, LegionGo_touchpads, 1, NULL, &LegionGo_sensor },

#endif
