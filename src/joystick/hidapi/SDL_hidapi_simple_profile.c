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

#ifdef SDL_JOYSTICK_HIDAPI_SIMPLE_PROFILE

// Define this if you want to log all packets from the controller
#if 0
#define DEBUG_SIMPLE_PROFILE_PROTOCOL
#endif

#ifndef SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE
#define SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE "SDL_JOYSTICK_HIDAPI_SIMPLE_PROFILE"
#endif

#ifndef SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE_CONFIG
#define SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE_CONFIG "SDL_JOYSTICK_HIDAPI_SIMPLE_PROFILE_CONFIG"
#endif

#ifndef SDL_HIDAPI_SIMPLE_PROFILE_DEFAULT_CONFIG_DIR
#define SDL_HIDAPI_SIMPLE_PROFILE_DEFAULT_CONFIG_DIR "src/joystick/hidapi/simple_profiles"
#endif

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
 * JSON profile directory.
 * Hint value accepts a directory path and every *.json file in that directory is scanned.
 * If the hint is unset, SDL_HIDAPI_SIMPLE_PROFILE_DEFAULT_CONFIG_DIR is used.
 *
 * Supported root shape:
 * - { "profiles": [ ... ] }
 * - { "dpad_defs": { ... }, "buttons_defs": { ... }, "axes_defs": { ... }, "profiles": [ ... ] }
 * - [ ... ]
 * - { ... } (single profile object)
 *
 * Profile fields:
 * - vendor_id, product_id, allow_swapped_vid_pid, collection
 * - name, mapping_string_suffix
 * - report_size_min
 * - dpad: { byte_index, up_mask, down_mask, left_mask, right_mask }
 * - dpad_ref: "name"
 * - buttons: [ { button, byte_index, mask } ]
 * - buttons_ref: "name"
 * - axes: [ { axis, byte_index, encoding, invert } ]
 * - axes_ref: "name"
 *
 * When using *_defs with profiles, define *_defs before profiles in the same root object.
 */
typedef struct
{
    SDL_HIDAPI_SimpleDeviceProfile profile;
    SDL_HIDAPI_SimpleReportLayout layout;
    SDL_HIDAPI_SimpleButtonBinding *buttons;
    int num_buttons;
    SDL_HIDAPI_SimpleAxisBinding *axes;
    int num_axes;
    char *name;
    char *mapping_string_suffix;
} SDL_HIDAPI_SimpleLoadedProfile;

static SDL_HIDAPI_SimpleLoadedProfile *SDL_hidapi_simple_loaded_profiles = NULL;
static int SDL_hidapi_simple_num_loaded_profiles = 0;
static bool SDL_hidapi_simple_profiles_initialized = false;

typedef struct
{
    const SDL_HIDAPI_SimpleDeviceProfile *profile;
    bool last_state_initialized;
    Uint8 last_state[USB_PACKET_LENGTH];
} SDL_DriverSimpleProfile_Context;

typedef struct
{
    const char *start;
    const char *ptr;
    const char *end;
    const char *error;
} SDL_DriverSimpleProfile_JSONParser;

typedef struct
{
    bool has_vendor_id;
    Uint16 vendor_id;
    bool has_product_id;
    Uint16 product_id;
    bool has_allow_swapped_vid_pid;
    bool allow_swapped_vid_pid;
    bool has_collection;
    int collection;
    char *name;
    char *mapping_string_suffix;
    bool has_report_size_min;
    Uint8 report_size_min;
    bool has_dpad;
    SDL_HIDAPI_SimpleDPadBinding dpad;
    char *dpad_ref;
    SDL_HIDAPI_SimpleButtonBinding *buttons;
    int num_buttons;
    char *buttons_ref;
    SDL_HIDAPI_SimpleAxisBinding *axes;
    int num_axes;
    char *axes_ref;
} SDL_DriverSimpleProfile_JSONProfile;

typedef struct
{
    char *name;
    SDL_HIDAPI_SimpleDPadBinding dpad;
} SDL_DriverSimpleProfile_JSONNamedDPad;

typedef struct
{
    char *name;
    SDL_HIDAPI_SimpleButtonBinding *buttons;
    int num_buttons;
} SDL_DriverSimpleProfile_JSONNamedButtons;

typedef struct
{
    char *name;
    SDL_HIDAPI_SimpleAxisBinding *axes;
    int num_axes;
} SDL_DriverSimpleProfile_JSONNamedAxes;

typedef struct
{
    SDL_DriverSimpleProfile_JSONNamedDPad *dpads;
    int num_dpads;
    SDL_DriverSimpleProfile_JSONNamedButtons *buttons;
    int num_buttons;
    SDL_DriverSimpleProfile_JSONNamedAxes *axes;
    int num_axes;
} SDL_DriverSimpleProfile_JSONDefinitions;

static bool HIDAPI_DriverSimpleProfile_JSONSetError(SDL_DriverSimpleProfile_JSONParser *parser, const char *error)
{
    if (!parser->error) {
        parser->error = error;
    }
    return false;
}

static void HIDAPI_DriverSimpleProfile_JSONSkipWhitespace(SDL_DriverSimpleProfile_JSONParser *parser)
{
    while (parser->ptr < parser->end && SDL_isspace((unsigned char)*parser->ptr)) {
        ++parser->ptr;
    }
}

static char HIDAPI_DriverSimpleProfile_JSONPeek(SDL_DriverSimpleProfile_JSONParser *parser)
{
    HIDAPI_DriverSimpleProfile_JSONSkipWhitespace(parser);
    if (parser->ptr >= parser->end) {
        return '\0';
    }
    return *parser->ptr;
}

static bool HIDAPI_DriverSimpleProfile_JSONConsume(SDL_DriverSimpleProfile_JSONParser *parser, char expected)
{
    HIDAPI_DriverSimpleProfile_JSONSkipWhitespace(parser);
    if (parser->ptr >= parser->end || *parser->ptr != expected) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Unexpected token");
    }
    ++parser->ptr;
    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONParseString(SDL_DriverSimpleProfile_JSONParser *parser, char *buffer, size_t buffer_size)
{
    size_t len = 0;

    HIDAPI_DriverSimpleProfile_JSONSkipWhitespace(parser);
    if (parser->ptr >= parser->end || *parser->ptr != '"') {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected string");
    }
    ++parser->ptr;

    while (parser->ptr < parser->end) {
        char c = *parser->ptr++;

        if (c == '"') {
            if (buffer && buffer_size > 0) {
                buffer[len] = '\0';
            }
            return true;
        }

        if (c == '\\') {
            if (parser->ptr >= parser->end) {
                return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Invalid escape sequence");
            }
            c = *parser->ptr++;
            switch (c) {
            case '"':
            case '\\':
            case '/':
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'u': {
                int i;
                if ((parser->end - parser->ptr) < 4) {
                    return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Invalid unicode escape");
                }
                for (i = 0; i < 4; ++i) {
                    if (!SDL_isxdigit((unsigned char)parser->ptr[i])) {
                        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Invalid unicode escape");
                    }
                }
                parser->ptr += 4;
                c = '?';
                break;
            }
            default:
                return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Invalid escape sequence");
            }
        }

        if (buffer && buffer_size > 0) {
            if ((len + 1) >= buffer_size) {
                return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "String too long");
            }
            buffer[len++] = c;
        }
    }

    return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Unterminated string");
}

static bool HIDAPI_DriverSimpleProfile_JSONParseToken(SDL_DriverSimpleProfile_JSONParser *parser, char *buffer, size_t buffer_size)
{
    size_t len = 0;

    HIDAPI_DriverSimpleProfile_JSONSkipWhitespace(parser);
    if (parser->ptr >= parser->end) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Unexpected end of JSON");
    }

    while (parser->ptr < parser->end) {
        const char c = *parser->ptr;
        if (SDL_isspace((unsigned char)c) || c == ',' || c == ']' || c == '}' || c == ':') {
            break;
        }
        if ((len + 1) >= buffer_size) {
            return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Token too long");
        }
        buffer[len++] = c;
        ++parser->ptr;
    }

    if (len == 0) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected token");
    }

    buffer[len] = '\0';
    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONParseInt(SDL_DriverSimpleProfile_JSONParser *parser, int *value)
{
    char token[64];
    char *endptr = NULL;
    long parsed_value;

    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '"') {
        if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, token, sizeof(token))) {
            return false;
        }
    } else {
        if (!HIDAPI_DriverSimpleProfile_JSONParseToken(parser, token, sizeof(token))) {
            return false;
        }
    }

    parsed_value = SDL_strtol(token, &endptr, 0);
    if (!endptr || endptr == token || *endptr != '\0') {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Invalid integer value");
    }
    if (parsed_value < SDL_MIN_SINT32 || parsed_value > SDL_MAX_SINT32) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Integer value out of range");
    }

    *value = (int)parsed_value;
    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONParseBool(SDL_DriverSimpleProfile_JSONParser *parser, bool *value)
{
    char token[16];

    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '"') {
        if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, token, sizeof(token))) {
            return false;
        }
    } else {
        if (!HIDAPI_DriverSimpleProfile_JSONParseToken(parser, token, sizeof(token))) {
            return false;
        }
    }

    if (SDL_strcasecmp(token, "true") == 0 || SDL_strcmp(token, "1") == 0) {
        *value = true;
        return true;
    }
    if (SDL_strcasecmp(token, "false") == 0 || SDL_strcmp(token, "0") == 0) {
        *value = false;
        return true;
    }

    return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Invalid boolean value");
}

