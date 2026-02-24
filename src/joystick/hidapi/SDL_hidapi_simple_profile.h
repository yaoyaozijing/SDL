#ifndef SDL_hidapi_simple_profile_h_
#define SDL_hidapi_simple_profile_h_

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
    SDL_HIDAPI_SimpleDPadBinding dpad;
    const SDL_HIDAPI_SimpleButtonBinding *buttons;
    int num_buttons;
    const SDL_HIDAPI_SimpleAxisBinding *axes;
    int num_axes;
} SDL_HIDAPI_SimpleReportLayout;

typedef struct
{
    const Uint8 *packet_data;
    Uint8 packet_size;
    Uint8 low_frequency_byte_index;
    Uint8 high_frequency_byte_index;
} SDL_HIDAPI_SimpleRumbleBinding;

typedef struct
{
    const Uint8 *packet_data;
    Uint8 packet_size;
    Uint8 left_trigger_byte_index;
    Uint8 right_trigger_byte_index;
} SDL_HIDAPI_SimpleTriggerRumbleBinding;

typedef struct
{
    int collection;
    Uint8 gyro_offset;
    Uint8 accel_offset;
    float report_rate_hz;
    float accel_scale;
    float gyro_scale;
} SDL_HIDAPI_SimpleSensorBinding;

typedef struct
{
    Uint8 down_byte_index;
    Uint8 down_mask;
    Uint8 down_value;   // 1 or 0 depending on whether the "down" state is indicated by a bit being set or clear
    Uint8 x_high_byte_index;
    Uint8 x_low_byte_index;
    Uint8 y_high_byte_index;
    Uint8 y_low_byte_index;
    Uint16 x_resolution;
    Uint16 y_resolution;
} SDL_HIDAPI_SimpleTouchpadBinding;

#define SDL_HIDAPI_SIMPLE_PROFILE_TOUCH_BYTE_NONE 0xFF

#define SDL_HIDAPI_SIMPLE_TOUCHPAD_BINDING(down_byte, down_mask, down_value, x_high, x_low, y_high, y_low, x_res, y_res) \
    { (down_byte), (down_mask), (down_value), (x_high), (x_low), (y_high), (y_low), (x_res), (y_res) }

#include "simple_profiles/controllers.h"

extern const SDL_HIDAPI_SimpleDeviceProfile *HIDAPI_Simple_GetDeviceProfile(Uint16 vendor_id, Uint16 product_id);
extern bool HIDAPI_Simple_IsSupportedVIDPID(Uint16 vendor_id, Uint16 product_id);
extern const char *HIDAPI_Simple_GetMappingStringSuffix(Uint16 vendor_id, Uint16 product_id);

#endif
