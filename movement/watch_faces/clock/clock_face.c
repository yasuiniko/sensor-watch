/* SPDX-License-Identifier: MIT */

/*
 * MIT License
 *
 * Copyright © 2021-2023 Joey Castillo <joeycastillo@utexas.edu> <jose.castillo@gmail.com>
 * Copyright © 2022 David Keck <davidskeck@users.noreply.github.com>
 * Copyright © 2022 TheOnePerson <a.nebinger@web.de>
 * Copyright © 2023 Jeremy O'Brien <neutral@fastmail.com>
 * Copyright © 2023 Mikhail Svarichevsky <3@14.by>
 * Copyright © 2023 Wesley Aptekar-Cassels <me@wesleyac.com>
 * Copyright © 2024 Matheus Afonso Martins Moreira <matheus.a.m.moreira@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include "clock_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "watch_private_display.h"

#define NUM_TIME_ZONES  41

// 2.45 volts will happen when the battery has maybe 1 week remaining?
// we can refine this later.
#ifndef CLOCK_FACE_LOW_BATTERY_VOLTAGE_THRESHOLD
#define CLOCK_FACE_LOW_BATTERY_VOLTAGE_THRESHOLD 2450
#endif

// #ifndef CLOCK_FACE_24H_ONLY
// #define CLOCK_FACE_24H_ONLY 1
// #endif


/* Simple macros for navigation */
#define FORWARD             +1
#define BACKWARD            -1

/* Activate refresh of time */
#define REFRESH_TIME        0xffffffff

/* List of all time zone names */
const char *zone_namez[] = {
    "UTC",	//  0 :   0:00:00 (UTC)
    "CET",	//  1 :   1:00:00 (Central European Time)
    "SAST",	//  2 :   2:00:00 (South African Standard Time)
    "ARST",	//  3 :   3:00:00 (Arabia Standard Time)
    "IRST",	//  4 :   3:30:00 (Iran Standard Time)
    "GET",	//  5 :   4:00:00 (Georgia Standard Time)
    "AFT",	//  6 :   4:30:00 (Afghanistan Time)
    "PKT",	//  7 :   5:00:00 (Pakistan Standard Time)
    "IST",	//  8 :   5:30:00 (Indian Standard Time)
    "NPT",	//  9 :   5:45:00 (Nepal Time)
    "KGT",	// 10 :   6:00:00 (Kyrgyzstan time)
    "MYST",	// 11 :   6:30:00 (Myanmar Time)
    "THA",	// 12 :   7:00:00 (Thailand Standard Time)
    "CST",	// 13 :   8:00:00 (China Standard Time, Australian Western Standard Time)
    "ACWS",	// 14 :   8:45:00 (Australian Central Western Standard Time)
    "JST",	// 15 :   9:00:00 (Japan Standard Time, Korea Standard Time)
    "ACST",	// 16 :   9:30:00 (Australian Central Standard Time)
    "AEST",	// 17 :  10:00:00 (Australian Eastern Standard Time)
    "LHST",	// 18 :  10:30:00 (Lord Howe Standard Time)
    "SBT",	// 19 :  11:00:00 (Solomon Islands Time)
    "NZST",	// 20 :  12:00:00 (New Zealand Standard Time)
    "CHAS",	// 21 :  12:45:00 (Chatham Standard Time)
    "TOT",	// 22 :  13:00:00 (Tonga Time)
    "CHAD",	// 23 :  13:45:00 (Chatham Daylight Time)
    "LINT",	// 24 :  14:00:00 (Line Islands Time)
    "BIT",	// 25 : -12:00:00 (Baker Island Time)
    "NUT",	// 26 : -11:00:00 (Niue Time)
    "HST",	// 27 : -10:00:00 (Hawaii-Aleutian Standard Time)
    "MART",	// 28 :  -9:30:00 (Marquesas Islands Time)
    "AKST",	// 29 :  -9:00:00 (Alaska Standard Time)
    "PST",	// 30 :  -8:00:00 (Pacific Standard Time)
    "MST",	// 31 :  -7:00:00 (Mountain Standard Time)
    "CST",	// 32 :  -6:00:00 (Central Standard Time)
    "EST",	// 33 :  -5:00:00 (Eastern Standard Time)
    "VET",	// 34 :  -4:30:00 (Venezuelan Standard Time)
    "AST",	// 35 :  -4:00:00 (Atlantic Standard Time)
    "NST",	// 36 :  -3:30:00 (Newfoundland Standard Time)
    "BRT",	// 37 :  -3:00:00 (Brasilia Time)
    "NDT",	// 38 :  -2:30:00 (Newfoundland Daylight Time)
    "FNT",	// 39 :  -2:00:00 (Fernando de Noronha Time)
    "AZOT",	// 40 :  -1:00:00 (Azores Standard Time)
};