static bool HIDAPI_DriverSimpleProfile_JSONSkipValue(SDL_DriverSimpleProfile_JSONParser *parser, int depth);

static bool HIDAPI_DriverSimpleProfile_JSONSkipObject(SDL_DriverSimpleProfile_JSONParser *parser, int depth)
{
    if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, '{')) {
        return false;
    }

    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '}') {
        return HIDAPI_DriverSimpleProfile_JSONConsume(parser, '}');
    }

    for (;;) {
        char key[64];
        char separator;

        if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, key, sizeof(key))) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, ':')) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONSkipValue(parser, depth + 1)) {
            return false;
        }

        separator = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
        if (separator == ',') {
            ++parser->ptr;
            continue;
        }
        if (separator == '}') {
            ++parser->ptr;
            return true;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected ',' or '}'");
    }
}

static bool HIDAPI_DriverSimpleProfile_JSONSkipArray(SDL_DriverSimpleProfile_JSONParser *parser, int depth)
{
    if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, '[')) {
        return false;
    }

    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == ']') {
        return HIDAPI_DriverSimpleProfile_JSONConsume(parser, ']');
    }

    for (;;) {
        char separator;

        if (!HIDAPI_DriverSimpleProfile_JSONSkipValue(parser, depth + 1)) {
            return false;
        }

        separator = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
        if (separator == ',') {
            ++parser->ptr;
            continue;
        }
        if (separator == ']') {
            ++parser->ptr;
            return true;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected ',' or ']'");
    }
}

static bool HIDAPI_DriverSimpleProfile_JSONSkipValue(SDL_DriverSimpleProfile_JSONParser *parser, int depth)
{
    char token[64];
    char c;

    if (depth > 32) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "JSON nesting too deep");
    }

    c = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
    switch (c) {
    case '{':
        return HIDAPI_DriverSimpleProfile_JSONSkipObject(parser, depth);
    case '[':
        return HIDAPI_DriverSimpleProfile_JSONSkipArray(parser, depth);
    case '"':
        return HIDAPI_DriverSimpleProfile_JSONParseString(parser, NULL, 0);
    default:
        if (!HIDAPI_DriverSimpleProfile_JSONParseToken(parser, token, sizeof(token))) {
            return false;
        }
        return true;
    }
}

static void HIDAPI_DriverSimpleProfile_JSONProfileReset(SDL_DriverSimpleProfile_JSONProfile *profile)
{
    if (!profile) {
        return;
    }

    SDL_free(profile->name);
    SDL_free(profile->mapping_string_suffix);
    SDL_free(profile->dpad_ref);
    SDL_free(profile->buttons);
    SDL_free(profile->buttons_ref);
    SDL_free(profile->axes);
    SDL_free(profile->axes_ref);
    SDL_zero(*profile);
}

static void HIDAPI_DriverSimpleProfile_JSONDefinitionsReset(SDL_DriverSimpleProfile_JSONDefinitions *defs)
{
    int i;

    if (!defs) {
        return;
    }

    for (i = 0; i < defs->num_dpads; ++i) {
        SDL_free(defs->dpads[i].name);
    }
    SDL_free(defs->dpads);

    for (i = 0; i < defs->num_buttons; ++i) {
        SDL_free(defs->buttons[i].name);
        SDL_free(defs->buttons[i].buttons);
    }
    SDL_free(defs->buttons);

    for (i = 0; i < defs->num_axes; ++i) {
        SDL_free(defs->axes[i].name);
        SDL_free(defs->axes[i].axes);
    }
    SDL_free(defs->axes);

    SDL_zero(*defs);
}

static bool HIDAPI_DriverSimpleProfile_JSONParseAllocatedString(SDL_DriverSimpleProfile_JSONParser *parser, char **value)
{
    char buffer[256];
    char *string;

    if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, buffer, sizeof(buffer))) {
        return false;
    }

    string = SDL_strdup(buffer);
    if (!string) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Out of memory");
    }
    SDL_free(*value);
    *value = string;
    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONParseAxisEncoding(SDL_DriverSimpleProfile_JSONParser *parser, SDL_HIDAPI_SimpleAxisEncoding *encoding)
{
    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '"') {
        char value[64];

        if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, value, sizeof(value))) {
            return false;
        }
        if (SDL_strcasecmp(value, "stick_8bit_center_0x80") == 0) {
            *encoding = SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80;
            return true;
        }
        if (SDL_strcasecmp(value, "trigger_8bit_0_255") == 0) {
            *encoding = SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255;
            return true;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Unknown axis encoding");
    } else {
        int value = 0;
        if (!HIDAPI_DriverSimpleProfile_JSONParseInt(parser, &value)) {
            return false;
        }
        if (value < 0 || value > SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255) {
            return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Axis encoding out of range");
        }
        *encoding = (SDL_HIDAPI_SimpleAxisEncoding)value;
        return true;
    }
}

