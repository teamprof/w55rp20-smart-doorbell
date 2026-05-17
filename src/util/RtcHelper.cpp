/* Copyright 2026 teamprof.net@gmail.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "hardware/rtc.h"

#include "../ArduProfApp.h"
#include "./RtcHelper.h"

namespace rtc
{
    bool initFromCompileTime(void)
    {
        datetime_t t;

        rtc_init();

        char monthStr[4];
        // Use standard 'int' for temporary storage to stay safe with sscanf
        int tYear, tMonth, tDay, tHour, tMin, tSec;

        // 1. Parse the strings into integers
        sscanf(__DATE__, "%s %d %d", monthStr, &tDay, &tYear);
        sscanf(__TIME__, "%d:%d:%d", &tHour, &tMin, &tSec);

        // 2. Assign to struct (handling the naming mismatch)
        t.year = (uint16_t)tYear;
        t.day = (uint8_t)tDay;
        t.hour = (uint8_t)tHour;
        t.min = (uint8_t)tMin; // Using .min based on your previous error
        t.sec = (uint8_t)tSec; // Using .sec for consistency

        // 3. Convert month string to integer
        const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        t.month = 0;
        for (uint8_t i = 0; i < 12; i++)
        {
            if (strcmp(monthStr, months[i]) == 0)
            {
                t.month = i + 1;
                break;
            }
        }
        LOG_TRACE(__DATE__, " ", __TIME__);
        LOG_TRACE("monthStr=", monthStr, ", t.month=", t.month,
                  ", t.day=", t.day, ", t.year=", t.year,
                  ", t.hour=", t.hour, ", t.min=", t.min, ", t.sec=", t.sec);
        // monthStr=Apr, t.month=4, t.day=21, t.year=2026, t.hour=15, t.min=35, t.sec=3

        struct tm ti = {0};
        ti.tm_year = t.year - 1900;
        ti.tm_mon = t.month - 1;
        ti.tm_mday = t.day;
        mktime(&ti);
        t.dotw = (int8_t)ti.tm_wday;

        return rtc_set_datetime(&t);
    }

    bool initFromSystemTime(void)
    {
        // datetime_t t;

        rtc_init();

        time_t now = time(nullptr);
        struct tm *tm_info = localtime(&now);
        datetime_t t = {
            .year = (int16_t)(tm_info->tm_year + 1900),
            .month = (int8_t)(tm_info->tm_mon + 1),
            .day = (int8_t)tm_info->tm_mday,
            .dotw = (int8_t)tm_info->tm_wday, // 0 is Sunday
            .hour = (int8_t)tm_info->tm_hour,
            .min = (int8_t)tm_info->tm_min,
            .sec = (int8_t)tm_info->tm_sec,
        };
        LOG_TRACE("t.month=", t.month,
                  ", t.day=", t.day, ", t.year=", t.year,
                  ", t.hour=", t.hour, ", t.min=", t.min, ", t.sec=", t.sec);
        // t.month=4, t.day=21, t.year=2026, t.hour=7, t.min=57, t.sec=24

        return rtc_set_datetime(&t);
    }
} // namespace rtc
