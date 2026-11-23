#pragma once
#include "Resume.h"

// Keeps the resume snapshot (lib/Resume) in a file on the spare "spiffs" partition, mounted as
// LittleFS. A save goes to a temporary file that is then renamed, so a power loss during the write
// leaves the previous snapshot intact. Callers serialise access (main.cpp holds its own mutex).
namespace resume_store {
    // Mounts the partition, formatting it on first use. False when it can't be used.
    bool begin();

    bool save(const resume::Snapshot& snapshot);

    // False when there is no snapshot, or it is unreadable or of another version (then it's removed)
    bool load(resume::Snapshot& snapshot);

    void erase();
}