static bool HIDAPI_DriverSimpleProfile_JSONParseButton(SDL_DriverSimpleProfile_JSONParser *parser, SDL_HIDAPI_SimpleButtonBinding *binding)
{
    bool has_button = false;
    bool has_byte_index = false;
    bool has_mask = false;

    SDL_zero(*binding);

    if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, '{')) {
        return false;
    }

    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '}') {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Button entry must not be empty");
    }

    for (;;) {
        char key[64];
        char separator;

        if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, key, sizeof(key))) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, ':')) {
            return false;
        }

        if (SDL_strcasecmp(key, "button") == 0) {
            if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '"') {
                char button_name[64];
                SDL_GamepadButton button;

                if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, button_name, sizeof(button_name))) {
                    return false;
                }
                button = SDL_GetGamepadButtonFromString(button_name);
                if (button == SDL_GAMEPAD_BUTTON_INVALID) {
                    return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Unknown button name");
                }
                binding->button = button;
            } else {
                int button_index = 0;

                if (!HIDAPI_DriverSimpleProfile_JSONParseInt(parser, &button_index)) {
                    return false;
                }
                if (button_index < 0 || button_index >= SDL_GAMEPAD_BUTTON_COUNT) {
                    return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Button index out of range");
                }
                binding->button = (SDL_GamepadButton)button_index;
            }
            has_button = true;
        } else if (SDL_strcasecmp(key, "byte_index") == 0) {
            int byte_index = 0;
            if (!HIDAPI_DriverSimpleProfile_JSONParseInt(parser, &byte_index)) {
                return false;
            }
            if (byte_index < 0 || byte_index > 0xff) {
                return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "byte_index out of range");
            }
            binding->byte_index = (Uint8)byte_index;
            has_byte_index = true;
        } else if (SDL_strcasecmp(key, "mask") == 0) {
            int mask = 0;
            if (!HIDAPI_DriverSimpleProfile_JSONParseInt(parser, &mask)) {
                return false;
            }
            if (mask < 0 || mask > 0xff) {
                return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "mask out of range");
            }
            binding->mask = (Uint8)mask;
            has_mask = true;
        } else {
            if (!HIDAPI_DriverSimpleProfile_JSONSkipValue(parser, 0)) {
                return false;
            }
        }

        separator = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
        if (separator == ',') {
            ++parser->ptr;
            continue;
        }
        if (separator == '}') {
            ++parser->ptr;
            break;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected ',' or '}' in button object");
    }

    if (!has_button || !has_byte_index || !has_mask) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Button entry is missing required fields");
    }

    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONParseButtonsArrayTo(SDL_DriverSimpleProfile_JSONParser *parser,
                                                               SDL_HIDAPI_SimpleButtonBinding **bindings,
                                                               int *num_bindings)
{
    if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, '[')) {
        return false;
    }

    SDL_free(*bindings);
    *bindings = NULL;
    *num_bindings = 0;

    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == ']') {
        return HIDAPI_DriverSimpleProfile_JSONConsume(parser, ']');
    }

    for (;;) {
        SDL_HIDAPI_SimpleButtonBinding binding;
        SDL_HIDAPI_SimpleButtonBinding *new_buttons;
        char separator;

        if (!HIDAPI_DriverSimpleProfile_JSONParseButton(parser, &binding)) {
            return false;
        }

        if (*num_bindings >= SDL_MAX_SINT32 / (int)sizeof(**bindings)) {
            return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Too many buttons");
        }

        new_buttons = (SDL_HIDAPI_SimpleButtonBinding *)SDL_realloc(*bindings, (*num_bindings + 1) * sizeof(**bindings));
        if (!new_buttons) {
            return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Out of memory");
        }
        *bindings = new_buttons;
        (*bindings)[(*num_bindings)++] = binding;

        separator = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
        if (separator == ',') {
            ++parser->ptr;
            continue;
        }
        if (separator == ']') {
            ++parser->ptr;
            break;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected ',' or ']'");
    }

    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONParseButtonsArray(SDL_DriverSimpleProfile_JSONParser *parser, SDL_DriverSimpleProfile_JSONProfile *profile)
{
    return HIDAPI_DriverSimpleProfile_JSONParseButtonsArrayTo(parser, &profile->buttons, &profile->num_buttons);
}

static bool HIDAPI_DriverSimpleProfile_JSONParseDPad(SDL_DriverSimpleProfile_JSONParser *parser, SDL_HIDAPI_SimpleDPadBinding *dpad)
{
    bool has_byte_index = false;
    bool has_up_mask = false;
    bool has_down_mask = false;
    bool has_left_mask = false;
    bool has_right_mask = false;

    SDL_zero(*dpad);

    if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, '{')) {
        return false;
    }

    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '}') {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "dpad object must not be empty");
    }

    for (;;) {
        char key[64];
        char separator;
        int value = 0;

        if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, key, sizeof(key))) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, ':')) {
            return false;
        }

        if (SDL_strcasecmp(key, "byte_index") == 0 ||
            SDL_strcasecmp(key, "up_mask") == 0 ||
            SDL_strcasecmp(key, "down_mask") == 0 ||
            SDL_strcasecmp(key, "left_mask") == 0 ||
            SDL_strcasecmp(key, "right_mask") == 0) {
            if (!HIDAPI_DriverSimpleProfile_JSONParseInt(parser, &value)) {
                return false;
            }
            if (value < 0 || value > 0xff) {
                return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "dpad field out of range");
            }
            if (SDL_strcasecmp(key, "byte_index") == 0) {
                dpad->byte_index = (Uint8)value;
                has_byte_index = true;
            } else if (SDL_strcasecmp(key, "up_mask") == 0) {
                dpad->up_mask = (Uint8)value;
                has_up_mask = true;
            } else if (SDL_strcasecmp(key, "down_mask") == 0) {
                dpad->down_mask = (Uint8)value;
                has_down_mask = true;
            } else if (SDL_strcasecmp(key, "left_mask") == 0) {
                dpad->left_mask = (Uint8)value;
                has_left_mask = true;
            } else if (SDL_strcasecmp(key, "right_mask") == 0) {
                dpad->right_mask = (Uint8)value;
                has_right_mask = true;
            }
        } else {
            if (!HIDAPI_DriverSimpleProfile_JSONSkipValue(parser, 0)) {
                return false;
            }
        }

        separator = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
        if (separator == ',') {
            ++parser->ptr;
            continue;
        }
        if (separator == '}') {
            ++parser->ptr;
            break;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected ',' or '}' in dpad object");
    }

    if (!has_byte_index || !has_up_mask || !has_down_mask || !has_left_mask || !has_right_mask) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "dpad object missing required fields");
    }

    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONParseAxis(SDL_DriverSimpleProfile_JSONParser *parser, SDL_HIDAPI_SimpleAxisBinding *axis)
{
    bool has_axis = false;
    bool has_byte_index = false;
    bool has_encoding = false;

    SDL_zero(*axis);
    axis->invert = false;

    if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, '{')) {
        return false;
    }
    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '}') {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Axis entry must not be empty");
    }

    for (;;) {
        char key[64];
        char separator;

        if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, key, sizeof(key))) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, ':')) {
            return false;
        }

        if (SDL_strcasecmp(key, "axis") == 0) {
            if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '"') {
                char axis_name[64];
                SDL_GamepadAxis gamepad_axis;
                if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, axis_name, sizeof(axis_name))) {
                    return false;
                }
                gamepad_axis = SDL_GetGamepadAxisFromString(axis_name);
                if (gamepad_axis == SDL_GAMEPAD_AXIS_INVALID) {
                    return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Unknown axis name");
                }
                axis->axis = gamepad_axis;
            } else {
                int axis_index = 0;
                if (!HIDAPI_DriverSimpleProfile_JSONParseInt(parser, &axis_index)) {
                    return false;
                }
                if (axis_index < 0 || axis_index >= SDL_GAMEPAD_AXIS_COUNT) {
                    return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Axis index out of range");
                }
                axis->axis = (SDL_GamepadAxis)axis_index;
            }
            has_axis = true;
        } else if (SDL_strcasecmp(key, "byte_index") == 0) {
            int byte_index = 0;
            if (!HIDAPI_DriverSimpleProfile_JSONParseInt(parser, &byte_index)) {
                return false;
            }
            if (byte_index < 0 || byte_index > 0xff) {
                return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Axis byte_index out of range");
            }
            axis->byte_index = (Uint8)byte_index;
            has_byte_index = true;
        } else if (SDL_strcasecmp(key, "encoding") == 0) {
            if (!HIDAPI_DriverSimpleProfile_JSONParseAxisEncoding(parser, &axis->encoding)) {
                return false;
            }
            has_encoding = true;
        } else if (SDL_strcasecmp(key, "invert") == 0) {
            bool invert = false;
            if (!HIDAPI_DriverSimpleProfile_JSONParseBool(parser, &invert)) {
                return false;
            }
            axis->invert = invert;
        } else {
            if (!HIDAPI_DriverSimpleProfile_JSONSkipValue(parser, 0)) {
                return false;
            }
        }

        separator = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
        if (separator == ',') {
            ++parser->ptr;
            continue;
        }
        if (separator == '}') {
            ++parser->ptr;
            break;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected ',' or '}' in axis object");
    }

    if (!has_axis || !has_byte_index || !has_encoding) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Axis entry is missing required fields");
    }

    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONParseAxesArrayTo(SDL_DriverSimpleProfile_JSONParser *parser,
                                                            SDL_HIDAPI_SimpleAxisBinding **bindings,
                                                            int *num_bindings)
{
    if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, '[')) {
        return false;
    }

    SDL_free(*bindings);
    *bindings = NULL;
    *num_bindings = 0;

    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == ']') {
        return HIDAPI_DriverSimpleProfile_JSONConsume(parser, ']');
    }

    for (;;) {
        SDL_HIDAPI_SimpleAxisBinding axis;
        SDL_HIDAPI_SimpleAxisBinding *new_axes;
        char separator;

        if (!HIDAPI_DriverSimpleProfile_JSONParseAxis(parser, &axis)) {
            return false;
        }

        if (*num_bindings >= SDL_MAX_SINT32 / (int)sizeof(**bindings)) {
            return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Too many axes");
        }

        new_axes = (SDL_HIDAPI_SimpleAxisBinding *)SDL_realloc(*bindings, (*num_bindings + 1) * sizeof(**bindings));
        if (!new_axes) {
            return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Out of memory");
        }
        *bindings = new_axes;
        (*bindings)[(*num_bindings)++] = axis;

        separator = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
        if (separator == ',') {
            ++parser->ptr;
            continue;
        }
        if (separator == ']') {
            ++parser->ptr;
            break;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected ',' or ']'");
    }

    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONParseAxesArray(SDL_DriverSimpleProfile_JSONParser *parser, SDL_DriverSimpleProfile_JSONProfile *profile)
{
    return HIDAPI_DriverSimpleProfile_JSONParseAxesArrayTo(parser, &profile->axes, &profile->num_axes);
}

static bool HIDAPI_DriverSimpleProfile_CopyButtons(const SDL_HIDAPI_SimpleButtonBinding *src, int num_src, SDL_HIDAPI_SimpleButtonBinding **dst, int *num_dst)
{
    SDL_HIDAPI_SimpleButtonBinding *copy;

    if (!src || num_src <= 0 || !dst || !num_dst) {
        return false;
    }

    copy = (SDL_HIDAPI_SimpleButtonBinding *)SDL_malloc(num_src * sizeof(*copy));
    if (!copy) {
        return false;
    }
    SDL_memcpy(copy, src, num_src * sizeof(*copy));

    SDL_free(*dst);
    *dst = copy;
    *num_dst = num_src;
    return true;
}

static bool HIDAPI_DriverSimpleProfile_CopyAxes(const SDL_HIDAPI_SimpleAxisBinding *src, int num_src, SDL_HIDAPI_SimpleAxisBinding **dst, int *num_dst)
{
    SDL_HIDAPI_SimpleAxisBinding *copy;

    if (!src || num_src <= 0 || !dst || !num_dst) {
        return false;
    }

    copy = (SDL_HIDAPI_SimpleAxisBinding *)SDL_malloc(num_src * sizeof(*copy));
    if (!copy) {
        return false;
    }
    SDL_memcpy(copy, src, num_src * sizeof(*copy));

    SDL_free(*dst);
    *dst = copy;
    *num_dst = num_src;
    return true;
}

static SDL_DriverSimpleProfile_JSONNamedDPad *HIDAPI_DriverSimpleProfile_FindNamedDPad(SDL_DriverSimpleProfile_JSONDefinitions *defs, const char *name)
{
    int i;

    if (!defs || !name || !*name) {
        return NULL;
    }
    for (i = 0; i < defs->num_dpads; ++i) {
        if (SDL_strcmp(defs->dpads[i].name, name) == 0) {
            return &defs->dpads[i];
        }
    }
    return NULL;
}

static SDL_DriverSimpleProfile_JSONNamedButtons *HIDAPI_DriverSimpleProfile_FindNamedButtons(SDL_DriverSimpleProfile_JSONDefinitions *defs, const char *name)
{
    int i;

    if (!defs || !name || !*name) {
        return NULL;
    }
    for (i = 0; i < defs->num_buttons; ++i) {
        if (SDL_strcmp(defs->buttons[i].name, name) == 0) {
            return &defs->buttons[i];
        }
    }
    return NULL;
}

static SDL_DriverSimpleProfile_JSONNamedAxes *HIDAPI_DriverSimpleProfile_FindNamedAxes(SDL_DriverSimpleProfile_JSONDefinitions *defs, const char *name)
{
    int i;

    if (!defs || !name || !*name) {
        return NULL;
    }
    for (i = 0; i < defs->num_axes; ++i) {
        if (SDL_strcmp(defs->axes[i].name, name) == 0) {
            return &defs->axes[i];
        }
    }
    return NULL;
}

static bool HIDAPI_DriverSimpleProfile_AddNamedDPad(SDL_DriverSimpleProfile_JSONParser *parser, SDL_DriverSimpleProfile_JSONDefinitions *defs, const char *name, const SDL_HIDAPI_SimpleDPadBinding *dpad)
{
    SDL_DriverSimpleProfile_JSONNamedDPad *new_dpads;
    char *name_copy;

    if (!defs || !name || !*name || !dpad) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Invalid dpad definition");
    }
    if (HIDAPI_DriverSimpleProfile_FindNamedDPad(defs, name)) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Duplicate dpad definition name");
    }
    if (defs->num_dpads >= SDL_MAX_SINT32 / (int)sizeof(*defs->dpads)) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Too many dpad definitions");
    }

    name_copy = SDL_strdup(name);
    if (!name_copy) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Out of memory");
    }

    new_dpads = (SDL_DriverSimpleProfile_JSONNamedDPad *)SDL_realloc(defs->dpads, (defs->num_dpads + 1) * sizeof(*defs->dpads));
    if (!new_dpads) {
        SDL_free(name_copy);
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Out of memory");
    }
    defs->dpads = new_dpads;
    defs->dpads[defs->num_dpads].name = name_copy;
    defs->dpads[defs->num_dpads].dpad = *dpad;
    ++defs->num_dpads;
    return true;
}

