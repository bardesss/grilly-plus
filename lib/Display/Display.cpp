#include <Arduino.h>
#include <U8g2lib.h>
#include <vector>
#include <utility>
#include <WiFi.h>
#include <chrono>
#include <string>
#include <iomanip>
#include <sstream>

#include "Display.h"
#include "FoodTypes.h"
#include "Grill.h"
#include "Probe.h"
#include "Config.h"
#include "Power.h"
#include "SharedLock.h"


U8G2_ST7565_64128N_F_4W_SW_SPI screen(U8G2_R2, /* clock=*/ 18, /* data=*/ 23, /* cs=*/ 5, /* dc=*/ 17, /* reset=*/ 16); 
bool is_critical_battery_flash          = false;
bool is_display_updating                = false;
int notification_offset                 = 0;
int current_screen_page                 = 0;
String current_active_name              = "";
float current_active_temp               = 0;
int current_target_temp                 = 0;
int current_minimum_temp                = 0;
std::string current_elapsed_time        = "";
unsigned long millis_backlight_timeout  = 0;
unsigned long millis_screen_timeout     = 0;
bool          blink_phase               = false;   // flips every screen update, drives the alarm label blink

static const unsigned char battery_icon[]       U8X8_PROGMEM = {0xfe,0x03,0x01,0x04,0x01,0x0c,0x01,0x0c,0x01,0x04,0xfe,0x03};
static const unsigned char battery_charging[]   U8X8_PROGMEM = {0x3d,0xe5,0x25,0x26,0xe4,0x3c};
static const unsigned char wifi_disconnected[]  U8X8_PROGMEM = {0x05,0x02,0x05};
static const unsigned char probe_thermometer[]  U8X8_PROGMEM = {0x04,0x0a,0x0a,0x1b,0x0a,0x0a,0x0a,0x11,0x11,0x11,0x0e};
static const unsigned char volume_5[]           U8X8_PROGMEM = {0x48,0x8c,0xaf,0xaf,0x8c,0x48};
static const unsigned char volume_4[]           U8X8_PROGMEM = {0x08,0x8c,0xaf,0xaf,0x8c,0x08};
static const unsigned char volume_3[]           U8X8_PROGMEM = {0x08,0x0c,0xaf,0xaf,0x0c,0x08};
static const unsigned char volume_2[]           U8X8_PROGMEM = {0x08,0x0c,0x2f,0x2f,0x0c,0x08};
static const unsigned char volume_1[]           U8X8_PROGMEM = {0x08,0x0c,0x2f,0x2f,0x0c,0x08};
static const unsigned char volume_mute[]        U8X8_PROGMEM = {0x08,0xac,0x4f,0xaf,0x0c,0x08};
static const unsigned char degrees[]            U8X8_PROGMEM = {0x02,0x05,0x02};
static const unsigned char temp_low[]           U8X8_PROGMEM = {0x80,0x00,0xa0,0x02,0xc8,0x09,0x8c,0x18,0x90,0x04,0xa2,0x22,0xc4,0x11,0xff,0x7f,0xc4,0x11,0xa2,0x22,0x90,0x04,0x8c,0x18,0xc8,0x09,0xa0,0x02,0x80,0x00};
static const unsigned char temp_high[]          U8X8_PROGMEM = {0x44,0x04,0x88,0x08,0x88,0x08,0x88,0x08,0x44,0x04,0x22,0x02,0x11,0x01,0x11,0x01,0x11,0x01,0x22,0x02};



disp::disp() {}

bool disp::init(void){
    screen.begin();
    screen.setContrast(22);
    screen.setFontMode(1);
    screen.setBitmapMode(1);
    screen_background_pwr(ENABLE);
    return true;
}

bool disp::switch_page(void){
    std::pair<int, std::vector<int>> connectedProbeInfo = get_connected_probes();
        

    if (current_screen_page >= connectedProbeInfo.first) {current_screen_page = 0;} 
    else {current_screen_page++;}
   
    screen_background_pwr(ENABLE);
    screen_pwr(ENABLE);
    display_update();
    return true;
}

bool disp::show_settings_page(void){
    current_screen_page = 10;
    screen_background_pwr(ENABLE);
    screen_pwr(ENABLE);
    return true;
}

bool disp::wake(void){
    screen_background_pwr(ENABLE);
    screen_pwr(ENABLE);
    return true;
}

String disp::fit(const String& text, int max_width){
    if (screen.getStrWidth(text.c_str()) <= max_width) { return text; }
    String cut = text;
    while (cut.length() > 0) {
        cut.remove(cut.length() - 1);
        cut.trim();
        String shown = cut + ".";
        if (screen.getStrWidth(shown.c_str()) <= max_width) { return shown; }
    }
    return "";
}

