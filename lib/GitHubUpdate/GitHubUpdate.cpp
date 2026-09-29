#include <ctype.h>
#include <string.h>
#include "GitHubUpdate.h"

namespace ghupdate {

namespace {
    const char ASSET_PREFIX[] = "https://github.com/bardesss/grilly-plus/releases/download/";

    // Copies at most size-1 bytes and never ends in the middle of a UTF-8 character
    void copy_text(char* dst, size_t size, const char* src){
        size_t length = strlen(src);
        if(length >= size){
            length = size - 1;
            while(length > 0 && ((unsigned char)src[length] & 0xC0) == 0x80){ length--; }
        }
        memcpy(dst, src, length);
        dst[length] = '\0';
    }
}

Version parse_version(const char* text){
    Version version;
    if(text == nullptr){ return version; }
    const char* p = text;
    if(*p == 'v' || *p == 'V'){ p++; }
    int count = 0;
    while(count < 4){
        if(*p < '0' || *p > '9'){ return Version(); }
        long value = 0;
        while(*p >= '0' && *p <= '9'){
            value = value * 10 + (*p - '0');
            if(value > 100000){ return Version(); }
            p++;
        }
        version.parts[count++] = (int)value;
        if(*p != '.'){ break; }
        p++;
    }
    if(count < 3){ return Version(); }
    if(*p == '-'){ version.suffix = true; }
    else if(*p != '\0'){ return Version(); }
    version.valid = true;
    return version;
}

int compare_versions(const Version& a, const Version& b){
    if(a.valid != b.valid){ return a.valid ? 1 : -1; }
    if(!a.valid){ return 0; }
    for(int i = 0; i < 4; i++){
        if(a.parts[i] != b.parts[i]){ return a.parts[i] < b.parts[i] ? -1 : 1; }
    }
    if(a.suffix != b.suffix){ return a.suffix ? -1 : 1; }
    return 0;
}

bool is_newer(const char* candidate, const char* current){
    Version version = parse_version(candidate);
    return version.valid && compare_versions(version, parse_version(current)) > 0;
}

void release_filter(JsonDocument& filter){
    filter["tag_name"] = true;
    filter["body"]     = true;
    JsonObject asset = filter["assets"].add<JsonObject>();
    asset["name"]                 = true;
    asset["browser_download_url"] = true;
    asset["size"]                 = true;
    asset["digest"]               = true;
}

const char* read_release(JsonVariantConst release, Release& out){
    out = Release();
    const char* tag = release["tag_name"] | "";
    if(!parse_version(tag).valid || strlen(tag) >= sizeof(out.version)){ return "The release has no valid version"; }
    copy_text(out.version, sizeof(out.version), tag);
    copy_text(out.notes, sizeof(out.notes), release["body"] | "");

    for(JsonVariantConst asset : release["assets"].as<JsonArrayConst>()){
        const char* name = asset["name"] | "";
        size_t name_length = strlen(name);
        if(name_length < 8 || strcmp(name + name_length - 8, "-ota.bin") != 0){ continue; }

        const char* url = asset["browser_download_url"] | "";
        if(strncmp(url, ASSET_PREFIX, sizeof(ASSET_PREFIX) - 1) != 0){ return "The update file is not in the Grilly+ repository"; }
        if(strlen(url) >= sizeof(out.url)){ return "The download address is too long"; }

        const char* digest = asset["digest"] | "";
        if(strncmp(digest, "sha256:", 7) != 0 || strlen(digest) != 7 + 64){ return "The release has no checksum for the update file"; }
        for(int i = 0; i < 64; i++){
            char c = (char)tolower((unsigned char)digest[7 + i]);
            if(!isxdigit((unsigned char)c)){ return "The release has no checksum for the update file"; }
            out.sha256[i] = c;
        }
        out.sha256[64] = '\0';

        out.size = asset["size"] | 0;
        if(out.size == 0){ return "The update file is empty"; }
        copy_text(out.url, sizeof(out.url), url);
        return nullptr;
    }
    return "The release has no update file";
}

bool battery_allows_update(int percent, bool charging){
    return charging || percent >= MIN_BATTERY_PERCENT;
}

}
