#ifndef SDL_hidapi_simple_profile_controller_zhidong_h_
#define SDL_hidapi_simple_profile_controller_zhidong_h_

#define USB_VENDOR_ZHIDONG_USB_XINPUT 0x11c0
#define USB_VENDOR_ZHIDONG_USB_DINPUT 0x1949
#define USB_VENDOR_ZHIDONG_24G 0x2345

#define USB_PRODUCT_ZHIDONG_USB_XINPUT 0x5505
#define USB_PRODUCT_ZHIDONG_USB_DINPUT 0x00c1
#define USB_PRODUCT_ZHIDONG_24G_XINPUT 0xe023
#define USB_PRODUCT_ZHIDONG_24G_DINPUT 0xe024

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
};

static const SDL_HIDAPI_SimpleTouchpadBinding Zhidong_S_touchpads[] = {
    SDL_HIDAPI_SIMPLE_TOUCHPAD_BINDING(
        3, 0x40, 1,
        SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE, 0xFF, 6,
        SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE, 0xFF, 7,
        255, 255
    ),
    SDL_HIDAPI_SIMPLE_TOUCHPAD_BINDING(
        4, 0x02, 1,
        SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE, 0xFF, 8,
        SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE, 0xFF, 9,
        255, 255
    ),
};

#define SDL_HIDAPI_SIMPLE_PROFILE_CONTROLLER_ENTRIES_ZHIDONG \
    { USB_VENDOR_ZHIDONG_USB_XINPUT, USB_PRODUCT_ZHIDONG_USB_XINPUT, 2, "Zhidong Controller", "misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,misc2:b21,misc3:b22,", &Zhidong_S_layout, NULL, NULL, Zhidong_S_touchpads, 2, NULL, NULL }, \
    { USB_VENDOR_ZHIDONG_USB_DINPUT, USB_PRODUCT_ZHIDONG_USB_DINPUT, 2, "Zhidong Controller", "misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,misc2:b21,misc3:b22,", &Zhidong_S_layout, NULL, NULL, NULL, 0, NULL, NULL }, \
    { USB_VENDOR_ZHIDONG_24G, USB_PRODUCT_ZHIDONG_24G_XINPUT, 2, "Zhidong Controller", "misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,misc2:b21,misc3:b22,", &Zhidong_S_layout, NULL, NULL, NULL, 0, NULL, NULL }, \
    { USB_VENDOR_ZHIDONG_24G, USB_PRODUCT_ZHIDONG_24G_DINPUT, 2, "Zhidong Controller", "misc1:b15,paddle1:b16,paddle2:b17,paddle3:b18,paddle4:b19,misc2:b21,misc3:b22,", &Zhidong_S_layout, NULL, NULL, NULL, 0, NULL, NULL },

#endif