static bool HIDAPI_DriverSimpleProfile_AddNamedButtons(SDL_DriverSimpleProfile_JSONParser *parser, SDL_DriverSimpleProfile_JSONDefinitions *defs, const char *name, const SDL_HIDAPI_SimpleButtonBinding *buttons, int num_buttons)
{
    SDL_DriverSimpleProfile_JSONNamedButtons *new_defs;
    SDL_HIDAPI_SimpleButtonBinding *buttons_copy = NULL;
    char *name_copy;

    if (!defs || !name || !*name || !buttons || num_buttons <= 0) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Invalid buttons definition");
    }
    if (HIDAPI_DriverSimpleProfile_FindNamedButtons(defs, name)) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Duplicate buttons definition name");
    }
    if (defs->num_buttons >= SDL_MAX_SINT32 / (int)sizeof(*defs->buttons)) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Too many buttons definitions");
    }

    if (!HIDAPI_DriverSimpleProfile_CopyButtons(buttons, num_buttons, &buttons_copy, &num_buttons)) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Out of memory");
    }

    name_copy = SDL_strdup(name);
    if (!name_copy) {
        SDL_free(buttons_copy);
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Out of memory");
    }

    new_defs = (SDL_DriverSimpleProfile_JSONNamedButtons *)SDL_realloc(defs->buttons, (defs->num_buttons + 1) * sizeof(*defs->buttons));
    if (!new_defs) {
        SDL_free(name_copy);
        SDL_free(buttons_copy);
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Out of memory");
    }
    defs->buttons = new_defs;
    defs->buttons[defs->num_buttons].name = name_copy;
    defs->buttons[defs->num_buttons].buttons = buttons_copy;
    defs->buttons[defs->num_buttons].num_buttons = num_buttons;
    ++defs->num_buttons;
    return true;
}

static bool HIDAPI_DriverSimpleProfile_AddNamedAxes(SDL_DriverSimpleProfile_JSONParser *parser, SDL_DriverSimpleProfile_JSONDefinitions *defs, const char *name, const SDL_HIDAPI_SimpleAxisBinding *axes, int num_axes)
{
    SDL_DriverSimpleProfile_JSONNamedAxes *new_defs;
    SDL_HIDAPI_SimpleAxisBinding *axes_copy = NULL;
    char *name_copy;

    if (!defs || !name || !*name || !axes || num_axes <= 0) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Invalid axes definition");
    }
    if (HIDAPI_DriverSimpleProfile_FindNamedAxes(defs, name)) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Duplicate axes definition name");
    }
    if (defs->num_axes >= SDL_MAX_SINT32 / (int)sizeof(*defs->axes)) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Too many axes definitions");
    }

    if (!HIDAPI_DriverSimpleProfile_CopyAxes(axes, num_axes, &axes_copy, &num_axes)) {
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Out of memory");
    }

    name_copy = SDL_strdup(name);
    if (!name_copy) {
        SDL_free(axes_copy);
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Out of memory");
    }

    new_defs = (SDL_DriverSimpleProfile_JSONNamedAxes *)SDL_realloc(defs->axes, (defs->num_axes + 1) * sizeof(*defs->axes));
    if (!new_defs) {
        SDL_free(name_copy);
        SDL_free(axes_copy);
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Out of memory");
    }
    defs->axes = new_defs;
    defs->axes[defs->num_axes].name = name_copy;
    defs->axes[defs->num_axes].axes = axes_copy;
    defs->axes[defs->num_axes].num_axes = num_axes;
    ++defs->num_axes;
    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONParseDPadDefinitionsObject(SDL_DriverSimpleProfile_JSONParser *parser, SDL_DriverSimpleProfile_JSONDefinitions *defs)
{
    if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, '{')) {
        return false;
    }
    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '}') {
        ++parser->ptr;
        return true;
    }

    for (;;) {
        SDL_HIDAPI_SimpleDPadBinding dpad;
        char key[128];
        char separator;

        if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, key, sizeof(key))) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, ':')) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONParseDPad(parser, &dpad)) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_AddNamedDPad(parser, defs, key, &dpad)) {
            return false;
        }

        separator = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
        if (separator == ',') {
            ++parser->ptr;
            continue;
        }
        if (separator == '}') {
            ++parser->ptr;
            return true;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected ',' or '}'");
    }
}

static bool HIDAPI_DriverSimpleProfile_JSONParseButtonsDefinitionsObject(SDL_DriverSimpleProfile_JSONParser *parser, SDL_DriverSimpleProfile_JSONDefinitions *defs)
{
    if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, '{')) {
        return false;
    }
    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '}') {
        ++parser->ptr;
        return true;
    }

    for (;;) {
        SDL_HIDAPI_SimpleButtonBinding *buttons = NULL;
        int num_buttons = 0;
        char key[128];
        char separator;

        if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, key, sizeof(key))) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, ':')) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONParseButtonsArrayTo(parser, &buttons, &num_buttons)) {
            SDL_free(buttons);
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_AddNamedButtons(parser, defs, key, buttons, num_buttons)) {
            SDL_free(buttons);
            return false;
        }
        SDL_free(buttons);

        separator = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
        if (separator == ',') {
            ++parser->ptr;
            continue;
        }
        if (separator == '}') {
            ++parser->ptr;
            return true;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected ',' or '}'");
    }
}

static bool HIDAPI_DriverSimpleProfile_JSONParseAxesDefinitionsObject(SDL_DriverSimpleProfile_JSONParser *parser, SDL_DriverSimpleProfile_JSONDefinitions *defs)
{
    if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, '{')) {
        return false;
    }
    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '}') {
        ++parser->ptr;
        return true;
    }

    for (;;) {
        SDL_HIDAPI_SimpleAxisBinding *axes = NULL;
        int num_axes = 0;
        char key[128];
        char separator;

        if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, key, sizeof(key))) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, ':')) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONParseAxesArrayTo(parser, &axes, &num_axes)) {
            SDL_free(axes);
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_AddNamedAxes(parser, defs, key, axes, num_axes)) {
            SDL_free(axes);
            return false;
        }
        SDL_free(axes);

        separator = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
        if (separator == ',') {
            ++parser->ptr;
            continue;
        }
        if (separator == '}') {
            ++parser->ptr;
            return true;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected ',' or '}'");
    }
}

