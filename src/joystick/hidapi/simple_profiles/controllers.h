#ifndef SDL_hidapi_simple_profile_controllers_h_
#define SDL_hidapi_simple_profile_controllers_h_

typedef struct
{
    Uint16 vendor_id;
    Uint16 product_id;
    int interface_number; /* required HID interface (MI) number */
    int collection;
    const char *name;
    const char *mapping_string_suffix;
    const SDL_HIDAPI_SimpleReportLayout *layout;
    const SDL_HIDAPI_SimpleRumbleBinding *rumble;
    const SDL_HIDAPI_SimpleSensorBinding *sensors;
    bool block_xbox360_driver; /* if true, don't fall through to Xbox 360 HIDAPI drivers when simple profile doesn't claim */
} SDL_HIDAPI_SimpleDeviceProfile;

#include "controller_beitong.h"
#include "controller_legiongo.h"
#include "controller_zhidong.h"

static const SDL_HIDAPI_SimpleDeviceProfile SDL_hidapi_simple_profiles[] = {
    SDL_HIDAPI_SIMPLE_PROFILE_CONTROLLER_ENTRIES_LEGIONGO
    SDL_HIDAPI_SIMPLE_PROFILE_CONTROLLER_ENTRIES_ZHIDONG
    SDL_HIDAPI_SIMPLE_PROFILE_CONTROLLER_ENTRIES_BEITONG
};

#endif
