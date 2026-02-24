#ifndef SDL_hidapi_simple_profile_controller_beitong_h_
#define SDL_hidapi_simple_profile_controller_beitong_h_

#define USB_VENDOR_BEITONG 0x20bc
#define USB_PRODUCT_BEITONG_ZEUS2 0x5076

static const SDL_HIDAPI_SimpleButtonBinding Beitong_Zeus2_layout_buttons[] = {
    { SDL_GAMEPAD_BUTTON_START,          8, 0x10 },
    { SDL_GAMEPAD_BUTTON_BACK,           8, 0x20 },
    { SDL_GAMEPAD_BUTTON_LEFT_STICK,     8, 0x40 },
    { SDL_GAMEPAD_BUTTON_RIGHT_STICK,    8, 0x80 },

    { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  9, 0x01 },
    { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 9, 0x02 },
    { SDL_GAMEPAD_BUTTON_SOUTH,          9, 0x10 },
    { SDL_GAMEPAD_BUTTON_EAST,           9, 0x20 },
    { SDL_GAMEPAD_BUTTON_WEST,           9, 0x40 },
    { SDL_GAMEPAD_BUTTON_NORTH,          9, 0x80 },

    { SDL_GAMEPAD_BUTTON_GUIDE,          10, 0x40 },
    { SDL_GAMEPAD_BUTTON_MISC2,          10, 0x20 },
    { SDL_GAMEPAD_BUTTON_MISC3,          10, 0x10 },
    { SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,   10, 0x04 },
    { SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,   10, 0x08 },
    { SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,  10, 0x01 },
    { SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,  10, 0x02 },
};

static const SDL_HIDAPI_SimpleAxisBinding Beitong_Zeus2_layout_axes[] = {
    { SDL_GAMEPAD_AXIS_LEFTX,         2, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_LEFTY,         3, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, true },
    { SDL_GAMEPAD_AXIS_RIGHTX,        4, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, false },
    { SDL_GAMEPAD_AXIS_RIGHTY,        5, SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80, true },
    { SDL_GAMEPAD_AXIS_LEFT_TRIGGER,  6, SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255, false },
    { SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 7, SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255, false },
};

static const SDL_HIDAPI_SimpleReportLayout Beitong_Zeus2_layout = {
    { 8, 0x01, 0x02, 0x04, 0x08 },
    Beitong_Zeus2_layout_buttons, (int)SDL_arraysize(Beitong_Zeus2_layout_buttons),
    Beitong_Zeus2_layout_axes, (int)SDL_arraysize(Beitong_Zeus2_layout_axes),
};

static const SDL_HIDAPI_SimpleSensorBinding Beitong_Zeus2_sensor = {
    5,
    1,
    7,
    500.0f,
    (2.0f * SDL_STANDARD_GRAVITY) / 32768.0f,
    ((float)(SDL_PI_F / 180.0f)) / 16.0f,
};

#define SDL_HIDAPI_SIMPLE_PROFILE_CONTROLLER_ENTRIES_BEITONG \
    { USB_VENDOR_BEITONG, USB_PRODUCT_BEITONG_ZEUS2, false, 4, "Beitong Zeus 2", "paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,misc2:b21,misc3:b22,", &Beitong_Zeus2_layout, NULL, NULL, &Beitong_Zeus2_sensor },

#endif