static bool HIDAPI_DriverSimpleProfile_JSONResolveProfileReferences(SDL_DriverSimpleProfile_JSONProfile *profile,
                                                                    SDL_DriverSimpleProfile_JSONDefinitions *defs,
                                                                    const char **error)
{
    if (profile->has_dpad && profile->dpad_ref) {
        if (error) {
            *error = "Only one of dpad or dpad_ref can be set";
        }
        return false;
    }
    if (profile->num_buttons > 0 && profile->buttons_ref) {
        if (error) {
            *error = "Only one of buttons or buttons_ref can be set";
        }
        return false;
    }
    if (profile->num_axes > 0 && profile->axes_ref) {
        if (error) {
            *error = "Only one of axes or axes_ref can be set";
        }
        return false;
    }

    if (profile->dpad_ref) {
        SDL_DriverSimpleProfile_JSONNamedDPad *dpad_def = HIDAPI_DriverSimpleProfile_FindNamedDPad(defs, profile->dpad_ref);
        if (!dpad_def) {
            if (error) {
                *error = "Unknown dpad_ref";
            }
            return false;
        }
        profile->dpad = dpad_def->dpad;
        profile->has_dpad = true;
    }

    if (profile->buttons_ref) {
        SDL_DriverSimpleProfile_JSONNamedButtons *buttons_def = HIDAPI_DriverSimpleProfile_FindNamedButtons(defs, profile->buttons_ref);
        if (!buttons_def) {
            if (error) {
                *error = "Unknown buttons_ref";
            }
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_CopyButtons(buttons_def->buttons, buttons_def->num_buttons, &profile->buttons, &profile->num_buttons)) {
            if (error) {
                *error = "Out of memory";
            }
            return false;
        }
    }

    if (profile->axes_ref) {
        SDL_DriverSimpleProfile_JSONNamedAxes *axes_def = HIDAPI_DriverSimpleProfile_FindNamedAxes(defs, profile->axes_ref);
        if (!axes_def) {
            if (error) {
                *error = "Unknown axes_ref";
            }
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_CopyAxes(axes_def->axes, axes_def->num_axes, &profile->axes, &profile->num_axes)) {
            if (error) {
                *error = "Out of memory";
            }
            return false;
        }
    }

    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONParseProfileField(SDL_DriverSimpleProfile_JSONParser *parser, const char *key, SDL_DriverSimpleProfile_JSONProfile *profile)
{
    if (SDL_strcasecmp(key, "vendor_id") == 0) {
        int value = 0;
        if (!HIDAPI_DriverSimpleProfile_JSONParseInt(parser, &value)) {
            return false;
        }
        if (value < 0 || value > 0xffff) {
            return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "vendor_id out of range");
        }
        profile->has_vendor_id = true;
        profile->vendor_id = (Uint16)value;
        return true;
    }
    if (SDL_strcasecmp(key, "product_id") == 0) {
        int value = 0;
        if (!HIDAPI_DriverSimpleProfile_JSONParseInt(parser, &value)) {
            return false;
        }
        if (value < 0 || value > 0xffff) {
            return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "product_id out of range");
        }
        profile->has_product_id = true;
        profile->product_id = (Uint16)value;
        return true;
    }
    if (SDL_strcasecmp(key, "allow_swapped_vid_pid") == 0) {
        bool value = false;
        if (!HIDAPI_DriverSimpleProfile_JSONParseBool(parser, &value)) {
            return false;
        }
        profile->has_allow_swapped_vid_pid = true;
        profile->allow_swapped_vid_pid = value;
        return true;
    }
    if (SDL_strcasecmp(key, "collection") == 0) {
        int value = 0;
        if (!HIDAPI_DriverSimpleProfile_JSONParseInt(parser, &value)) {
            return false;
        }
        profile->has_collection = true;
        profile->collection = value;
        return true;
    }
    if (SDL_strcasecmp(key, "name") == 0) {
        return HIDAPI_DriverSimpleProfile_JSONParseAllocatedString(parser, &profile->name);
    }
    if (SDL_strcasecmp(key, "mapping_string_suffix") == 0) {
        if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '"') {
            return HIDAPI_DriverSimpleProfile_JSONParseAllocatedString(parser, &profile->mapping_string_suffix);
        } else {
            char token[16];
            if (!HIDAPI_DriverSimpleProfile_JSONParseToken(parser, token, sizeof(token))) {
                return false;
            }
            if (SDL_strcasecmp(token, "null") == 0) {
                SDL_free(profile->mapping_string_suffix);
                profile->mapping_string_suffix = NULL;
                return true;
            }
            return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "mapping_string_suffix must be string or null");
        }
    }
    if (SDL_strcasecmp(key, "report_size_min") == 0) {
        int value = 0;
        if (!HIDAPI_DriverSimpleProfile_JSONParseInt(parser, &value)) {
            return false;
        }
        if (value < 1 || value > 0xff) {
            return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "report_size_min out of range");
        }
        profile->has_report_size_min = true;
        profile->report_size_min = (Uint8)value;
        return true;
    }
    if (SDL_strcasecmp(key, "dpad") == 0) {
        if (!HIDAPI_DriverSimpleProfile_JSONParseDPad(parser, &profile->dpad)) {
            return false;
        }
        profile->has_dpad = true;
        return true;
    }
    if (SDL_strcasecmp(key, "dpad_ref") == 0) {
        return HIDAPI_DriverSimpleProfile_JSONParseAllocatedString(parser, &profile->dpad_ref);
    }
    if (SDL_strcasecmp(key, "buttons") == 0) {
        return HIDAPI_DriverSimpleProfile_JSONParseButtonsArray(parser, profile);
    }
    if (SDL_strcasecmp(key, "buttons_ref") == 0) {
        return HIDAPI_DriverSimpleProfile_JSONParseAllocatedString(parser, &profile->buttons_ref);
    }
    if (SDL_strcasecmp(key, "axes") == 0) {
        return HIDAPI_DriverSimpleProfile_JSONParseAxesArray(parser, profile);
    }
    if (SDL_strcasecmp(key, "axes_ref") == 0) {
        return HIDAPI_DriverSimpleProfile_JSONParseAllocatedString(parser, &profile->axes_ref);
    }

    return HIDAPI_DriverSimpleProfile_JSONSkipValue(parser, 0);
}

static bool HIDAPI_DriverSimpleProfile_JSONParseProfileObject(SDL_DriverSimpleProfile_JSONParser *parser, SDL_DriverSimpleProfile_JSONProfile *profile)
{
    if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, '{')) {
        return false;
    }

    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == '}') {
        ++parser->ptr;
        return true;
    }

    for (;;) {
        char key[64];
        char separator;

        if (!HIDAPI_DriverSimpleProfile_JSONParseString(parser, key, sizeof(key))) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, ':')) {
            return false;
        }
        if (!HIDAPI_DriverSimpleProfile_JSONParseProfileField(parser, key, profile)) {
            return false;
        }

        separator = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
        if (separator == ',') {
            ++parser->ptr;
            continue;
        }
        if (separator == '}') {
            ++parser->ptr;
            break;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected ',' or '}' in profile object");
    }

    return true;
}

static bool HIDAPI_Simple_ProfileMatchesVIDPID(const SDL_HIDAPI_SimpleDeviceProfile *profile, Uint16 vendor_id, Uint16 product_id)
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

static bool HIDAPI_DriverSimpleProfile_JSONProfileIsComplete(const SDL_DriverSimpleProfile_JSONProfile *profile)
{
    return (profile->has_vendor_id &&
            profile->has_product_id &&
            profile->name &&
            profile->has_report_size_min &&
            profile->num_buttons > 0 &&
            profile->num_axes > 0);
}

