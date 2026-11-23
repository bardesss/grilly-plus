#include "Resume.h"

#include <string.h>
#include <type_traits>

namespace resume {

static_assert(std::is_trivially_copyable<history::ProbeHistory>::value, "copied byte for byte");
static_assert(std::is_trivially_copyable<notify::ProbeWatch>::value, "copied byte for byte");
static_assert(std::is_trivially_copyable<ProbeState>::value, "copied byte for byte");
static_assert(std::is_trivially_copyable<Snapshot>::value, "copied byte for byte");

namespace {
// magic (4), version (2), probe_size (2)
constexpr size_t HEADER_SIZE = 8;
constexpr size_t CRC_SIZE    = 4;
}

uint32_t crc32(const uint8_t* data, size_t size){
    uint32_t crc = 0xFFFFFFFF;
    for(size_t i = 0; i < size; i++){
        crc ^= data[i];
        for(int bit = 0; bit < 8; bit++){ crc = (crc >> 1) ^ (0xEDB88320 & (0u - (crc & 1))); }
    }
    return ~crc;
}

size_t encoded_size(){
    return HEADER_SIZE + sizeof(Snapshot) + CRC_SIZE;
}

size_t encode(const Snapshot& snapshot, uint8_t* out, size_t size){
    if(out == nullptr || size < encoded_size()){ return 0; }
    uint32_t magic      = MAGIC;
    uint16_t version    = VERSION;
    uint16_t probe_size = sizeof(ProbeState);
    memcpy(out, &magic, 4);
    memcpy(out + 4, &version, 2);
    memcpy(out + 6, &probe_size, 2);
    memcpy(out + HEADER_SIZE, &snapshot, sizeof(Snapshot));
    uint32_t crc = crc32(out, HEADER_SIZE + sizeof(Snapshot));
    memcpy(out + HEADER_SIZE + sizeof(Snapshot), &crc, CRC_SIZE);
    return encoded_size();
}

bool decode(const uint8_t* in, size_t size, Snapshot& out){
    if(in == nullptr || size != encoded_size()){ return false; }
    uint32_t magic, crc;
    uint16_t version, probe_size;
    memcpy(&magic, in, 4);
    memcpy(&version, in + 4, 2);
    memcpy(&probe_size, in + 6, 2);
    memcpy(&crc, in + HEADER_SIZE + sizeof(Snapshot), CRC_SIZE);
    if(magic != MAGIC || version != VERSION || probe_size != sizeof(ProbeState)){ return false; }
    if(crc != crc32(in, HEADER_SIZE + sizeof(Snapshot))){ return false; }
    memcpy(&out, in + HEADER_SIZE, sizeof(Snapshot));
    return true;
}

Decision decide(uint32_t saved_epoch, uint32_t now_epoch, uint32_t* gap_s){
    *gap_s = 0;
    if(saved_epoch == 0 || now_epoch == 0 || now_epoch < saved_epoch){ return RESTORE_UNKNOWN_GAP; }
    uint32_t gap = now_epoch - saved_epoch;
    if(gap > MAX_AGE_S){ return DISCARD; }
    *gap_s = gap;
    return RESTORE_KNOWN_GAP;
}

bool flags_apply(const ProbeState& state, float target, float minimum){
    return state.target == target && state.minimum == minimum;
}

uint8_t steps_done_for(const ProbeState& state, const steps::StepList& current){
    if(state.step_count != current.count()){ return 0; }
    uint8_t mask = 0;
    for(size_t i = 0; i < current.count() && i < steps::MAX_STEPS; i++){
        if(current.at(i).temperature == state.step_temperatures[i] && (state.step_done & (1 << i))){ mask |= 1 << i; }
    }
    return mask;
}

}  // namespace resume
