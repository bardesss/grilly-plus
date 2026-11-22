#pragma once
#include <stddef.h>
#include <stdint.h>

// Food types for the probe icons ("beef", "pork", ...), shown on the grill screen and in the app.
// Pure (no Arduino), so it is tested on the host.
namespace food {

const int FOOD_TYPE_COUNT = 9;

// The keys in icon order: beef, lamb, pork, poultry, fish, sausage, vegetables, pit, other
extern const char* const FOOD_TYPES[FOOD_TYPE_COUNT];

// The index of a type key, -1 for "" (no type) or an unknown key
int type_index(const char* key);

// Whether a key can be saved: "" or one of FOOD_TYPES
bool type_valid(const char* key);

// XBM bitmaps (least significant bit first), for u8g2's drawXBM. nullptr for -1.
// 16x16 for the one-probe screen, 10x10 (2 bytes a row) for the screens with more probes.
const uint8_t* icon_16(int index);
const uint8_t* icon_10(int index);

}  // namespace food
