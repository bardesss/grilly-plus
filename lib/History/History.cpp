#include <math.h>
#include "History.h"

namespace history {

namespace {
    constexpr int   ETA_WINDOW_SAMPLES    = 60;          // 10 minutes of fine samples
    constexpr int   ETA_MIN_SAMPLES       = 18;          // 3 minutes
    constexpr float ETA_MIN_SLOPE_PER_MIN = 0.05f;
    constexpr int32_t ETA_MAX_S           = 24 * 3600;

    constexpr int     STALL_WINDOW_SAMPLES = 120;        // 20 minutes of fine samples
    constexpr int     STALL_MIN_VALID      = 108;        // 90 % of them, a short unplug is fine
    constexpr int16_t STALL_MAX_SPREAD     = 15;         // tenths: 1.5 degrees
    constexpr int16_t STALL_LOW            = 600;        // tenths: 60 degrees
    constexpr int16_t STALL_HIGH           = 800;        // tenths: 80 degrees
    constexpr float   STALL_TARGET_ABOVE   = 5.0f;       // degrees between the reading and the target

    constexpr int     DROP_WINDOW_SAMPLES  = 7;          // the newest reading and the minute before it
    constexpr int16_t DROP_MIN             = 80;         // tenths: 8 degrees
    constexpr int16_t LID_RECOVERED_WITHIN = 20;         // tenths: back within 2 degrees of the high

