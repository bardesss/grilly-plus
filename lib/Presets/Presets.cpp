#include "Presets.h"

#include <string.h>

#include "FoodTypes.h"
#include "ProbeSteps.h"

namespace presets {

namespace {

// A text of 1..max bytes (or 0..max when it may be empty) without control characters
bool text_ok(const char* text, size_t max, bool may_be_empty){
    if(text == nullptr){ return false; }
    size_t length = strlen(text);
    if(length > max || (length == 0 && !may_be_empty)){ return false; }
    for(const char* p = text; *p; p++){
        if((unsigned char)*p < 0x20){ return false; }
    }
    return true;
}

bool number_in(JsonVariantConst value, float low, float high){
    if(!value.is<float>()){ return false; }
    float number = value.as<float>();
    return number >= low && number <= high;
}

std::string numbered(const char* text, size_t index){
    return std::string(text) + " (preset " + std::to_string(index + 1) + ")";
}

// Whole degrees, at least 1 and below top
bool whole_below(JsonVariantConst value, float top){
    return value.is<int>() && value.as<int>() >= 1 && value.as<int>() < top;
}

// A preset's steps: at most 3, the same actions a probe takes, whole degrees below the target in
// both units. Copied into out sorted by c (an empty list adds nothing).
std::string clean_steps(JsonVariantConst in, float c, float f, size_t index, JsonObject copy, bool range){
    if(in.isNull()){ return ""; }
    if(!in.is<JsonArrayConst>() || in.size() > steps::MAX_STEPS){ return numbered("steps: at most 3", index); }
    JsonVariantConst sorted[steps::MAX_STEPS];
    size_t count = 0;
    for(JsonVariantConst step : in.as<JsonArrayConst>()){
        if(!steps::check_action(step["action"] | (const char*)nullptr).empty()){
            return numbered("steps need an action of 1 to 16 characters", index);
        }
        if(!whole_below(step["c"], c) || !whole_below(step["f"], f)){
            return numbered("steps should be below c and f", index);
        }
        size_t j = count++;
        while(j > 0 && sorted[j - 1]["c"].as<int>() > step["c"].as<int>()){ sorted[j] = sorted[j - 1]; j--; }
        sorted[j] = step;
    }
    if(range || count == 0){ return ""; }
    JsonArray out = copy["steps"].to<JsonArray>();
    for(size_t i = 0; i < count; i++){
        JsonObject s = out.add<JsonObject>();
        s["action"] = sorted[i]["action"].as<const char*>();
        s["c"] = sorted[i]["c"].as<int>();
        s["f"] = sorted[i]["f"].as<int>();
    }
    return "";
}

}  // namespace

std::string clean_presets(JsonVariantConst in, JsonArray out){
    if(!in.is<JsonArrayConst>()){ return "presets should be a list"; }
    JsonArrayConst list = in.as<JsonArrayConst>();
    if(list.size() > MAX_PRESETS){ return "At most 30 presets"; }

    size_t index = 0;
    for(JsonVariantConst item : list){
        if(!item.is<JsonObjectConst>()){ return numbered("Every preset should be an object", index); }

        const char* name = item["name"] | (const char*)nullptr;
        if(!text_ok(name, NAME_LENGTH, false)){ return numbered("name should be 1 to 24 characters", index); }

        const char* group = item["group"].isNull() ? "" : (item["group"] | (const char*)nullptr);
        if(!text_ok(group, GROUP_LENGTH, true)){ return numbered("group should be at most 20 characters", index); }

        const char* type = item["type"].isNull() ? "" : (item["type"] | (const char*)nullptr);
        if(!food::type_valid(type)){ return numbered("type should be empty or a food type", index); }

        if(!number_in(item["c"], 1, 300) || !number_in(item["f"], 34, 572)){
            return numbered("c should be 1 to 300 and f 34 to 572", index);
        }
        float c = item["c"].as<float>(), f = item["f"].as<float>();

        // A range needs both minimums, below their maximum; 0 or missing is no range
        bool has_min_c = !item["min_c"].isNull() && item["min_c"].as<float>() != 0;
        bool has_min_f = !item["min_f"].isNull() && item["min_f"].as<float>() != 0;
        if(has_min_c != has_min_f){ return numbered("min_c and min_f go together", index); }
        if(has_min_c && (!number_in(item["min_c"], 1, c - 0.5f) || !number_in(item["min_f"], 33, f - 0.5f))){
            return numbered("min_c and min_f should be below c and f", index);
        }

        // Minutes of rest after the target, 0 or missing is none; a range has no rest
        int rest = 0;
        if(!item["rest"].isNull()){
            if(!item["rest"].is<int>() || item["rest"].as<int>() < 0 || item["rest"].as<int>() > MAX_REST){
                return numbered("rest should be 0 to 180 minutes", index);
            }
            rest = item["rest"].as<int>();
        }

        // Steps are checked before the copy is added, so a refused list leaves out as it was
        JsonDocument step_copy;
        std::string step_error = clean_steps(item["steps"], c, f, index, step_copy.to<JsonObject>(), has_min_c);
        if(!step_error.empty()){ return step_error; }

        JsonObject copy = out.add<JsonObject>();
        copy["name"] = name;
        if(group[0]){ copy["group"] = group; }
        if(type[0]){ copy["type"] = type; }
        copy["c"] = c;
        copy["f"] = f;
        if(has_min_c){
            copy["min_c"] = item["min_c"].as<float>();
            copy["min_f"] = item["min_f"].as<float>();
        } else if(rest > 0){
            copy["rest"] = rest;
        }
        if(!step_copy["steps"].isNull()){ copy["steps"] = step_copy["steps"]; }
        index++;
    }
    return "";
}

std::string clean_hidden(JsonVariantConst in, JsonArray out){
    if(!in.is<JsonArrayConst>()){ return "hidden should be a list"; }
    JsonArrayConst list = in.as<JsonArrayConst>();
    if(list.size() > MAX_HIDDEN){ return "At most 30 hidden presets"; }
    for(JsonVariantConst item : list){
        const char* id = item | (const char*)nullptr;
        size_t length = id ? strlen(id) : 0;
        if(length == 0 || length > ID_LENGTH){ return "hidden should hold preset ids"; }
        for(const char* p = id; *p; p++){
            bool ok = (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-';
            if(!ok){ return "hidden should hold preset ids"; }
        }
        out.add(id);
    }
    return "";
}

}  // namespace presets
