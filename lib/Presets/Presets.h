#pragma once
#include <stddef.h>
#include <string>

#include <ArduinoJson.h>

// Food presets of your own, stored on the grill so every phone and browser sees the same list, and
// which of the app's built-in presets are hidden. Pure (no Arduino), so it is tested on the host.
namespace presets {

const size_t MAX_PRESETS = 30;
const int    MAX_REST    = 180;     // minutes
const size_t NAME_LENGTH    = 24;     // bytes
const size_t GROUP_LENGTH   = 20;
const size_t MAX_HIDDEN  = 30;
const size_t ID_LENGTH      = 24;

// Checks a presets array and writes a clean copy (only the known keys) into out.
// A preset: {"name", "group" (optional), "type" (optional food type), "c", "f",
//            "min_c" and "min_f" (optional, both or neither, for a range)}
// Returns "" when it is fine, otherwise why not.
std::string clean_presets(JsonVariantConst in, JsonArray out);

// Checks the ids of hidden built-in presets (letters, digits and -) and copies them into out
std::string clean_hidden(JsonVariantConst in, JsonArray out);

}  // namespace presets
