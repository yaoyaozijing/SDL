#ifndef SDL_hidapi_simple_profile_controller_beitong_h_
#define SDL_hidapi_simple_profile_controller_beitong_h_

#define BEITONG_MAPPING_SUFFIX "misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,misc2:b21,misc3:b22,"


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

static const SDL_HIDAPI_SimpleBatteryBinding Beitong_Zeus2_battery =
    SDL_HIDAPI_SIMPLE_BATTERY_BINDING(23);

static const SDL_HIDAPI_SimpleReportLayout Beitong_Zeus2_layout = {
    { 8, 0x01, 0x02, 0x04, 0x08 },
    Beitong_Zeus2_layout_buttons, (int)SDL_arraysize(Beitong_Zeus2_layout_buttons),
    Beitong_Zeus2_layout_axes, (int)SDL_arraysize(Beitong_Zeus2_layout_axes),
    NULL, 0,
    &Beitong_Zeus2_battery,
};

/* Xbox 360 style rumble packet: low motor at byte 3, high motor at byte 4 */
static const Uint8 Beitong_Zeus2_rumble_packet[] = { 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const SDL_HIDAPI_SimpleRumbleBinding Beitong_Zeus2_rumble = {
    Beitong_Zeus2_rumble_packet,
    (Uint8)SDL_arraysize(Beitong_Zeus2_rumble_packet),
    3, /* low frequency motor */
    4, /* high frequency motor */
    NULL,
    0,
    0,
    0,
#if defined(SDL_PLATFORM_WIN32)
    2,
#else
    0,
#endif
};

static const SDL_HIDAPI_SimpleSensorBinding Beitong_Zeus2_sensor = {
    0,  /* MI */
    5,  /* COL */
    9,  /* gyro x */
    7,  /* gyro y */
    11, /* gyro z */
    3,  /* accel x */
    1,  /* accel y */
    5,  /* accel z */
    SDL_HIDAPI_SIMPLE_PROFILE_SENSOR_BYTE_NONE,
    500.0f,
    (2.0f * SDL_STANDARD_GRAVITY) / 32768.0f,
    (2000.0f * (float)(SDL_PI_F / 180.0f)) / 32768.0f,
    SDL_HIDAPI_SIMPLE_PROFILE_SENSOR_AXIS_Y,
    SDL_HIDAPI_SIMPLE_PROFILE_SENSOR_AXIS_Y,
};

#define SDL_HIDAPI_SIMPLE_PROFILE_CONTROLLER_ENTRIES_BEITONG \
    { 0x20bc, 0x5073, 0, 4, "Beitong Zeus 2 2.4G", BEITONG_MAPPING_SUFFIX, &Beitong_Zeus2_layout, &Beitong_Zeus2_rumble, &Beitong_Zeus2_sensor, true }, \
    { 0x20bc, 0x5076, 0, 4, "Beitong Zeus 2 USB", BEITONG_MAPPING_SUFFIX, &Beitong_Zeus2_layout, &Beitong_Zeus2_rumble, &Beitong_Zeus2_sensor, true },

#endif