void disp::draw_wrapped(int x, int y, int max_width, int line_height, int max_lines, const String& text){
    String rest = text;
    rest.trim();
    for (int line = 0; line < max_lines && rest.length() > 0; line++) {
        String shown = rest;
        if (line < max_lines - 1 && screen.getStrWidth(rest.c_str()) > max_width) {
            // Take as many whole words as fit
            int end = -1;
            for (int space = rest.indexOf(' '); space > 0; space = rest.indexOf(' ', space + 1)) {
                if (screen.getStrWidth(rest.substring(0, space).c_str()) > max_width) { break; }
                end = space;
            }
            shown = end > 0 ? rest.substring(0, end) : rest;
        }
        shown = fit(shown, max_width);
        screen.drawStr(x, y + line * line_height, shown.c_str());
        if (shown.endsWith(".") && !rest.startsWith(shown)) { break; }   // the last line was cut
        rest = rest.substring(shown.length());
        rest.trim();
    }
}

void disp::draw_update(const char* version, int percent, const char* status){
    screen.clearBuffer();
    screen.setFont(u8g2_font_profont12_tr);
    screen.drawStr(3, 14, "Updating");
    screen.setFont(u8g2_font_profont10_tr);
    screen.drawStr(3, 26, version);
    if (percent >= 0) {
        screen.drawFrame(3, 32, 122, 10);
        screen.drawBox(5, 34, (118 * percent) / 100, 6);
        screen.setCursor(3, 53); screen.printf("%d%%", percent);
        screen.drawStr(3, 63, fit(status, 122).c_str());
    } else {
        draw_wrapped(3, 43, 122, 10, 3, status);
    }
    screen.sendBuffer();
}

void disp::draw_screen_reset_code(const char* code, uint32_t seconds_left){
    screen.setFont(u8g2_font_4x6_tr);
    screen.drawStr(2, 6, "Password reset");
    screen.setFont(u8g2_font_profont22_tr);
    screen.drawStr((128 - screen.getStrWidth(code)) / 2, 34, code);
    screen.setFont(u8g2_font_profont10_tr);
    const char* hint = "Enter it in the app";
    screen.drawStr((128 - screen.getStrWidth(hint)) / 2, 48, hint);
    char valid[24];
    snprintf(valid, sizeof(valid), "Valid for %u:%02u", (unsigned)(seconds_left / 60), (unsigned)(seconds_left % 60));
    screen.drawStr((128 - screen.getStrWidth(valid)) / 2, 60, valid);
}

bool disp::screen_background_pwr(status_type type){
    switch (type) {
	case ENABLE:
        power.setScreenBrightness(config::backlight_brightness);
        millis_backlight_timeout = millis();
        break;
    case DISABLE:
        power.setScreenBrightness(0);
        break;
    }
    return true;
}

bool disp::screen_pwr(status_type type){
    switch (type) {
	case ENABLE:
        screen.setPowerSave(ENABLE);
        millis_screen_timeout = millis();
        break;
    case DISABLE:
        screen.setPowerSave(DISABLE);
        break;
    }
    return true;
}


