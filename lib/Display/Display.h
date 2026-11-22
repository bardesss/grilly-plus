#pragma once
#include <Arduino.h>
#include <vector>
#include <utility>

#include "Config.h"

// ***********************************
// * Definitions
// ***********************************

class disp {
public:
    disp();
    /**
    *	@brief Initializes the display
    *	@return true if screen was updated
    */
    bool init(void);
    /**
    *	@brief Updates the display
    *	@return true if screen was updated
    */
    bool display_update(void);
    /**
    *	@brief Switches between the various pages
    *	@return true if screen page was switched
    */
   bool switch_page(void);
    /**
    *	@brief Show settings page
    *	@return true if screen page was switched
    */
   bool show_settings_page(void);
    /**
    *	@brief Turns the screen and backlight on with the current brightness and restarts their timeouts
    *	@return true
    */
    bool wake(void);
    /**
    *	@brief Full screen progress for the update mode: title, version, bar and a short status line
    *   @param percent 0-100, or -1 to leave the bar out
    */
    void draw_update(const char* version, int percent, const char* status);



private:
    /**
    *	@brief Subroutine to update the thermometer display
    *   @param xLoc x axis base location
    *   @param yLoc y axis base location
    *	@return true 
    */
    bool draw_thermometer(int xLoc = 0, int yLoc = 0, int probe_temp = 0, int probe_target = 0);
    /**
    *	@brief Subroutine to update the temperature 
    *   @param connectedProbe probe number
    *	@return true 
    */
    bool draw_temp(int connectedProbe);
    /**
    *	@brief Subroutine to draw the progress bar target
    *   @param current_minimum_temp current minimum temperature setting
    *   @param current_target_temp current target temperature setting
    *   @param xLoc x-axis location offset from 0 (left)
    *	@return true 
    */
    bool draw_progress_target(int current_minimum_temp, int current_target_temp, int xLoc = 0);
    /**
    *	@brief Subroutine to draw the progress bar range
    *   @param current_minimum_temp current minimum temperature setting
    *   @param current_target_temp current target temperature setting
    *   @param xLoc x-axis location offset from 0 (left)
    *	@return true 
    */
    bool draw_progress_range(int current_minimum_temp, int current_target_temp, int xLoc = 0);
    /**
    *	@brief Subroutine to get the target temperature
    *   @param connectedProbe probe number
    *	@return true 
    */
    int get_target_temp(int connectedProbe);
        /**
    *	@brief Subroutine to get the minimum temperature 
    *   @param connectedProbe probe number
    *	@return true 
    */
    int get_minimum_temp(int connectedProbe);
    /**
    *	@brief Subroutine to get the probe name 
    *   @param connectedProbe probe number
    *	@return name of probe 
    */
    String get_name(int connectedProbe);
        /**
    *	@brief Subroutine to get the time since connection  
    *   @param connectedProbe probe number
    *	@return true
    */
    std::string get_connection_time(int connectedProbe);
    /**
    *	@brief Seconds until the probe reaches its target, -1 when unknown
    *   @param connectedProbe probe number
    */
    // The food type index of a probe (FoodTypes), -1 = none
    int get_food_icon(int connectedProbe);
    bool get_stalled(int connectedProbe);
    bool get_lid_open(int connectedProbe);
    int32_t get_eta(int connectedProbe);
    /**
    *	@brief Inverts a box over a probe label every other screen update while that probe's alarm sounds
    *   @param connectedProbe probe number
    */
    void blink_label(int connectedProbe, int x, int y, int width, int height);
        /**
    *	@brief Subroutine to get the temperature  
    *   @param connectedProbe probe number
    *	@return true
    */
    float get_temp(int connectedProbe);
    /**
    *	@brief Checks how many probes are connected
    *	@return number of probes connected 
    *   @return vector of connected probes
    */
    std::pair<int, std::vector<int>> get_connected_probes(void);
    /**
    *	@brief Subroutine to draw the temp page
    *	@return true 
    */
    bool draw_screen_temp(void);
    /**
    *	@brief Subroutine to create the temp detail screen
    *	@return true 
    */
    bool draw_screen_details(int connectedProbe);
    /**
    *	@brief Subroutine to create the wifi screen
    *	@return true 
    */
    bool draw_screen_info(void);
    /**
    *	@brief The code for resetting the admin password, shown instead of every page while it is valid
    */
    void draw_screen_reset_code(const char* code, uint32_t seconds_left);
    /**
    *	@brief The text cut to max_width pixels in the current font, ending in "." when it was cut
    */
    String fit(const String& text, int max_width);
    /**
    *	@brief Draws text in the current font over up to max_lines lines of max_width pixels, breaking
    *   between words; the last line is cut with fit()
    */
    void draw_wrapped(int x, int y, int max_width, int line_height, int max_lines, const String& text);
    /**
    *	@brief enables / disables the screen background LED
    *	@return true 
    */
    bool screen_background_pwr(status_type type = ENABLE);
    /**
    *	@brief enables / disables the screen
    *	@return true 
    */
    bool screen_pwr(status_type type = ENABLE);
};

extern disp display;