static bool HIDAPI_DriverSimpleProfile_AddJSONProfile(SDL_DriverSimpleProfile_JSONProfile *json_profile, SDL_HIDAPI_SimpleLoadedProfile **profiles, int *num_profiles)
{
    SDL_HIDAPI_SimpleLoadedProfile *new_profiles;
    SDL_HIDAPI_SimpleLoadedProfile *profile;

    if (!HIDAPI_DriverSimpleProfile_JSONProfileIsComplete(json_profile)) {
        return false;
    }

    if (*num_profiles >= SDL_MAX_SINT32 / (int)sizeof(**profiles)) {
        return false;
    }

    new_profiles = (SDL_HIDAPI_SimpleLoadedProfile *)SDL_realloc(*profiles, (*num_profiles + 1) * sizeof(**profiles));
    if (!new_profiles) {
        return false;
    }
    *profiles = new_profiles;
    profile = &new_profiles[*num_profiles];
    SDL_zero(*profile);

    profile->buttons = json_profile->buttons;
    profile->num_buttons = json_profile->num_buttons;
    json_profile->buttons = NULL;
    json_profile->num_buttons = 0;

    profile->axes = json_profile->axes;
    profile->num_axes = json_profile->num_axes;
    json_profile->axes = NULL;
    json_profile->num_axes = 0;

    profile->name = json_profile->name;
    json_profile->name = NULL;

    profile->mapping_string_suffix = json_profile->mapping_string_suffix;
    json_profile->mapping_string_suffix = NULL;

    profile->layout.report_size_min = json_profile->report_size_min;
    profile->layout.dpad = json_profile->has_dpad ? json_profile->dpad : (SDL_HIDAPI_SimpleDPadBinding){ 0, 0, 0, 0, 0 };
    profile->layout.buttons = profile->buttons;
    profile->layout.num_buttons = profile->num_buttons;
    profile->layout.axes = profile->axes;
    profile->layout.num_axes = profile->num_axes;

    profile->profile.vendor_id = json_profile->vendor_id;
    profile->profile.product_id = json_profile->product_id;
    profile->profile.allow_swapped_vid_pid = json_profile->has_allow_swapped_vid_pid ? json_profile->allow_swapped_vid_pid : false;
    profile->profile.collection = json_profile->has_collection ? json_profile->collection : 0;
    profile->profile.name = profile->name;
    profile->profile.mapping_string_suffix = profile->mapping_string_suffix;
    profile->profile.layout = &profile->layout;

    ++(*num_profiles);
    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONParseProfilesArray(SDL_DriverSimpleProfile_JSONParser *parser,
                                                              SDL_HIDAPI_SimpleLoadedProfile **profiles,
                                                              int *num_profiles,
                                                              SDL_DriverSimpleProfile_JSONDefinitions *defs)
{
    if (!HIDAPI_DriverSimpleProfile_JSONConsume(parser, '[')) {
        return false;
    }

    if (HIDAPI_DriverSimpleProfile_JSONPeek(parser) == ']') {
        ++parser->ptr;
        return true;
    }

    for (;;) {
        SDL_DriverSimpleProfile_JSONProfile candidate;
        char separator;

        SDL_zero(candidate);

        if (!HIDAPI_DriverSimpleProfile_JSONParseProfileObject(parser, &candidate)) {
            HIDAPI_DriverSimpleProfile_JSONProfileReset(&candidate);
            return false;
        }
        {
            const char *resolve_error = NULL;
            if (!HIDAPI_DriverSimpleProfile_JSONResolveProfileReferences(&candidate, defs, &resolve_error)) {
                HIDAPI_DriverSimpleProfile_JSONProfileReset(&candidate);
                return HIDAPI_DriverSimpleProfile_JSONSetError(parser, resolve_error ? resolve_error : "Failed to resolve profile references");
            }
        }

        if (!HIDAPI_DriverSimpleProfile_AddJSONProfile(&candidate, profiles, num_profiles)) {
            HIDAPI_DriverSimpleProfile_JSONProfileReset(&candidate);
            return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Profile is invalid or out of memory");
        }
        HIDAPI_DriverSimpleProfile_JSONProfileReset(&candidate);

        separator = HIDAPI_DriverSimpleProfile_JSONPeek(parser);
        if (separator == ',') {
            ++parser->ptr;
            continue;
        }
        if (separator == ']') {
            ++parser->ptr;
            break;
        }
        return HIDAPI_DriverSimpleProfile_JSONSetError(parser, "Expected ',' or ']'");
    }

    return true;
}

static bool HIDAPI_DriverSimpleProfile_JSONLoadProfilesFromText(const char *json_text,
                                                                SDL_HIDAPI_SimpleLoadedProfile **profiles,
                                                                int *num_profiles,
                                                                const char **parse_error,
                                                                size_t *error_offset)
{
    SDL_DriverSimpleProfile_JSONParser parser;
    SDL_DriverSimpleProfile_JSONProfile direct_profile;
    SDL_DriverSimpleProfile_JSONDefinitions defs;
    bool has_profiles_array = false;
    char root_type;

    SDL_zero(parser);
    SDL_zero(direct_profile);
    SDL_zero(defs);
    parser.start = json_text;
    parser.ptr = json_text;
    parser.end = json_text ? (json_text + SDL_strlen(json_text)) : json_text;
    parser.error = NULL;

    if (!json_text || !*json_text) {
        if (parse_error) {
            *parse_error = "Empty JSON";
        }
        if (error_offset) {
            *error_offset = 0;
        }
        return false;
    }

    root_type = HIDAPI_DriverSimpleProfile_JSONPeek(&parser);
    if (root_type == '[') {
        if (!HIDAPI_DriverSimpleProfile_JSONParseProfilesArray(&parser, profiles, num_profiles, &defs)) {
            goto parse_failed;
        }
        HIDAPI_DriverSimpleProfile_JSONDefinitionsReset(&defs);
        return true;
    }

    if (root_type == '{') {
        if (!HIDAPI_DriverSimpleProfile_JSONConsume(&parser, '{')) {
            goto parse_failed;
        }

        if (HIDAPI_DriverSimpleProfile_JSONPeek(&parser) == '}') {
            ++parser.ptr;
            return false;
        }

        for (;;) {
            char key[64];
            char separator;

            if (!HIDAPI_DriverSimpleProfile_JSONParseString(&parser, key, sizeof(key))) {
                goto parse_failed;
            }
            if (!HIDAPI_DriverSimpleProfile_JSONConsume(&parser, ':')) {
                goto parse_failed;
            }

            if (SDL_strcasecmp(key, "profiles") == 0) {
                if (!HIDAPI_DriverSimpleProfile_JSONParseProfilesArray(&parser, profiles, num_profiles, &defs)) {
                    goto parse_failed;
                }
                has_profiles_array = true;
            } else if (SDL_strcasecmp(key, "dpad_defs") == 0) {
                if (has_profiles_array) {
                    parser.error = "dpad_defs must appear before profiles";
                    goto parse_failed;
                }
                if (!HIDAPI_DriverSimpleProfile_JSONParseDPadDefinitionsObject(&parser, &defs)) {
                    goto parse_failed;
                }
            } else if (SDL_strcasecmp(key, "buttons_defs") == 0) {
                if (has_profiles_array) {
                    parser.error = "buttons_defs must appear before profiles";
                    goto parse_failed;
                }
                if (!HIDAPI_DriverSimpleProfile_JSONParseButtonsDefinitionsObject(&parser, &defs)) {
                    goto parse_failed;
                }
            } else if (SDL_strcasecmp(key, "axes_defs") == 0) {
                if (has_profiles_array) {
                    parser.error = "axes_defs must appear before profiles";
                    goto parse_failed;
                }
                if (!HIDAPI_DriverSimpleProfile_JSONParseAxesDefinitionsObject(&parser, &defs)) {
                    goto parse_failed;
                }
            } else {
                if (!HIDAPI_DriverSimpleProfile_JSONParseProfileField(&parser, key, &direct_profile)) {
                    goto parse_failed;
                }
            }

            separator = HIDAPI_DriverSimpleProfile_JSONPeek(&parser);
            if (separator == ',') {
                ++parser.ptr;
                continue;
            }
            if (separator == '}') {
                ++parser.ptr;
                break;
            }
            parser.error = "Expected ',' or '}' at root";
            goto parse_failed;
        }

        if (!has_profiles_array) {
            const char *resolve_error = NULL;
            if (!HIDAPI_DriverSimpleProfile_JSONResolveProfileReferences(&direct_profile, &defs, &resolve_error)) {
                parser.error = resolve_error ? resolve_error : "Failed to resolve root profile references";
                goto parse_failed;
            }
            if (!HIDAPI_DriverSimpleProfile_AddJSONProfile(&direct_profile, profiles, num_profiles)) {
                parser.error = "Root profile is invalid or out of memory";
                goto parse_failed;
            }
        }

        HIDAPI_DriverSimpleProfile_JSONProfileReset(&direct_profile);
        HIDAPI_DriverSimpleProfile_JSONDefinitionsReset(&defs);
        return true;
    }

    parser.error = "JSON must start with '{' or '['";

parse_failed:
    HIDAPI_DriverSimpleProfile_JSONProfileReset(&direct_profile);
    HIDAPI_DriverSimpleProfile_JSONDefinitionsReset(&defs);
    if (parse_error) {
        *parse_error = parser.error ? parser.error : "Invalid JSON";
    }
    if (error_offset) {
        *error_offset = (size_t)(parser.ptr - parser.start);
    }
    return false;
}

static const char *HIDAPI_DriverSimpleProfile_SkipLeadingSpace(const char *value)
{
    while (value && *value && SDL_isspace((unsigned char)*value)) {
        ++value;
    }
    return value;
}

static const char *HIDAPI_DriverSimpleProfile_GetConfigJSONPath(bool *from_hint)
{
    const char *hint = SDL_GetHint(SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE_CONFIG);
    const char *trimmed_hint = HIDAPI_DriverSimpleProfile_SkipLeadingSpace(hint);

    if (trimmed_hint && *trimmed_hint) {
        if (from_hint) {
            *from_hint = true;
        }
        return trimmed_hint;
    }

    if (from_hint) {
        *from_hint = false;
    }
    return SDL_HIDAPI_SIMPLE_PROFILE_DEFAULT_CONFIG_DIR;
}

static bool HIDAPI_DriverSimpleProfile_HasJSONExtension(const char *filename)
{
    const char *ext = SDL_strrchr(filename, '.');
    return (ext && SDL_strcasecmp(ext, ".json") == 0);
}

static void HIDAPI_DriverSimpleProfile_FreeLoadedProfiles(SDL_HIDAPI_SimpleLoadedProfile *profiles, int num_profiles)
{
    int i;
    for (i = 0; i < num_profiles; ++i) {
        SDL_HIDAPI_SimpleLoadedProfile *profile = &profiles[i];
        SDL_free(profile->buttons);
        SDL_free(profile->axes);
        SDL_free(profile->name);
        SDL_free(profile->mapping_string_suffix);
    }
    SDL_free(profiles);
}

typedef struct
{
    SDL_HIDAPI_SimpleLoadedProfile *profiles;
    int num_profiles;
    bool ok;
} SDL_DriverSimpleProfile_ConfigDirectoryLoadContext;

static SDL_EnumerationResult SDLCALL HIDAPI_DriverSimpleProfile_LoadConfigDirectoryCallback(void *userdata, const char *dirname, const char *fname)
{
    SDL_DriverSimpleProfile_ConfigDirectoryLoadContext *ctx = (SDL_DriverSimpleProfile_ConfigDirectoryLoadContext *)userdata;
    const char *parse_error = NULL;
    char *json_data = NULL;
    char *fullpath = NULL;
    size_t parse_error_offset = 0;

    if (!HIDAPI_DriverSimpleProfile_HasJSONExtension(fname)) {
        return SDL_ENUM_CONTINUE;
    }

    if (SDL_asprintf(&fullpath, "%s/%s", dirname, fname) < 0 || !fullpath) {
        return SDL_ENUM_FAILURE;
    }

    json_data = (char *)SDL_LoadFile(fullpath, NULL);
    if (!json_data) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                    "Failed to load simple profile JSON config file '%s': %s",
                    fullpath, SDL_GetError());
        SDL_free(fullpath);
        ctx->ok = false;
        return SDL_ENUM_FAILURE;
    }

    if (!HIDAPI_DriverSimpleProfile_JSONLoadProfilesFromText(json_data, &ctx->profiles, &ctx->num_profiles, &parse_error, &parse_error_offset)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                    "Failed to parse simple profile JSON config file '%s' at byte %u: %s",
                    fullpath,
                    (unsigned int)parse_error_offset,
                    parse_error ? parse_error : "Invalid JSON");
        SDL_free(json_data);
        SDL_free(fullpath);
        ctx->ok = false;
        return SDL_ENUM_FAILURE;
    }

    SDL_free(json_data);
    SDL_free(fullpath);
    return SDL_ENUM_CONTINUE;
}