bool disp::display_update(void) {
    if(is_display_updating) {return true;}  //* prevent mulitple simultanious display updates 
    is_display_updating = true;
    blink_phase = !blink_phase;
    if (config::backlight_timeout_minutes > 0 and millis_backlight_timeout + (config::backlight_timeout_minutes * 60000) < millis()) {
        screen_background_pwr(DISABLE);
    }
    // Keep the screen on while a password reset code is showing
    bool showing_reset_code;
    {
        SharedLock lock;
        showing_reset_code = config::admin_reset.active(millis());
    }
    if (showing_reset_code) {
        millis_backlight_timeout = millis();
        millis_screen_timeout = millis();
    }
    if (config::screen_timeout_minutes > 0 and millis_screen_timeout + (config::screen_timeout_minutes * 60000) < millis()) {
        screen_pwr(DISABLE);
        is_display_updating = false;
        return true; 
    }

    screen.clearBuffer(); 
    delay(10);
    screen.drawLine(2, 7, 125, 7);
     
    // ***********************************
    // * Battery elements
    // ***********************************
    if (grill::battery_percentage < 10){
        if (is_critical_battery_flash){
            is_critical_battery_flash = false;
            screen.drawXBMP(114, 0, 12, 6, battery_icon);
        }
        else { is_critical_battery_flash = true; }       
    }
    else {
        screen.drawXBMP(114, 0, 12, 6, battery_icon);
        if (grill::battery_percentage >= 80) { screen.drawLine(122, 2, 122, 3); }
        if (grill::battery_percentage >= 60) { screen.drawLine(120, 2, 120, 3); }
        if (grill::battery_percentage >= 40) { screen.drawLine(118, 2, 118, 3); }
        if (grill::battery_percentage >= 10) { screen.drawLine(116, 2, 116, 3); }
    }

    if(grill::battery_charging) {
        screen.drawXBMP(105, 0, 8,  6, battery_charging);
        notification_offset = 9;
    }
    else {notification_offset = 0;}
    
    // ***********************************
    // * Wifi Elements
    // ***********************************
    if(grill::wifi_connected) {
        if (WiFi.RSSI() > -90) {screen.drawLine(101 - notification_offset, 4, 101 - notification_offset, 5);}
        if (WiFi.RSSI() > -80) {screen.drawLine(103 - notification_offset, 3, 103 - notification_offset, 5);}
        if (WiFi.RSSI() > -70) {screen.drawLine(105 - notification_offset, 2, 105 - notification_offset, 5);}
        if (WiFi.RSSI() > -60) {screen.drawLine(107 - notification_offset, 1, 107 - notification_offset, 5);}
        if (WiFi.RSSI() > -50) {screen.drawLine(109 - notification_offset, 0, 109 - notification_offset, 5);}
    }
    else {
        screen.drawLine(100 - notification_offset, 2, 102 - notification_offset, 0); // cross
        screen.drawLine(100 - notification_offset, 0, 102 - notification_offset, 2); // cross
        screen.drawLine(101 - notification_offset, 4, 101 - notification_offset, 5);
        screen.drawLine(103 - notification_offset, 3, 103 - notification_offset, 5);
        screen.drawLine(105 - notification_offset, 2, 105 - notification_offset, 5);
        screen.drawLine(107 - notification_offset, 1, 107 - notification_offset, 5);
        screen.drawLine(109 - notification_offset, 0, 109 - notification_offset, 5);
    }

    // ***********************************
    // * Volume Elements
    // ***********************************
    if (!config::beep_enabled)          { screen.drawXBMP(88 - notification_offset, 0, 8, 6, volume_mute);}
    else if (config::beep_volume == 5)  { screen.drawXBMP(88 - notification_offset, 0, 8, 6, volume_5);}
    else if (config::beep_volume == 4)  { screen.drawXBMP(88 - notification_offset, 0, 8, 6, volume_4);}
    else if (config::beep_volume == 3)  { screen.drawXBMP(88 - notification_offset, 0, 8, 6, volume_3);}
    else if (config::beep_volume == 2)  { screen.drawXBMP(88 - notification_offset, 0, 8, 6, volume_2);}
    else if (config::beep_volume == 1)  { screen.drawXBMP(88 - notification_offset, 0, 8, 6, volume_1);}
    else if (config::beep_volume == 0)  { screen.drawXBMP(88 - notification_offset, 0, 8, 6, volume_mute);}
    
    // A details page for a probe that was unplugged since the page was selected no longer exists,
    // fall back to the overview instead of reading past the end of the connected probes
    std::pair<int, std::vector<int>> connectedProbeInfo = get_connected_probes();
    if (current_screen_page > 0 and current_screen_page < 9 and current_screen_page > (int)connectedProbeInfo.second.size()) {
        current_screen_page = 0;
    }

    {
        // The pages read probe names, the temperature unit and the grill/wifi/ap Strings. Only drawing
        // into the buffer is locked, sending it to the screen is slow.
        SharedLock lock;
        const char* reset_code = config::admin_reset.code(millis());
        if(reset_code[0] != '\0') {draw_screen_reset_code(reset_code, config::admin_reset.seconds_left(millis()));}
        else if(current_screen_page == 0) {draw_screen_temp();}
        else if (current_screen_page > 0 and current_screen_page < 9) {
            screen.setFont(u8g2_font_4x6_tr);
            screen.drawStr(2, 6, "Details");
            draw_screen_details(connectedProbeInfo.second[current_screen_page-1]);
        }
        else if (current_screen_page == 10) {draw_screen_info();}
        else {current_screen_page = 0;}
    }

    screen.sendBuffer();
    is_display_updating = false;
	return true; 
}

