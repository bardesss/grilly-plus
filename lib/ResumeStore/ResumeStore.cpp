#include "ResumeStore.h"

#include <Arduino.h>
#include <LittleFS.h>

namespace resume_store {

namespace {
    const char* FILE_PATH = "/resume.bin";
    const char* TEMP_PATH = "/resume.tmp";

    bool    mounted = false;
    uint8_t buffer[sizeof(resume::Snapshot) + 64];     // encoded snapshot, too big for a stack
}

bool begin(){
    mounted = LittleFS.begin(true, "/littlefs", 4, "spiffs");
    if(!mounted){ Serial.println("Resume: no file system, a cook won't carry on after a restart"); }
    return mounted;
}

bool save(const resume::Snapshot& snapshot){
    if(!mounted){ return false; }
    size_t size = resume::encode(snapshot, buffer, sizeof(buffer));
    if(size == 0){ return false; }

    File file = LittleFS.open(TEMP_PATH, "w");
    if(!file){ return false; }
    size_t written = file.write(buffer, size);
    file.close();
    if(written != size){
        LittleFS.remove(TEMP_PATH);
        return false;
    }
    LittleFS.remove(FILE_PATH);     // LittleFS.rename() won't replace an existing file
    return LittleFS.rename(TEMP_PATH, FILE_PATH);
}

bool load(resume::Snapshot& snapshot){
    if(!mounted){ return false; }
    // A save that lost power between the remove and the rename left only the temporary file
    if(!LittleFS.exists(FILE_PATH) && LittleFS.exists(TEMP_PATH)){ LittleFS.rename(TEMP_PATH, FILE_PATH); }
    if(!LittleFS.exists(FILE_PATH)){ return false; }

    File file = LittleFS.open(FILE_PATH, "r");
    if(!file){ return false; }
    size_t size = file.read(buffer, sizeof(buffer));
    file.close();
    if(resume::decode(buffer, size, snapshot)){ return true; }

    Serial.println("Resume: the saved cook is unreadable or of another version, removed");
    LittleFS.remove(FILE_PATH);
    return false;
}

void erase(){
    if(!mounted){ return; }
    LittleFS.remove(FILE_PATH);
    LittleFS.remove(TEMP_PATH);
}

}  // namespace resume_store