static bool HIDAPI_DriverSimpleProfile_LoadProfilesFromDirectory(SDL_HIDAPI_SimpleLoadedProfile **profiles, int *num_profiles)
{
    const char *directory_path;
    SDL_PathInfo info;
    bool path_from_hint = false;

    directory_path = HIDAPI_DriverSimpleProfile_GetConfigJSONPath(&path_from_hint);
    if (!directory_path || !*directory_path) {
        return false;
    }

    directory_path = HIDAPI_DriverSimpleProfile_SkipLeadingSpace(directory_path);
    if (!directory_path || !*directory_path) {
        if (path_from_hint) {
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                        "Simple profile JSON config hint is set but the path is empty");
        }
        return false;
    }

    if (!SDL_GetPathInfo(directory_path, &info)) {
        if (path_from_hint) {
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                        "Simple profile JSON config path '%s' is not accessible: %s",
                        directory_path, SDL_GetError());
        }
        return false;
    }

    if (info.type != SDL_PATHTYPE_DIRECTORY) {
        if (path_from_hint) {
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                        "Simple profile JSON config path '%s' is not a directory",
                        directory_path);
        }
        return false;
    }

    {
        SDL_DriverSimpleProfile_ConfigDirectoryLoadContext directory_context;

        SDL_zero(directory_context);
        directory_context.ok = true;

        if (!SDL_EnumerateDirectory(directory_path, HIDAPI_DriverSimpleProfile_LoadConfigDirectoryCallback, &directory_context) && path_from_hint) {
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                        "Failed to enumerate simple profile JSON config directory '%s': %s",
                        directory_path, SDL_GetError());
        }
        if (!directory_context.ok) {
            HIDAPI_DriverSimpleProfile_FreeLoadedProfiles(directory_context.profiles, directory_context.num_profiles);
            return false;
        }
        *profiles = directory_context.profiles;
        *num_profiles = directory_context.num_profiles;
        return (*num_profiles > 0);
    }
}

static bool HIDAPI_Simple_EnsureProfilesLoaded(void)
{
    if (SDL_hidapi_simple_profiles_initialized) {
        return (SDL_hidapi_simple_num_loaded_profiles > 0);
    }

    SDL_hidapi_simple_profiles_initialized = true;
    if (!HIDAPI_DriverSimpleProfile_LoadProfilesFromDirectory(&SDL_hidapi_simple_loaded_profiles, &SDL_hidapi_simple_num_loaded_profiles)) {
        SDL_hidapi_simple_loaded_profiles = NULL;
        SDL_hidapi_simple_num_loaded_profiles = 0;
        return false;
    }
    return true;
}

const SDL_HIDAPI_SimpleDeviceProfile *HIDAPI_Simple_GetDeviceProfile(Uint16 vendor_id, Uint16 product_id)
{
    int i;

    if (!HIDAPI_Simple_EnsureProfilesLoaded()) {
        return NULL;
    }

    for (i = 0; i < SDL_hidapi_simple_num_loaded_profiles; ++i) {
        const SDL_HIDAPI_SimpleDeviceProfile *profile = &SDL_hidapi_simple_loaded_profiles[i].profile;
        if (HIDAPI_Simple_ProfileMatchesVIDPID(profile, vendor_id, product_id)) {
            return profile;
        }
    }
    return NULL;
}

bool HIDAPI_Simple_IsSupportedVIDPID(Uint16 vendor_id, Uint16 product_id)
{
    return (HIDAPI_Simple_GetDeviceProfile(vendor_id, product_id) != NULL);
}

const char *HIDAPI_Simple_GetMappingStringSuffix(Uint16 vendor_id, Uint16 product_id)
{
    const SDL_HIDAPI_SimpleDeviceProfile *profile = HIDAPI_Simple_GetDeviceProfile(vendor_id, product_id);
    if (!profile) {
        return NULL;
    }
    return profile->mapping_string_suffix;
}

static void HIDAPI_DriverSimpleProfile_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE, callback, userdata);
}

static void HIDAPI_DriverSimpleProfile_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE, callback, userdata);
}

static bool HIDAPI_DriverSimpleProfile_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static bool HIDAPI_DriverSimpleProfile_PathHasCollection(const char *path, int collection)
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

static bool HIDAPI_DriverSimpleProfile_PathHasAnyCollectionMarker(const char *path)
{
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)
    const char *p;

    if (!path) {
        return false;
    }

    p = path;
    while ((p = SDL_strcasestr(p, "col")) != NULL) {
        if (p[3] && p[4] &&
            SDL_isdigit((unsigned char)p[3]) && SDL_isdigit((unsigned char)p[4])) {
            return true;
        }
        ++p;
    }
    return false;
#else
    (void)path;
    return false;
#endif
}

static bool HIDAPI_DriverSimpleProfile_IsExpectedCollection(SDL_HIDAPI_Device *device, const SDL_HIDAPI_SimpleDeviceProfile *profile)
{
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)
    if (!profile || profile->collection <= 0) {
        return true;
    }
    if (!device || !device->path) {
        return true;
    }
    if (HIDAPI_DriverSimpleProfile_PathHasCollection(device->path, profile->collection)) {
        return true;
    }
    if (!HIDAPI_DriverSimpleProfile_PathHasAnyCollectionMarker(device->path)) {
        // Unknown path format; don't reject device based only on collection hint.
        return true;
    }
    return false;
#else
    (void)device;
    (void)profile;
    return true;
#endif
}