bool disp::draw_screen_temp(void){
    // ***********************************
    // * Temp unit Elements
    // ***********************************
    screen.setFont(u8g2_font_4x6_tr);
    if (config::temperature_unit == "celcius"){screen.drawXBMP(2, 1, 3, 3, degrees); screen.drawUTF8(6, 6, "C");}
    else                                      {screen.drawXBMP(2, 1, 3, 3, degrees); screen.drawUTF8(6, 6, "F");}

    std::pair<int, std::vector<int>> connectedProbeInfo = get_connected_probes();
    switch (connectedProbeInfo.first) {
    case 0:
        screen.setFont(u8g2_font_profont15_tr);
        screen.drawLine(31, 20, 96, 20);
        screen.drawStr(36, 47, "Detected");
        screen.drawStr(33, 33, "No probes");
        screen.drawLine(31, 49, 96, 49);
        break;
    case 1:
        draw_screen_details(connectedProbeInfo.second[0]);  // Reuse of the details screen
        break;
        
    case 2:
        // divider 
        screen.drawLine(0, 36, 127, 36);

        for (int i = 0; i <= 1; i++){
            // variables
            int y_offset = 0;
            current_active_name = get_name(connectedProbeInfo.second[i]);
            current_active_temp = get_temp(connectedProbeInfo.second[i]);
            current_minimum_temp = get_minimum_temp(connectedProbeInfo.second[i]);
            current_target_temp = get_target_temp(connectedProbeInfo.second[i]);
            current_elapsed_time = get_connection_time(connectedProbeInfo.second[i]);

            if(i == 0) { y_offset = 16;}
            if(i == 1) { y_offset = 45;}

            // probe name, with the food icon between the number and the name
            screen.setFont(u8g2_font_profont10_tr);
            screen.drawStr(2, y_offset, "P :");
            screen.setCursor(7, y_offset); screen.print(connectedProbeInfo.second[i]);
            const uint8_t* icon = food::icon_10(get_food_icon(connectedProbeInfo.second[i]));
            int name_x = icon ? 29 : 18;
            if (icon) { screen.drawXBM(17, y_offset - 9, 10, 10, icon); }
            String name = fit(current_active_name, 84 - name_x);   // the timer starts at x 88, keep a gap
            screen.setCursor(name_x, y_offset); screen.print(name);
            blink_label(connectedProbeInfo.second[i], 0, y_offset - 8, name_x + 1 + screen.getStrWidth(name.c_str()), 10);

            // status text
            screen.setCursor(88, y_offset); screen.printf(current_elapsed_time.c_str());
            //screen.drawStr(102, y_offset, "00:00");

            // probe temp 
            screen.setFont(u8g2_font_profont22_tr); 
            screen.setCursor(2, y_offset+18); screen.printf("%.1f", current_active_temp); 

            // progress status, or else the time to target
            screen.setFont(u8g2_font_profont11_tr);
            bool status_shown = (current_target_temp > 0 and current_active_temp > current_target_temp)
                             or current_active_temp < current_minimum_temp;
            int32_t eta = get_eta(connectedProbeInfo.second[i]);
            if (!status_shown and get_stalled(connectedProbeInfo.second[i])) {
                screen.drawStr(125 - screen.getStrWidth("STALL"), y_offset + 17, "STALL");
            } else if (!status_shown and eta >= 0) {
                int minutes = (eta + 59) / 60;
                char eta_text[16];
                snprintf(eta_text, sizeof(eta_text), "in %d:%02d", minutes / 60, minutes % 60);
                screen.drawStr(125 - screen.getStrWidth(eta_text), y_offset + 17, eta_text);
            }
            if (current_minimum_temp <= 0 and current_target_temp > 0 and current_active_temp > current_target_temp){
                screen.drawStr(125-screen.getStrWidth("READY"), y_offset+17, "READY");
                screen.setDrawColor(2);
                screen.drawBox(95, y_offset+9, 31, 9);
                screen.setDrawColor(1);
            }
            else {
                if(current_target_temp > 0 and current_active_temp > current_target_temp) {
                    screen.drawStr(125-screen.getStrWidth("HIGH"), y_offset+17, "HIGH");
                    screen.setDrawColor(2);
                    screen.drawBox(101, y_offset+9, 25, 9);
                    screen.setDrawColor(1);
                    }
                else if (current_active_temp < current_minimum_temp){
                    screen.drawStr(125-screen.getStrWidth("LOW"), y_offset+17, get_lid_open(connectedProbeInfo.second[i]) ? "LID" : "LOW");
                    screen.setDrawColor(2);
                    screen.drawBox(107, y_offset+9, 19, 9);
                    screen.setDrawColor(1);
                    }
                }
        } 
        break;
    
    default:
        if(connectedProbeInfo.first <= 4) {
            // divider 
            screen.drawLine(64, 8, 64, 63);
            screen.drawLine(0, 36, 127, 36);

            for (int i = 0; i <= connectedProbeInfo.first-1; i++){
                int y_offset = 0;
                int x_offset = 0;
                current_active_name = get_name(connectedProbeInfo.second[i]);
                current_active_temp = get_temp(connectedProbeInfo.second[i]);
                current_minimum_temp = get_minimum_temp(connectedProbeInfo.second[i]);
                current_target_temp = get_target_temp(connectedProbeInfo.second[i]);

                if(i == 0) { x_offset = 2; y_offset = 16;}
                if(i == 1) { x_offset = 67; y_offset = 16;}
                if(i == 2) { x_offset = 2; y_offset = 45;}
                if(i == 3) { x_offset = 67; y_offset = 45;}
                
                
                
                // progress status
                screen.setFont(u8g2_font_profont10_tr); 
                screen.drawStr(x_offset, y_offset, "P :");
                screen.setCursor(x_offset + 5, y_offset); screen.print(connectedProbeInfo.second[i]);
                blink_label(connectedProbeInfo.second[i], x_offset - 1, y_offset - 8, 13, 10);

                // The food icon sits between the number and the name or status, which move over for it
                const uint8_t* icon = food::icon_10(get_food_icon(connectedProbeInfo.second[i]));
                int text_x = x_offset + (icon ? 26 : 16);
                if (icon) { screen.drawXBM(x_offset + 14, y_offset - 9, 10, 10, icon); }

                if (current_minimum_temp <= 0 and current_target_temp > 0 and current_active_temp > current_target_temp){
                    screen.drawStr(text_x, y_offset, "READY");
                    screen.setDrawColor(2);
                    screen.drawBox(text_x - 1, y_offset-7, 26, 8);
                    screen.setDrawColor(1);
                }
                else {
                    if(current_target_temp > 0 and current_active_temp > current_target_temp) {
                        screen.drawStr(text_x, y_offset, "HIGH");
                        screen.setDrawColor(2);
                        screen.drawBox(text_x - 1, y_offset-7, 21, 8);
                        screen.setDrawColor(1);
                    }
                    else if (current_active_temp < current_minimum_temp){
                        screen.drawStr(text_x, y_offset, get_lid_open(connectedProbeInfo.second[i]) ? "LID" : "LOW");
                        screen.setDrawColor(2);
                        screen.drawBox(text_x - 1, y_offset-7, 16, 8);
                        screen.setDrawColor(1);
                    }
                    else {
                        // probe name
                        screen.setCursor(text_x, y_offset); screen.print(fit(current_active_name, 60 - (text_x - x_offset)));
                    }
                }
                
                
                // probe temp 
                screen.setFont(u8g2_font_profont22_tr); 
                screen.setCursor(x_offset, y_offset+18); screen.printf("%.1f", current_active_temp); 
            }        
        }
        else {
                     // Probe numbers
        screen.setFont(u8g2_font_5x8_tr);
        screen.drawStr(2, 20, "1:");
        screen.drawStr(2, 33, "2:");
        screen.drawStr(2, 46, "3:");
        screen.drawStr(2, 59, "4:");
        screen.drawLine(63, 9, 63, 63);
        screen.drawStr(67, 20, "5:");
        screen.drawStr(67, 33, "6:");
        screen.drawStr(67, 46, "7:");
        screen.drawStr(67, 59, "8:");
        for (int i = 0; i < 8; i++) {
            blink_label(i + 1, i < 4 ? 1 : 66, 12 + (i % 4) * 13, 11, 10);
        }

        // Probe values in the configured unit, disconnected probes show "-"
        screen.setFont(u8g2_font_profont12_tr);
        {
            Probe* probes[] = {&grill::probe_1, &grill::probe_2, &grill::probe_3, &grill::probe_4,
                               &grill::probe_5, &grill::probe_6, &grill::probe_7, &grill::probe_8};

            for (int i = 0; i < 8; i++) {
                int y = 21 + (i % 4) * 13;
                screen.setCursor(i < 4 ? 15 : 80, y);

                if (!probes[i]->connected) {
                    screen.print("-");
                    continue;
                }

                screen.printf("%.1f", get_temp(i + 1));

                // Food icon after the temperature, then the thermometer
                const uint8_t* icon = food::icon_10(food::type_index(probes[i]->type_icon.c_str()));
                if (icon) { screen.drawXBM(i < 4 ? 46 : 111, y - 9, 10, 10, icon); }
                if (probes[i]->target_temperature > 0) {
                    draw_thermometer(i < 4 ? 57 : 122, y - 10, get_temp(i + 1), probes[i]->target_temperature);
                }
            }
        }
        break;
        }

    }
    return true;
}