typedef struct {
    struct {
        watch_date_time previous;
    } date_time;
    uint8_t last_battery_check;
    uint8_t watch_face_index;
    bool hour_signal_enabled;
    bool battery_low;
    enum {
        CLOCK_MODE_DISPLAY,
        CLOCK_MODE_SETTINGS,
    } current_mode;
    int16_t current_zone;
} clock_state_t;

// static bool clock_is_in_24h_mode(movement_settings_t *settings) {
//     if (CLOCK_FACE_24H_ONLY) { return true; }
//     return settings->bit.clock_mode_24h;
// }

static inline void clock_indicate(WatchIndicatorSegment indicator, bool on) {
    if (on) {
        watch_set_indicator(indicator);
    } else {
        watch_clear_indicator(indicator);
    }
}

// static void clock_indicate_alarm(movement_settings_t *settings) {
//     clock_indicate(WATCH_INDICATOR_BELL, settings->bit.alarm_enabled);
// }

// static void clock_indicate_hour_signal(clock_state_t *clock) {
//     clock_indicate(WATCH_INDICATOR_SIGNAL, clock->hour_signal_enabled);
// }

// static void clock_indicate_24h(movement_settings_t *settings) {
//     clock_indicate(WATCH_INDICATOR_24H, clock_is_in_24h_mode(settings));
// }

// static bool clock_is_pm(watch_date_time date_time) {
//     return date_time.unit.hour >= 12;
// }

// static void clock_indicate_pm(movement_settings_t *settings, watch_date_time date_time) {
//     if (settings->bit.clock_mode_24h) { return; }
//     clock_indicate(WATCH_INDICATOR_PM, clock_is_pm(date_time));
// }

static inline void clock_indicate_low_available_power(clock_state_t *clock) {
    // Set the LAP indicator if battery power is low
    clock_indicate(WATCH_INDICATOR_LAP, clock->battery_low);
}

// static watch_date_time clock_24h_to_12h(watch_date_time date_time) {
//     date_time.unit.hour %= 12;

//     if (date_time.unit.hour == 0) {
//         date_time.unit.hour = 12;
//     }

//     return date_time;
// }

static void clock_check_battery_periodically(clock_state_t *clock, watch_date_time date_time) {
    // check the battery voltage once a day
    if (date_time.unit.day == clock->last_battery_check) { return; }

    clock->last_battery_check = date_time.unit.day;

    watch_enable_adc();
    uint16_t voltage = watch_get_vcc_voltage();
    watch_disable_adc();

    clock->battery_low = voltage < CLOCK_FACE_LOW_BATTERY_VOLTAGE_THRESHOLD;

    clock_indicate_low_available_power(clock);
}

// static void clock_toggle_hour_signal(clock_state_t *clock) {
//     clock->hour_signal_enabled = !clock->hour_signal_enabled;
//     clock_indicate_hour_signal(clock);
// }

