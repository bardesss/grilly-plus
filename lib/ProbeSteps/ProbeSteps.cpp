#include "ProbeSteps.h"

#include <stdlib.h>
#include <string.h>

namespace steps {

std::string check_action(const char* action){
    const char* message = "A step needs an action of 1 to 16 characters";
    if(action == nullptr){ return message; }
    size_t length = strlen(action);
    if(length == 0 || length > ACTION_LENGTH){ return message; }
    for(const char* p = action; *p; p++){
        if((unsigned char)*p < 0x20 || *p == '|' || *p == ':'){ return message; }
    }
    return "";
}

static bool stored(const StepList* kept, int temperature){
    if(kept == nullptr){ return false; }
    for(size_t i = 0; i < kept->count(); i++){
        if(kept->at(i).temperature == temperature){ return true; }
    }
    return false;
}

std::string check(const Step* list, size_t count, float target, const StepList* kept){
    if(count > MAX_STEPS){ return "At most 3 steps"; }
    for(size_t i = 0; i < count; i++){
        // The action array always has room for its terminator; a full one means it was cut
        if(memchr(list[i].action, 0, sizeof(list[i].action)) == nullptr){ return check_action(nullptr); }
        std::string error = check_action(list[i].action);
        if(!error.empty()){ return error; }
        if(list[i].temperature < 1){ return "A step should be 1 or more degrees"; }
        if(list[i].temperature >= target && !stored(kept, list[i].temperature)){ return "A step should be below the target"; }
    }
    return "";
}

bool StepList::set(const Step* list, size_t count){
    if(count > MAX_STEPS){ count = MAX_STEPS; }
    Step sorted[MAX_STEPS];
    for(size_t i = 0; i < count; i++){
        sorted[i] = list[i];
        sorted[i].action[ACTION_LENGTH] = 0;
        sorted[i].done = false;
    }
    // Insertion sort by temperature, stable for equal ones
    for(size_t i = 1; i < count; i++){
        Step s = sorted[i];
        size_t j = i;
        while(j > 0 && sorted[j - 1].temperature > s.temperature){ sorted[j] = sorted[j - 1]; j--; }
        sorted[j] = s;
    }

    bool changed = count != count_;
    for(size_t i = 0; !changed && i < count; i++){
        changed = sorted[i].temperature != steps_[i].temperature || strcmp(sorted[i].action, steps_[i].action) != 0;
    }
    if(!changed){ return false; }

    for(size_t i = 0; i < count; i++){ steps_[i] = sorted[i]; }
    count_ = count;
    rearm();
    return true;
}

void StepList::rearm(){
    for(size_t i = 0; i < count_; i++){ steps_[i].done = false; }
    armed_ = false;
}

uint8_t StepList::update(bool connected, bool target_mode, float temperature, float target){
    if(!connected){ rearm(); return 0; }
    if(!target_mode){ return 0; }

    if(!armed_){
        armed_ = true;
        for(size_t i = 0; i < count_; i++){
            if(temperature >= steps_[i].temperature){ steps_[i].done = true; }
        }
        return 0;
    }

    uint8_t fired = 0;
    for(size_t i = 0; i < count_; i++){
        if(steps_[i].done || ignored(i, target)){ continue; }
        if(temperature >= steps_[i].temperature){
            steps_[i].done = true;
            fired |= (uint8_t)(1 << i);
        }
    }
    return fired;
}

int StepList::next(float target) const {
    for(size_t i = 0; i < count_; i++){
        if(!steps_[i].done && !ignored(i, target)){ return (int)i; }
    }
    return -1;
}

std::string StepList::serialize() const {
    std::string out;
    for(size_t i = 0; i < count_; i++){
        if(i > 0){ out += "|"; }
        out += std::to_string(steps_[i].temperature) + ":" + steps_[i].action;
    }
    return out;
}

bool StepList::parse(const char* text){
    Step list[MAX_STEPS];
    size_t count = 0;
    bool ok = text != nullptr;

    for(const char* p = text; ok && *p; ){
        if(count == MAX_STEPS){ ok = false; break; }
        const char* end = strchr(p, '|');
        size_t length = end ? (size_t)(end - p) : strlen(p);
        const char* colon = (const char*)memchr(p, ':', length);
        if(colon == nullptr || colon == p){ ok = false; break; }

        char number[8];
        size_t digits = (size_t)(colon - p);
        if(digits >= sizeof(number)){ ok = false; break; }
        memcpy(number, p, digits);
        number[digits] = 0;
        char* number_end = nullptr;
        long temperature = strtol(number, &number_end, 10);
        if(*number_end != 0 || temperature < 1 || temperature > 9999){ ok = false; break; }

        size_t action_length = length - digits - 1;
        if(action_length == 0 || action_length > ACTION_LENGTH){ ok = false; break; }
        memcpy(list[count].action, colon + 1, action_length);
        list[count].action[action_length] = 0;
        if(!check_action(list[count].action).empty()){ ok = false; break; }
        list[count].temperature = (int)temperature;
        list[count].done = false;
        count++;

        if(end == nullptr){ break; }
        p = end + 1;
        if(*p == 0){ ok = false; }    // a trailing '|'
    }

    set(list, ok ? count : 0);
    return ok;
}

void StepAlerts::add(uint8_t probe, uint8_t step, uint32_t now){
    if(probe >= PROBES){ return; }
    alerts_[probe].active = true;
    alerts_[probe].step   = step;
    alerts_[probe].at_ms  = now;
    alerts_[probe].order  = ++counter_;
    page_closed_ = false;
}

int StepAlerts::step_of(uint8_t probe, uint32_t now) const {
    if(probe >= PROBES || !alerts_[probe].active || now - alerts_[probe].at_ms > ALERT_MS){ return -1; }
    return alerts_[probe].step;
}

bool StepAlerts::page(uint32_t now, uint8_t* probe, uint8_t* step, int* more) const {
    if(page_closed_){ return false; }
    int newest = -1, others = 0;
    for(uint8_t i = 0; i < PROBES; i++){
        if(!alerts_[i].active || now - alerts_[i].at_ms > PAGE_MS){ continue; }
        if(newest < 0 || alerts_[i].order > alerts_[newest].order){
            if(newest >= 0){ others++; }
            newest = i;
        } else {
            others++;
        }
    }
    if(newest < 0){ return false; }
    *probe = (uint8_t)newest;
    *step  = alerts_[newest].step;
    *more  = others;
    return true;
}

void StepAlerts::clear(){
    for(uint8_t i = 0; i < PROBES; i++){ alerts_[i].active = false; }
}

void StepAlerts::clear_probe(uint8_t probe){
    if(probe < PROBES){ alerts_[probe].active = false; }
}

}  // namespace steps