bool disp::draw_screen_details(int connectedProbe){
    // variables
    current_active_name = get_name(connectedProbe);
    current_active_temp = get_temp(connectedProbe);
    current_minimum_temp = get_minimum_temp(connectedProbe);
    current_target_temp = get_target_temp(connectedProbe);
    current_elapsed_time = get_connection_time(connectedProbe);

    // probe name
    screen.setFont(u8g2_font_profont12_tr);
    screen.drawStr(3, 20, "P :");
    screen.setCursor(9, 20); screen.print(connectedProbe);
    // The bar and its labels start at x 105 when there is a target
    String name = fit(current_active_name, (current_target_temp > 0 ? 104 : 126) - 22);
    screen.setCursor(22, 20); screen.print(name);
    blink_label(connectedProbe, 1, 10, 23 + screen.getStrWidth(name.c_str()), 12);
    
    // probe temp 
    screen.setFont(u8g2_font_profont29_tr);
    char temp_text[12];
    snprintf(temp_text, sizeof(temp_text), "%.1f", current_active_temp);
    screen.drawStr(3, 42, temp_text);

    // Food icon right of the temperature, when it fits left of the bar and its labels
    const uint8_t* icon = food::icon_16(get_food_icon(connectedProbe));
    int icon_x = 3 + screen.getStrWidth(temp_text) + 6;
    if (icon && icon_x + 16 <= (current_target_temp > 0 ? 100 : 126)) {
        screen.drawXBM(icon_x, 26, 16, 16, icon);
    }

    // status text
    screen.setFont(u8g2_font_profont10_tr); 
    screen.setCursor(3, 53); screen.printf(current_elapsed_time.c_str());
    // Time until the target, rounded up to whole minutes
    int32_t eta = get_eta(connectedProbe);
    if (get_stalled(connectedProbe)) {
        screen.drawStr(3, 62, "STALL");
    } else if (eta >= 0) {
        int minutes = (eta + 59) / 60;
        screen.setCursor(3, 62); screen.printf("in %d:%02d", minutes / 60, minutes % 60);
    }

    if (current_minimum_temp <= 0 and current_target_temp > 0 and current_active_temp > current_target_temp){
        screen.drawStr(95, 62, "READY");
        screen.setDrawColor(2);
        screen.drawBox(94, 55, 26, 8);
        screen.setDrawColor(1);
    }
    else {
        if(current_target_temp > 0 and current_active_temp > current_target_temp) {
            screen.drawStr(100, 62, "HIGH");
            screen.setDrawColor(2);
            screen.drawBox(99, 55, 21, 8);
            screen.setDrawColor(1);
            }
        else if (current_active_temp < current_minimum_temp){
            screen.drawStr(105, 62, get_lid_open(connectedProbe) ? "LID" : "LOW");
            screen.setDrawColor(2);
            screen.drawBox(104, 55, 16, 8);
            screen.setDrawColor(1);
            }
        }
    
    // progress bar
    if (current_minimum_temp > 0 && current_target_temp > current_minimum_temp) 
        draw_progress_range(current_minimum_temp, current_target_temp, 121);
    if (current_target_temp > 0 && current_minimum_temp == 0) 
        draw_progress_target(current_minimum_temp, current_target_temp, 121);
    return true;
}