// static void clock_toggle_alarm(movement_settings_t *settings) {
//     settings->bit.alarm_enabled = !settings->bit.alarm_enabled;
//     clock_indicate_alarm(settings);
// }

static void clock_display_all(watch_date_time date_time) {
    char buf[10 + 1];

    snprintf(
        buf,
        sizeof(buf),
        "%s%2d%2d%02d%02d",
        watch_utility_get_weekday(date_time),
        date_time.unit.day,
        date_time.unit.hour,
        date_time.unit.minute,
        date_time.unit.second
    );

    watch_display_string(buf, 0);
}

static bool clock_display_some(watch_date_time current, clock_state_t *clock) {
    if ((current.reg >> 6) == (clock->date_time.previous.reg >> 6)) {
        // everything before seconds is the same, don't waste cycles setting those segments.

        watch_display_character_lp_seconds('0' + current.unit.second / 10, 8);
        watch_display_character_lp_seconds('0' + current.unit.second % 10, 9);

        return true;

    } else if ((current.reg >> 12) == (clock->date_time.previous.reg >> 12)) {
        // everything before minutes is the same.

        char buf[4 + 1];

        snprintf(
            buf,
            sizeof(buf),
            "%02d%02d",
            current.unit.minute,
            current.unit.second
        );

        watch_display_string(buf, 6);

        return true;

    } 

    // other stuff changed; let's do it all.
    clock_check_battery_periodically(clock, current);
    return false;
}

static void clock_display_clock(clock_state_t *clock, watch_date_time current) {
    if (!clock_display_some(current, clock)) {
        clock_display_all(current);
    }
}

static void clock_display_low_energy(clock_state_t *clock, watch_date_time current) {
    if ((current.reg >> 12) == (clock->date_time.previous.reg >> 12)) {
        // everything before minutes is the same.

        char buf[4 + 1];

        sprintf(
            buf,
            "%02d  ",
            current.unit.minute
        );

        watch_display_string(buf, 6);

        clock_check_battery_periodically(clock, current);
    } else {
        // write everything
        char buf[10 + 1];

        sprintf(
            buf,
            "%s%2d%2d%02d  ",
            watch_utility_get_weekday(current),
            current.unit.day,
            current.unit.hour,
            current.unit.minute
        );

        watch_display_string(buf, 0);
    }
}

// static void clock_start_tick_tock_animation(void) {
//     if (!watch_tick_animation_is_running()) {
//         watch_start_tick_animation(500);
//     }
// }

// static void clock_stop_tick_tock_animation(void) {
//     if (watch_tick_animation_is_running()) {
//         watch_stop_tick_animation();
//     }
// }

void clock_face_setup(movement_settings_t *settings, uint8_t watch_face_index, void ** context_ptr) {
    (void) settings;
    (void) watch_face_index;

    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(clock_state_t));
        clock_state_t *state = (clock_state_t *) *context_ptr;
        state->hour_signal_enabled = false;
        state->watch_face_index = watch_face_index;
    }
}

void clock_face_activate(movement_settings_t *settings, void *context) {
    clock_state_t *clock = (clock_state_t *) context;
    (void) settings;

    // clock_stop_tick_tock_animation();

    // clock_indicate_hour_signal(clock);
    // clock_indicate_alarm(settings);
    // clock_indicate_24h(settings);

    watch_set_colon();

    // this ensures that none of the timestamp fields will match, so we can re-render them all.
    clock->date_time.previous.reg = 0xFFFFFFFF;
    
    
}


