#include <string.h>
#include "Timers.h"

namespace timers {

const char* check_start(const char* name, long minutes){
    if(name != nullptr && strlen(name) > NAME_LENGTH){ return "name should be at most 24 characters"; }
    if(minutes < MIN_MINUTES || minutes > MAX_MINUTES){ return "minutes should be 1 to 1440"; }
    return nullptr;
}

void copy_name(char* out, const char* name){
    if(name == nullptr || name[0] == '\0'){ name = "Timer"; }
    size_t length = strlen(name);
    if(length > NAME_LENGTH){
        length = NAME_LENGTH;
        while(length > 0 && ((unsigned char)name[length] & 0xC0) == 0x80){ length--; }
    }
    memcpy(out, name, length);
    out[length] = '\0';
}

uint8_t TimerList::new_id(){
    for(;;){
        uint8_t id = next_id_;
        next_id_ = next_id_ == 255 ? 1 : next_id_ + 1;
        bool used = false;
        for(const Timer& timer : slots_){ if(timer.id == id){ used = true; } }
        if(!used){ return id; }
    }
}

int TimerList::start(const char* name, uint32_t length_ms, uint8_t probe, uint32_t now_ms){
    for(size_t i = 0; i < MAX_TIMERS; i++){
        if(slots_[i].id != 0){ continue; }
        Timer timer;
        timer.id         = new_id();
        copy_name(timer.name, name);
        timer.length_ms  = length_ms;
        timer.started_ms = now_ms;
        timer.probe      = probe;
        slots_[i] = timer;
        return (int)i;
    }
    return -1;
}

bool TimerList::cancel(uint8_t id){
    if(id == 0){ return false; }
    for(Timer& timer : slots_){
        if(timer.id == id){ timer = Timer(); return true; }
    }
    return false;
}

uint8_t TimerList::update(uint32_t now_ms){
    uint8_t finished = 0;
    for(size_t i = 0; i < MAX_TIMERS; i++){
        Timer& timer = slots_[i];
        if(timer.id == 0){ continue; }
        if(!timer.done && now_ms - timer.started_ms >= timer.length_ms){
            timer.done    = true;
            timer.done_ms = now_ms;
            finished |= (uint8_t)(1 << i);
        } else if(timer.done && now_ms - timer.done_ms >= DONE_TIMEOUT_MS){
            timer = Timer();
        }
    }
    return finished;
}

void TimerList::clear_done(){
    for(Timer& timer : slots_){
        if(timer.id != 0 && timer.done){ timer = Timer(); }
    }
}

uint8_t TimerList::done_mask() const {
    uint8_t mask = 0;
    for(size_t i = 0; i < MAX_TIMERS; i++){
        if(slots_[i].id != 0 && slots_[i].done){ mask |= (uint8_t)(1 << i); }
    }
    return mask;
}

int TimerList::first_running(uint32_t now_ms) const {
    int best = -1;
    for(size_t i = 0; i < MAX_TIMERS; i++){
        if(slots_[i].id == 0 || slots_[i].done){ continue; }
        if(best < 0 || remaining_ms((int)i, now_ms) < remaining_ms(best, now_ms)){ best = (int)i; }
    }
    return best;
}

uint32_t TimerList::remaining_ms(int slot, uint32_t now_ms) const {
    const Timer& timer = slots_[slot];
    if(timer.id == 0 || timer.done){ return 0; }
    uint32_t passed = now_ms - timer.started_ms;
    return passed >= timer.length_ms ? 0 : timer.length_ms - passed;
}

bool TimerList::has_rest_for(uint8_t probe) const {
    if(probe == 0){ return false; }
    for(const Timer& timer : slots_){
        if(timer.id != 0 && !timer.done && timer.probe == probe){ return true; }
    }
    return false;
}

size_t TimerList::count() const {
    size_t n = 0;
    for(const Timer& timer : slots_){ if(timer.id != 0){ n++; } }
    return n;
}

size_t TimerList::export_running(TimerState* out, uint32_t now_ms) const {
    size_t n = 0;
    for(size_t i = 0; i < MAX_TIMERS; i++){
        const Timer& timer = slots_[i];
        if(timer.id == 0 || timer.done){ continue; }
        TimerState& state = out[n++];
        memset(&state, 0, sizeof(state));
        memcpy(state.name, timer.name, sizeof(state.name));
        state.length_ms    = timer.length_ms;
        state.remaining_ms = remaining_ms((int)i, now_ms);
        state.probe        = timer.probe;
    }
    return n;
}

int TimerList::restore(const TimerState& state, uint32_t now_ms, uint32_t gap_ms){
    uint32_t remaining = state.remaining_ms < state.length_ms ? state.remaining_ms : state.length_ms;
    char name[NAME_LENGTH + 1];
    memcpy(name, state.name, NAME_LENGTH);
    name[NAME_LENGTH] = '\0';
    // A rest timer the probe started again before this one came back: the saved one knows the real time left
    if(state.probe != 0){
        for(Timer& timer : slots_){
            if(timer.id != 0 && !timer.done && timer.probe == state.probe){ timer = Timer(); }
        }
    }
    // Started as long ago as it had run, plus the time the grill was off
    return start(name, state.length_ms, state.probe, now_ms - (state.length_ms - remaining) - gap_ms);
}

}  // namespace timers