bool disp::draw_screen_info(void){
    screen.setFont(u8g2_font_4x6_tr);
    screen.drawStr(2, 6, "Info");

    screen.setFont(u8g2_font_4x6_tr);
    // Grill info
    screen.drawStr(10, 14, "NAME:");
    screen.setCursor(33, 14); screen.print(fit(config::grill_name, 94));
    screen.drawStr(18, 21, "FW: ");
    screen.setCursor(33, 21); screen.print(config::grill_firmware_version);
    // mDNS hostname, without ".local" (20 chars x 4px fits); also shown on the About page
    screen.drawStr(10, 28, "HOST:");
    screen.setCursor(33, 28); screen.print(grill::hostname);
    // AP info
    screen.drawStr(10, 35, "SSID:");
    screen.setCursor(33, 35); screen.print(fit(config::wifi_ssid, 94));
    screen.drawStr(18, 42, "IP: ");
    screen.setCursor(33, 42); screen.print(grill::wifi_ip);
    // local AP info, the password is shown so a generated one can be read off the device
    screen.drawStr(2, 49, "L-SSID:");
    screen.setCursor(33, 49); screen.print(fit(config::local_ap_ssid, 94));
    screen.drawStr(10, 56, "L-PW: ");
    screen.setCursor(33, 56); screen.print(config::local_ap_password.isEmpty() ? String("(open)") : fit(config::local_ap_password, 94));
    screen.drawStr(10, 63, "L-IP: ");
    screen.setCursor(33, 63); screen.print(config::local_ap_ip);

    return true;
}

bool disp::draw_thermometer(int xLoc, int yLoc, int probe_temp, int probe_target) {
    screen.drawXBMP(xLoc, yLoc, 5, 11, probe_thermometer);
    if(probe_temp >= probe_target - 30) { screen.drawLine(xLoc+1, yLoc+9, xLoc+3, yLoc+9); }
    if(probe_temp >= probe_target - 25) { screen.drawLine(xLoc+1, yLoc+8, xLoc+3, yLoc+8); }
    if(probe_temp >= probe_target - 20) { screen.drawLine(xLoc+1, yLoc+7, xLoc+3, yLoc+7); }
    if(probe_temp >= probe_target - 15) { screen.drawLine(xLoc+2, yLoc+6, xLoc+2, yLoc+6); }
    if(probe_temp >= probe_target - 10) { screen.drawLine(xLoc+2, yLoc+5, xLoc+2, yLoc+5); }
    if(probe_temp >= probe_target - 5 ) { screen.drawLine(xLoc+2, yLoc+4, xLoc+2, yLoc+4); }
    if(probe_temp >= probe_target - 0 ) { screen.drawLine(xLoc+2, yLoc+3, xLoc+2, yLoc+3); }
    if(probe_temp >= probe_target + 5 ) { screen.drawLine(xLoc+2, yLoc+2, xLoc+2, yLoc+2); }
    if(probe_temp >= probe_target + 10) { screen.drawLine(xLoc+2, yLoc+1, xLoc+2, yLoc+1); }
    return true;
} 

bool disp::draw_progress_range(int current_minimum_temp, int current_target_temp, int x_Loc) {
    screen.drawFrame(x_Loc, 10, 7, 53);
    screen.drawLine(x_Loc - 1, 21, x_Loc - 3, 21);
    int target_offset = 0;
    if (current_target_temp > 99) {target_offset = 4;}
    screen.setFont(u8g2_font_4x6_tr); screen.setCursor(x_Loc - 11 - target_offset, 24); screen.print(current_target_temp);
    screen.drawLine(x_Loc - 1, 41, x_Loc - 3, 41);
    target_offset = 0;
    if (current_minimum_temp > 99) {target_offset = 4;}
    screen.setFont(u8g2_font_4x6_tr); screen.setCursor(x_Loc - 11 - target_offset, 44); screen.print(current_minimum_temp);

    int range_offset = 20+(20*((current_active_temp-current_minimum_temp)/(current_target_temp-current_minimum_temp)));
    if (range_offset > 50) {range_offset = 50;} if (range_offset < 1) {range_offset = 0;}
    screen.drawBox(x_Loc+1, 60 - range_offset, 5, 2 + range_offset);

    return true;
}