static bool mode_display(movement_event_t event, movement_settings_t *settings, void *context) {
    clock_state_t *state = (clock_state_t *) context;
    watch_date_time current;

    switch (event.event_type) {
        case EVENT_LOW_ENERGY_UPDATE:
            current = watch_rtc_get_date_time();
            clock_display_low_energy(state, current);
            state->date_time.previous = current;
            break;
        case EVENT_TICK:
        case EVENT_ACTIVATE:
            current = watch_rtc_get_date_time();

            clock_display_clock(state, current);

            state->date_time.previous = current;

            break;
        case EVENT_ALARM_LONG_PRESS:
            movement_request_tick_frequency(4);
            watch_clear_colon();
            state->current_mode = CLOCK_MODE_SETTINGS;
            break;

        // case EVENT_BACKGROUND_TASK:
        //     // uncomment this line to snap back to the clock face when the hour signal sounds:
        //     // movement_move_to_face(state->watch_face_index);
        //     movement_play_signal();
        //     break;
        default:
            return movement_default_loop_handler(event, settings);
    }

    return true;
}


static bool mode_settings(movement_event_t event, movement_settings_t *settings, clock_state_t *state)
{
    char buf[11];
    int8_t hours, minutes;
    div_t result;
    uint32_t timestamp;
    watch_date_time date_time;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
        case EVENT_TICK:
        case EVENT_LOW_ENERGY_UPDATE:
            result = div(movement_timezone_offsets[state->current_zone], 60);
            hours = result.quot;
            minutes = result.rem;

            /*
            * Display time zone. The range of the parameters is reduced
            * to avoid accidentally overflowing the buffer and to suppress
            * corresponding compiler warnings.
            */
            sprintf(buf, "%.2s%2d %c%02d%02d",
                    zone_namez[state->current_zone],
                    state->current_zone,
                    hours < 0 ? '-' : '+',
                    abs(hours),
                    abs(minutes)
            );

            /* Let the zone number blink */
            if (event.subsecond & 1)
                buf[2] = buf[3] = ' ';

            watch_display_string(buf, 0);
            break;
        case EVENT_ALARM_BUTTON_UP:
            state->current_zone = state->current_zone + FORWARD;
            if (state->current_zone >= NUM_TIME_ZONES)
                state->current_zone = 0;
            break;
        case EVENT_LIGHT_BUTTON_UP:
            if (state->current_zone == 0)
                state->current_zone = 41;
            state->current_zone = state->current_zone + BACKWARD;
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            /* Do nothing */
            break;
        case EVENT_TIMEOUT:
        case EVENT_ALARM_LONG_PRESS:
            /* Switch to display mode */
            state->current_mode = CLOCK_MODE_DISPLAY;

            // update time
            date_time = watch_rtc_get_date_time();
            timestamp = watch_utility_date_time_to_unix_time(date_time, movement_timezone_offsets[settings->bit.time_zone] * 60);
            date_time = watch_utility_date_time_from_unix_time(timestamp, movement_timezone_offsets[state->current_zone] * 60);
            watch_rtc_set_date_time(date_time);

            // write the time zone
            settings->bit.time_zone = state->current_zone;
            break;
        case EVENT_MODE_BUTTON_UP:
            /* Reset frequency and move to next face */
            state->current_mode = CLOCK_MODE_DISPLAY;
            movement_request_tick_frequency(1);
            movement_move_to_next_face();
            break;
        default:
            return movement_default_loop_handler(event, settings);
    }

    return true;
}

bool clock_face_loop(movement_event_t event, movement_settings_t *settings, void *context)
{
    clock_state_t *state = (clock_state_t *) context;
    switch (state->current_mode) {
	case CLOCK_MODE_DISPLAY:
	    return mode_display(event, settings, state);
	case CLOCK_MODE_SETTINGS:
	    return mode_settings(event, settings, state);
    }
    return false;
}


void clock_face_resign(movement_settings_t *settings, void *context) {
    (void) settings;
    (void) context;
}

// bool clock_face_wants_background_task(movement_settings_t *settings, void *context) {
//     (void) settings;
//     clock_state_t *state = (clock_state_t *) context;
//     if (!state->hour_signal_enabled) return false;

//     watch_date_time date_time = watch_rtc_get_date_time();

//     return date_time.unit.minute == 0;
// }
