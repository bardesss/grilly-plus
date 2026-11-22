#pragma once
#include <stdint.h>
#include <ArduinoJson.h>

// Hardware-free rules for updating from GitHub releases. No Arduino calls, so
// test/test_github_update runs on the host: pio test -e native
namespace ghupdate {

constexpr int MIN_BATTERY_PERCENT = 30;

// A Grilly+ version: yy.mm.dd or yy.mm.dd.N, optionally with a "-label" (a test build)
struct Version {
    bool valid     = false;
    int  parts[4]  = {0, 0, 0, 0};
    bool suffix    = false;         // counts as older than the same numbers without a label
};

Version parse_version(const char* text);
// > 0 when a is newer than b, 0 when equal, < 0 when older. An invalid version is the oldest.
int  compare_versions(const Version& a, const Version& b);
bool is_newer(const char* candidate, const char* current);

// What the firmware keeps of a release
struct Release {
    char     version[24] = "";
    char     url[256]    = "";
    char     sha256[65]  = "";     // lowercase hex
    uint32_t size        = 0;
    char     notes[601]  = "";     // the start of the release notes, cut on a character boundary
};

// Keeps only tag_name, body and assets[].name/browser_download_url/size/digest of the ~10 KB answer
void release_filter(JsonDocument& filter);
// Reads a GitHub "latest release" object. nullptr when usable, otherwise why not.
const char* read_release(JsonVariantConst release, Release& out);

bool battery_allows_update(int percent, bool charging);

}