bool disp::draw_progress_target(int current_minimum_temp, int current_target_temp, int x_Loc) {
        screen.drawFrame(x_Loc, 10, 7, 53);

        screen.drawLine(x_Loc - 1, 21, x_Loc - 3, 21);
        int target_offset = 0;
        if (current_target_temp > 99) {target_offset = 4;}
        screen.setFont(u8g2_font_4x6_tr); screen.setCursor(x_Loc - 11 - target_offset, 24); screen.print(current_target_temp);

        int range_offset = 40*(current_active_temp/current_target_temp);
        if (range_offset > 50) {range_offset = 50;}
        screen.drawBox(x_Loc +1 , 60 - range_offset, 5, 2 + range_offset);
    return true;
}



bool disp::draw_temp(int connectedProbe) {
    if(config::temperature_unit == "celcius"){
        switch (connectedProbe) {
            case 1: screen.print(grill::probe_1.celcius); break;
            case 2: screen.print(grill::probe_2.celcius); break;
            case 3: screen.print(grill::probe_3.celcius); break;
            case 4: screen.print(grill::probe_4.celcius); break;
            case 5: screen.print(grill::probe_5.celcius); break;
            case 6: screen.print(grill::probe_6.celcius); break;
            case 7: screen.print(grill::probe_7.celcius); break;
            case 8: screen.print(grill::probe_8.celcius); break;
        default:
            break;
        }
    }
    else {
        switch (connectedProbe) {
            case 1: screen.print(grill::probe_1.fahrenheit); break;
            case 2: screen.print(grill::probe_2.fahrenheit); break;
            case 3: screen.print(grill::probe_3.fahrenheit); break;
            case 4: screen.print(grill::probe_4.fahrenheit); break;
            case 5: screen.print(grill::probe_5.fahrenheit); break;
            case 6: screen.print(grill::probe_6.fahrenheit); break;
            case 7: screen.print(grill::probe_7.fahrenheit); break;
            case 8: screen.print(grill::probe_8.fahrenheit); break;
        default:
            break;
        }
    }
    return true;
} 

String disp::get_name(int connectedProbe) {
    String name = "";
    switch (connectedProbe) {
        case 1: name = grill::probe_1.name; break;
        case 2: name = grill::probe_2.name; break;
        case 3: name = grill::probe_3.name; break;
        case 4: name = grill::probe_4.name; break;
        case 5: name = grill::probe_5.name; break;
        case 6: name = grill::probe_6.name; break;
        case 7: name = grill::probe_7.name; break;
        case 8: name = grill::probe_8.name; break;
    default:
        break;
    }
    return name;
}

std::string disp::get_connection_time(int connectedProbe) {                                                                                                                                                                                                                                                                   
    long connected_since;
    long elapsed_seconds;
    switch (connectedProbe) {
        case 1: connected_since = grill::probe_1.connected_time; break;
        case 2: connected_since = grill::probe_2.connected_time; break;
        case 3: connected_since = grill::probe_3.connected_time; break;
        case 4: connected_since = grill::probe_4.connected_time; break;
        case 5: connected_since = grill::probe_5.connected_time; break;
        case 6: connected_since = grill::probe_6.connected_time; break;
        case 7: connected_since = grill::probe_7.connected_time; break;
        case 8: connected_since = grill::probe_8.connected_time; break;
    default:
        break;
    }
    
    elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() - connected_since;
    std::ostringstream oss;

    int hours = elapsed_seconds / 3600;   
    int minutes = (elapsed_seconds / 60) % 60;  
    int seconds = elapsed_seconds % 60;

     oss << std::setfill('0') << std::setw(2) << hours << ":"
         << std::setfill('0') << std::setw(2) << minutes << ":"
         << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}


bool disp::get_lid_open(int connectedProbe) {
    switch (connectedProbe) {
        case 1: return grill::probe_1.lid_open;
        case 2: return grill::probe_2.lid_open;
        case 3: return grill::probe_3.lid_open;
        case 4: return grill::probe_4.lid_open;
        case 5: return grill::probe_5.lid_open;
        case 6: return grill::probe_6.lid_open;
        case 7: return grill::probe_7.lid_open;
        case 8: return grill::probe_8.lid_open;
        default: return false;
    }
}

int disp::get_food_icon(int connectedProbe) {
    if (connectedProbe < 1 || connectedProbe > 8) { return -1; }
    Probe* probes[] = {&grill::probe_1, &grill::probe_2, &grill::probe_3, &grill::probe_4,
                       &grill::probe_5, &grill::probe_6, &grill::probe_7, &grill::probe_8};
    return food::type_index(probes[connectedProbe - 1]->type_icon.c_str());
}