    int16_t average(int16_t a, int16_t b){
        if(a == NO_VALUE){ return b; }
        if(b == NO_VALUE){ return a; }
        return (int16_t)(((int32_t)a + b) / 2);
    }
}

int16_t to_tenths(float celcius){
    if(!isfinite(celcius)){ return NO_VALUE; }
    long tenths = lroundf(celcius * 10.0f);
    if(tenths >  32767){ return  32767; }
    if(tenths < -32767){ return -32767; }
    return (int16_t)tenths;
}

// Field by field: `*this = ProbeHistory()` would put a 600 byte temporary on the caller's stack
void ProbeHistory::clear(){
    fine_head_ = 0; fine_count_ = 0; fine_newest_s_ = 0;
    coarse_count_ = 0; coarse_interval_s_ = COARSE_START_INTERVAL_S; coarse_newest_s_ = 0;
    slot_sum_ = 0; slot_valid_ = 0; slot_samples_ = 0;
    plugged_in_ = false; unplugged_since_s_ = 0;
}

void ProbeHistory::add(uint32_t now_s, bool connected, float celcius){
    if(connected){
        plugged_in_ = true;
        add_value(now_s, to_tenths(celcius));
        return;
    }

    bool empty = fine_count_ == 0 && coarse_count_ == 0 && slot_samples_ == 0;
    if(empty){ return; }

    if(plugged_in_){
        plugged_in_        = false;
        unplugged_since_s_ = now_s;
    }
    if(now_s - unplugged_since_s_ > RESET_AFTER_UNPLUGGED_S){
        clear();
        return;
    }
    add_value(now_s, NO_VALUE);
}

void ProbeHistory::add_gap(uint32_t now_s, uint32_t seconds){
    bool empty = fine_count_ == 0 && coarse_count_ == 0 && slot_samples_ == 0;
    if(empty){ return; }
    uint32_t samples = (seconds < MAX_GAP_S ? seconds : MAX_GAP_S) / FINE_INTERVAL_S;
    for(uint32_t k = samples; k > 0; k--){
        add_value(now_s - (k - 1) * FINE_INTERVAL_S, NO_VALUE);
    }
}

void ProbeHistory::add_value(uint32_t now_s, int16_t value){
    fine_[fine_head_] = value;
    fine_head_        = (fine_head_ + 1) % FINE_SIZE;
    if(fine_count_ < FINE_SIZE){ fine_count_++; }
    fine_newest_s_    = now_s;

    slot_samples_++;
    if(value != NO_VALUE){
        slot_sum_ += value;
        slot_valid_++;
    }
    if(slot_samples_ * FINE_INTERVAL_S < coarse_interval_s_){ return; }

    coarse_[coarse_count_++] = slot_valid_ ? (int16_t)(slot_sum_ / slot_valid_) : NO_VALUE;
    coarse_newest_s_ = now_s;
    slot_sum_ = 0; slot_valid_ = 0; slot_samples_ = 0;

    // Full: merge neighbouring pairs right away, so every value in the tier has the same interval
    if(coarse_count_ == COARSE_SIZE){
        for(int i = 0; i < COARSE_SIZE / 2; i++){
            coarse_[i] = average(coarse_[2 * i], coarse_[2 * i + 1]);
        }
        coarse_count_       = COARSE_SIZE / 2;
        coarse_interval_s_ *= 2;
    }
}

int16_t ProbeHistory::fine_at(int i) const {
    return fine_[(fine_head_ - fine_count_ + i + FINE_SIZE) % FINE_SIZE];
}

int32_t ProbeHistory::eta_seconds(float target_celcius) const {
    int first = fine_count_ > ETA_WINDOW_SAMPLES ? fine_count_ - ETA_WINDOW_SAMPLES : 0;

    // A pulled probe or an opened lid can put a sharp dip in the window; fitting across it
    // drags the slope negative and hides a real recovery for up to 10 minutes. If the window's
    // lowest point fell at least 2 degrees from an earlier high, fit only from that low point on.
    {
        int16_t running_max = NO_VALUE, min_value = NO_VALUE, max_before_min = NO_VALUE;
        int min_index = first;
        for(int i = first; i < fine_count_; i++){
            int16_t value = fine_at(i);
            if(value == NO_VALUE){ continue; }
            if(min_value == NO_VALUE || value < min_value){
                min_value = value; min_index = i; max_before_min = running_max;
            }
            if(running_max == NO_VALUE || value > running_max){ running_max = value; }
        }
        if(max_before_min != NO_VALUE && max_before_min - min_value >= 20){ first = min_index; }
    }

    // Least squares line through the valid samples: x in seconds, y in degrees
    double n = 0, sum_x = 0, sum_y = 0, sum_xx = 0, sum_xy = 0;
    int16_t newest = NO_VALUE;
    for(int i = first; i < fine_count_; i++){
        int16_t value = fine_at(i);
        if(value == NO_VALUE){ continue; }
        double x = (double)(i - first) * FINE_INTERVAL_S;
        double y = value / 10.0;
        n++; sum_x += x; sum_y += y; sum_xx += x * x; sum_xy += x * y;
        newest = value;
    }
    if(n < ETA_MIN_SAMPLES){ return ETA_UNKNOWN; }

    double denominator = n * sum_xx - sum_x * sum_x;
    if(denominator <= 0){ return ETA_UNKNOWN; }
    double slope_per_s = (n * sum_xy - sum_x * sum_y) / denominator;
    if(slope_per_s * 60.0 < ETA_MIN_SLOPE_PER_MIN){ return ETA_UNKNOWN; }

    double remaining = target_celcius - newest / 10.0;
    if(remaining <= 0){ return ETA_UNKNOWN; }

    double eta = remaining / slope_per_s;
    if(eta > ETA_MAX_S){ return ETA_UNKNOWN; }
    return (int32_t)(eta + 0.5);
}

bool ProbeHistory::stalled(float target_celcius) const {
    if(fine_count_ < STALL_WINDOW_SAMPLES){ return false; }
    int16_t low = NO_VALUE, high = NO_VALUE, newest = NO_VALUE;
    int valid = 0;
    for(int i = fine_count_ - STALL_WINDOW_SAMPLES; i < fine_count_; i++){
        int16_t value = fine_at(i);
        if(value == NO_VALUE){ continue; }
        valid++;
        if(low == NO_VALUE || value < low){ low = value; }
        if(high == NO_VALUE || value > high){ high = value; }
        newest = value;
    }
    if(valid < STALL_MIN_VALID){ return false; }
    if(high - low > STALL_MAX_SPREAD){ return false; }
    if(newest < STALL_LOW || newest > STALL_HIGH){ return false; }
    return target_celcius - newest / 10.0f >= STALL_TARGET_ABOVE;
}

int16_t ProbeHistory::newest() const {
    return fine_count_ > 0 ? fine_at(fine_count_ - 1) : NO_VALUE;
}

int16_t ProbeHistory::sharp_drop_from() const {
    int16_t now = newest();
    if(now == NO_VALUE){ return NO_VALUE; }
    int first = fine_count_ > DROP_WINDOW_SAMPLES ? fine_count_ - DROP_WINDOW_SAMPLES : 0;
    int16_t high = NO_VALUE;
    for(int i = first; i < fine_count_; i++){
        int16_t value = fine_at(i);
        if(value != NO_VALUE && (high == NO_VALUE || value > high)){ high = value; }
    }
    return high - now >= DROP_MIN ? high : NO_VALUE;
}

void LidWatch::update(uint32_t now_s, bool range_mode, uint32_t pause_s, int16_t drop_from, int16_t newest){
    if(!range_mode || pause_s == 0){ open_ = false; return; }
    if(!open_){
        if(drop_from != NO_VALUE){ open_ = true; until_s_ = now_s + pause_s; before_ = drop_from; }
        return;
    }
    bool recovered = newest != NO_VALUE && newest >= before_ - LID_RECOVERED_WITHIN;
    if(now_s >= until_s_ || recovered){ open_ = false; }
}

bool stalled_for_probe(const ProbeHistory& history, bool connected, float target, float minimum, bool fahrenheit){
    bool target_mode = connected && target > 0 && minimum <= 0;
    if(!target_mode){ return false; }
    float target_celcius = fahrenheit ? (target - 32) / 1.8f : target;
    return history.stalled(target_celcius);
}

int32_t eta_for_probe(const ProbeHistory& history, bool connected, float target, float minimum, bool fahrenheit){
    // Only target mode has something to count down to. The target is in the display unit.
    bool target_mode = connected && target > 0 && minimum <= 0;
    if(!target_mode){ return ETA_UNKNOWN; }

    float target_celcius = fahrenheit ? (target - 32) / 1.8f : target;
    return history.eta_seconds(target_celcius);
}

}
