#include "AdminReset.h"
#include <string.h>

AdminReset::Start AdminReset::start(uint32_t now_ms, uint32_t random){
    if(active(now_ms)){ return Start::ALREADY_ACTIVE; }
    if(failed_codes_ >= MAX_FAILED_CODES){ return Start::LOCKED; }
    if(started_ && now_ms - started_ms_ < RESTART_MS){ return Start::TOO_SOON; }

    uint32_t number = random % 1000000UL;
    for(int i = 5; i >= 0; i--){
        code_[i] = '0' + (number % 10);
        number /= 10;
    }
    code_[6]    = '\0';
    active_     = true;
    started_    = true;
    started_ms_ = now_ms;
    attempts_   = 0;
    return Start::STARTED;
}

AdminReset::Check AdminReset::check(const char* code, uint32_t now_ms){
    if(!active(now_ms)){ return Check::NOT_ACTIVE; }

    // Compare every character, so the answer time doesn't tell how much of the code was right
    bool same = code != nullptr && strlen(code) == 6;
    unsigned char difference = 0;
    for(int i = 0; i < 6; i++){
        difference |= (unsigned char)((same ? code[i] : 0) ^ code_[i]);
    }
    if(same && difference == 0){
        cancel();
        return Check::OK;
    }

    attempts_++;
    if(attempts_ >= MAX_ATTEMPTS){
        failed_codes_++;
        cancel();
    }
    return Check::WRONG;
}

void AdminReset::cancel(){
    active_  = false;
    code_[0] = '\0';
}

bool AdminReset::active(uint32_t now_ms) const {
    return active_ && now_ms - started_ms_ < VALID_MS;
}

uint32_t AdminReset::seconds_left(uint32_t now_ms) const {
    if(!active(now_ms)){ return 0; }
    return (VALID_MS - (now_ms - started_ms_) + 999) / 1000;
}

const char* AdminReset::code(uint32_t now_ms) const {
    return active(now_ms) ? code_ : "";
}