bool disp::get_stalled(int connectedProbe) {
    switch (connectedProbe) {
        case 1: return grill::probe_1.stalled;
        case 2: return grill::probe_2.stalled;
        case 3: return grill::probe_3.stalled;
        case 4: return grill::probe_4.stalled;
        case 5: return grill::probe_5.stalled;
        case 6: return grill::probe_6.stalled;
        case 7: return grill::probe_7.stalled;
        case 8: return grill::probe_8.stalled;
        default: return false;
    }
}

int32_t disp::get_eta(int connectedProbe) {
    switch (connectedProbe) {
        case 1: return grill::probe_1.eta_seconds;
        case 2: return grill::probe_2.eta_seconds;
        case 3: return grill::probe_3.eta_seconds;
        case 4: return grill::probe_4.eta_seconds;
        case 5: return grill::probe_5.eta_seconds;
        case 6: return grill::probe_6.eta_seconds;
        case 7: return grill::probe_7.eta_seconds;
        case 8: return grill::probe_8.eta_seconds;
        default: return -1;
    }
}

void disp::blink_label(int connectedProbe, int x, int y, int width, int height) {
    // alarm_probes has bit (n-1) set for probe n while its alarm sounds, cleared when muted
    bool alarming = connectedProbe >= 1 && connectedProbe <= 8 && (grill::alarm_probes & (1 << (connectedProbe - 1)));
    if (!alarming || !blink_phase) { return; }
    screen.setDrawColor(2);
    screen.drawBox(x, y, width, height);
    screen.setDrawColor(1);
}

float disp::get_temp(int connectedProbe) {
    float temp = 0;
    if(config::temperature_unit == "celcius"){
        switch (connectedProbe) {
            case 1: temp = grill::probe_1.celcius; break;
            case 2: temp = grill::probe_2.celcius; break;
            case 3: temp = grill::probe_3.celcius; break;
            case 4: temp = grill::probe_4.celcius; break;
            case 5: temp = grill::probe_5.celcius; break;
            case 6: temp = grill::probe_6.celcius; break;
            case 7: temp = grill::probe_7.celcius; break;
            case 8: temp = grill::probe_8.celcius; break;
        default:
            break;
        }
    }
    else {
        switch (connectedProbe) {
            case 1: temp = grill::probe_1.fahrenheit; break;
            case 2: temp = grill::probe_2.fahrenheit; break;
            case 3: temp = grill::probe_3.fahrenheit; break;
            case 4: temp = grill::probe_4.fahrenheit; break;
            case 5: temp = grill::probe_5.fahrenheit; break;
            case 6: temp = grill::probe_6.fahrenheit; break;
            case 7: temp = grill::probe_7.fahrenheit; break;
            case 8: temp = grill::probe_8.fahrenheit; break;
        default:
            break;
        }
    }
    return temp;
} 

int disp::get_target_temp(int connectedProbe) {
    int temp = 0;
        switch (connectedProbe) {
            case 1: temp = grill::probe_1.target_temperature; break;
            case 2: temp = grill::probe_2.target_temperature; break;
            case 3: temp = grill::probe_3.target_temperature; break;
            case 4: temp = grill::probe_4.target_temperature; break;
            case 5: temp = grill::probe_5.target_temperature; break;
            case 6: temp = grill::probe_6.target_temperature; break;
            case 7: temp = grill::probe_7.target_temperature; break;
            case 8: temp = grill::probe_8.target_temperature; break;
        default:
            break;
        }
    return temp;
}

int disp::get_minimum_temp(int connectedProbe) {
    int temp = 0;
        switch (connectedProbe) {
            case 1: temp = grill::probe_1.minimum_temperature; break;
            case 2: temp = grill::probe_2.minimum_temperature; break;
            case 3: temp = grill::probe_3.minimum_temperature; break;
            case 4: temp = grill::probe_4.minimum_temperature; break;
            case 5: temp = grill::probe_5.minimum_temperature; break;
            case 6: temp = grill::probe_6.minimum_temperature; break;
            case 7: temp = grill::probe_7.minimum_temperature; break;
            case 8: temp = grill::probe_8.minimum_temperature; break;
        default:
            break;
        }
    return temp;
}

std::pair<int, std::vector<int>> disp::get_connected_probes(void) {
    std::vector<int> connectedProbes;
    int connectedCount = 0;

    if(grill::probe_1.connected) { connectedProbes.push_back(1); connectedCount++; }
    if(grill::probe_2.connected) { connectedProbes.push_back(2); connectedCount++; }
    if(grill::probe_3.connected) { connectedProbes.push_back(3); connectedCount++; }
    if(grill::probe_4.connected) { connectedProbes.push_back(4); connectedCount++; }
    if(grill::probe_5.connected) { connectedProbes.push_back(5); connectedCount++; }
    if(grill::probe_6.connected) { connectedProbes.push_back(6); connectedCount++; }
    if(grill::probe_7.connected) { connectedProbes.push_back(7); connectedCount++; }
    if(grill::probe_8.connected) { connectedProbes.push_back(8); connectedCount++; }
    return {connectedCount, connectedProbes};
}

disp display;