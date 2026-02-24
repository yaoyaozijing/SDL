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
} SDL_HIDAPI_SimpleRumbleBinding;

typedef struct
{
    Uint8 left_byte_index;
    Uint8 left_mask;
    Uint8 right_byte_index;
    Uint8 right_mask;
} SDL_HIDAPI_SimpleTriggerOverrideBinding;

typedef struct
{
    int collection;
    Uint8 gyro_offset;
    Uint8 accel_offset;
    float report_rate_hz;
    float accel_scale;
    float gyro_scale;
} SDL_HIDAPI_SimpleSensorBinding;

#include "simple_profiles/controllers.h"

extern const SDL_HIDAPI_SimpleDeviceProfile *HIDAPI_Simple_GetDeviceProfile(Uint16 vendor_id, Uint16 product_id);
extern bool HIDAPI_Simple_IsSupportedVIDPID(Uint16 vendor_id, Uint16 product_id);
extern const char *HIDAPI_Simple_GetMappingStringSuffix(Uint16 vendor_id, Uint16 product_id);

#endif