static bool HIDAPI_DriverSimpleProfile_IsLikelyXboxProtocolDevice(SDL_GamepadType type, int interface_class, int interface_subclass, int interface_protocol)
{
    const int LIBUSB_CLASS_VENDOR_SPEC = 0xFF;
    const int XB360_IFACE_SUBCLASS = 93;
    const int XB360_IFACE_PROTOCOL = 1;    // Wired
    const int XB360W_IFACE_PROTOCOL = 129; // Wireless
    const int XBONE_IFACE_SUBCLASS = 71;
    const int XBONE_IFACE_PROTOCOL = 208;

    if (type == SDL_GAMEPAD_TYPE_XBOX360 || type == SDL_GAMEPAD_TYPE_XBOXONE) {
        return true;
    }

    if (interface_class == LIBUSB_CLASS_VENDOR_SPEC &&
        interface_subclass == XB360_IFACE_SUBCLASS &&
        (interface_protocol == XB360_IFACE_PROTOCOL || interface_protocol == XB360W_IFACE_PROTOCOL)) {
        return true;
    }

    if (interface_class == LIBUSB_CLASS_VENDOR_SPEC &&
        interface_subclass == XBONE_IFACE_SUBCLASS &&
        interface_protocol == XBONE_IFACE_PROTOCOL) {
        return true;
    }

    return false;
}

static bool HIDAPI_DriverSimpleProfile_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    const SDL_HIDAPI_SimpleDeviceProfile *profile = HIDAPI_Simple_GetDeviceProfile(vendor_id, product_id);
    (void)name;
    (void)version;
    (void)interface_number;

    if (!profile) {
        return false;
    }

    if (HIDAPI_DriverSimpleProfile_IsLikelyXboxProtocolDevice(type, interface_class, interface_subclass, interface_protocol)) {
        return false;
    }

    if (device && !HIDAPI_DriverSimpleProfile_IsExpectedCollection(device, profile)) {
        return false;
    }

    return true;
}

static bool HIDAPI_DriverSimpleProfile_InitDevice(SDL_HIDAPI_Device *device)
{
    const SDL_HIDAPI_SimpleDeviceProfile *profile = HIDAPI_Simple_GetDeviceProfile(device->vendor_id, device->product_id);
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)SDL_calloc(1, sizeof(*ctx));

    if (!profile) {
        return false;
    }
    if (!ctx) {
        return false;
    }

    ctx->profile = profile;
    device->context = ctx;

    HIDAPI_SetDeviceName(device, ctx->profile->name ? ctx->profile->name : "Generic HID Controller");
    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverSimpleProfile_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverSimpleProfile_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static bool HIDAPI_DriverSimpleProfile_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    const SDL_HIDAPI_SimpleReportLayout *layout = ctx->profile ? ctx->profile->layout : NULL;

    SDL_AssertJoysticksLocked();

    SDL_zeroa(ctx->last_state);
    ctx->last_state_initialized = false;

    joystick->nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;
    joystick->nhats = (layout &&
                       (layout->dpad.up_mask || layout->dpad.down_mask || layout->dpad.left_mask || layout->dpad.right_mask)) ? 1 : 0;

    return true;
}

static bool HIDAPI_DriverSimpleProfile_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSimpleProfile_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverSimpleProfile_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    return 0;
}

static bool HIDAPI_DriverSimpleProfile_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSimpleProfile_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSimpleProfile_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    return SDL_Unsupported();
}

static Uint8 HIDAPI_DriverSimpleProfile_GetHat(const SDL_HIDAPI_SimpleDPadBinding *dpad, const Uint8 *data, int size)
{
    Uint8 dpad_byte;
    bool up, down, left, right;

    if (!dpad || dpad->byte_index >= size) {
        return SDL_HAT_CENTERED;
    }

    dpad_byte = data[dpad->byte_index];
    up = ((dpad_byte & dpad->up_mask) != 0);
    down = ((dpad_byte & dpad->down_mask) != 0);
    left = ((dpad_byte & dpad->left_mask) != 0);
    right = ((dpad_byte & dpad->right_mask) != 0);

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

static Sint16 HIDAPI_DriverSimpleProfile_DecodeAxis(const SDL_HIDAPI_SimpleAxisBinding *binding, Uint8 raw_value)
{
    Sint16 value = 0;

    switch (binding->encoding) {
    case SDL_HIDAPI_AXIS_ENCODING_STICK_8BIT_CENTER_0x80:
        value = (raw_value == 0x80) ? 0 : (Sint16)HIDAPI_RemapVal((float)((int)raw_value - 0x80), -0x80, 0x7f, SDL_MIN_SINT16, SDL_MAX_SINT16);
        break;
    case SDL_HIDAPI_AXIS_ENCODING_TRIGGER_8BIT_0_255:
        value = (Sint16)HIDAPI_RemapVal((float)raw_value, 0.0f, 255.0f, (float)SDL_MIN_SINT16, (float)SDL_MAX_SINT16);
        break;
    default:
        break;
    }

    if (binding->invert) {
        value = (value == SDL_MIN_SINT16) ? SDL_MAX_SINT16 : (Sint16)-value;
    }
    return value;
}

static void HIDAPI_DriverSimpleProfile_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverSimpleProfile_Context *ctx, const Uint8 *data, int size)
{
    const SDL_HIDAPI_SimpleReportLayout *layout = ctx->profile ? ctx->profile->layout : NULL;
    Uint64 timestamp = SDL_GetTicksNS();
    const bool initial = !ctx->last_state_initialized;
    const Uint8 *last = ctx->last_state;
    int i;

    if (!layout || size < layout->report_size_min) {
        return;
    }

    if (joystick->nhats > 0 &&
        layout->dpad.byte_index < size &&
        (initial || last[layout->dpad.byte_index] != data[layout->dpad.byte_index])) {
        SDL_SendJoystickHat(timestamp, joystick, 0, HIDAPI_DriverSimpleProfile_GetHat(&layout->dpad, data, size));
    }

    for (i = 0; i < layout->num_buttons; ++i) {
        const SDL_HIDAPI_SimpleButtonBinding *binding = &layout->buttons[i];
        Uint8 byte_index = binding->byte_index;

        if (byte_index >= size) {
            continue;
        }
        if (initial || last[byte_index] != data[byte_index]) {
            SDL_SendJoystickButton(timestamp, joystick, binding->button, ((data[byte_index] & binding->mask) != 0));
        }
    }

    for (i = 0; i < layout->num_axes; ++i) {
        const SDL_HIDAPI_SimpleAxisBinding *binding = &layout->axes[i];
        Uint8 byte_index = binding->byte_index;

        if (byte_index >= size) {
            continue;
        }
        if (initial || last[byte_index] != data[byte_index]) {
            Sint16 axis = HIDAPI_DriverSimpleProfile_DecodeAxis(binding, data[byte_index]);
            SDL_SendJoystickAxis(timestamp, joystick, binding->axis, axis);
        }
    }

    SDL_zeroa(ctx->last_state);
    SDL_memcpy(ctx->last_state, data, SDL_min(size, (int)sizeof(ctx->last_state)));
    ctx->last_state_initialized = true;
}

static bool HIDAPI_DriverSimpleProfile_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size = 0;

    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromID(device->joysticks[0]);
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_SIMPLE_PROFILE_PROTOCOL
        HIDAPI_DumpPacket("Simple profile packet: size = %d", data, size);
#endif
        if (!joystick) {
            continue;
        }
        HIDAPI_DriverSimpleProfile_HandleStatePacket(joystick, ctx, data, size);
    }

    if (size < 0 && device->num_joysticks > 0) {
        // Read error, device is disconnected
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return (size >= 0);
}

static void HIDAPI_DriverSimpleProfile_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverSimpleProfile_FreeDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverSimpleProfile_Context *ctx = (SDL_DriverSimpleProfile_Context *)device->context;
    if (ctx) {
        SDL_free(ctx);
        device->context = NULL;
    }
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSimpleProfile = {
    SDL_HINT_JOYSTICK_HIDAPI_SIMPLE_PROFILE,
    true,
    HIDAPI_DriverSimpleProfile_RegisterHints,
    HIDAPI_DriverSimpleProfile_UnregisterHints,
    HIDAPI_DriverSimpleProfile_IsEnabled,
    HIDAPI_DriverSimpleProfile_IsSupportedDevice,
    HIDAPI_DriverSimpleProfile_InitDevice,
    HIDAPI_DriverSimpleProfile_GetDevicePlayerIndex,
    HIDAPI_DriverSimpleProfile_SetDevicePlayerIndex,
    HIDAPI_DriverSimpleProfile_UpdateDevice,
    HIDAPI_DriverSimpleProfile_OpenJoystick,
    HIDAPI_DriverSimpleProfile_RumbleJoystick,
    HIDAPI_DriverSimpleProfile_RumbleJoystickTriggers,
    HIDAPI_DriverSimpleProfile_GetJoystickCapabilities,
    HIDAPI_DriverSimpleProfile_SetJoystickLED,
    HIDAPI_DriverSimpleProfile_SendJoystickEffect,
    HIDAPI_DriverSimpleProfile_SetJoystickSensorsEnabled,
    HIDAPI_DriverSimpleProfile_CloseJoystick,
    HIDAPI_DriverSimpleProfile_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_SIMPLE_PROFILE

#endif // SDL_JOYSTICK_HIDAPI
