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
#include <Wire.h>

#include "LibLog.h"

#include "Oled.h"
#include "ssd1306.h"
#include "../pins.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

Oled::Oled()
{
}

Oled::~Oled()
{
}

bool Oled::init(void)
{
    if (!ssd1306_init(SCREEN_ADDRESS))
    {
        LOG_TRACE("ssd1306_init: FAILED - address=", DebugLogBase::HEX, SCREEN_ADDRESS);
        return false;
    }

    if (!ssd1306_oled_default_config(SCREEN_HEIGHT, SCREEN_WIDTH))
    {
        LOG_TRACE("ssd1306_oled_default_config: FAILED - ", SCREEN_HEIGHT, SCREEN_WIDTH);
        return false;
    }

    if (!ssd1306_oled_clear_screen())
    {
        LOG_TRACE("ssd1306_oled_clear_screen() FAILED");
        return false;
    }

    if (!ssd1306_oled_onoff(true))
    {
        LOG_TRACE("ssd1306_oled_onoff(true) FAILED");
        return false;
    }

    return true;
}

void Oled::drawText(int16_t x, int16_t y, const char *str)
{
    ssd1306_oled_set_XY(x, y);
    ssd1306_oled_write_line(SSD1306_FONT_NORMAL, str);
}

///////////////////////////////////////////////////////////////////////
#define RTC_X0 0
#define RTC_X1 36
#define RTC_Y 0

#define IP_X0 0
#define IP_X1 0
#define IP_Y 1

#define NPU_X0 0
#define NPU_X1 32
#define NPU_Y 2

#define INFERENCE1_X0 0
#define INFERENCE1_X1 36
#define INFERENCE1_X2 108
#define INFERENCE1_Y1 3
#define INFERENCE1_Y2 4

#define SEND_RESULT_X 0
#define SEND_RESULT_Y 5

void Oled::showRTC(const char *timeStr)
{
    static const char strRTC[] = "RTC:            ";

    // LOG_TRACE("timeStr=", timeStr ? timeStr : "NULL");

    drawText(RTC_X0, RTC_Y, strRTC);
    if (timeStr)
    {
        drawText(RTC_X1, RTC_Y, timeStr);
    }
    else
    {
        drawText(RTC_X1, RTC_Y, "hh:mm:ss");
    }
}
void Oled::showIPAddr(const IPAddress addr)
{
    // static const char strIP[] =   "IP:            ";
    static const char strNone[] = "xxx.xxx.xxx.xxx";
    static const char strEmpty[] = "               ";
    // static const char strNone[] = "---.---.---.---";

    LOG_TRACE("addr=", addr.toString().c_str());

    // drawText(IP_X0, IP_Y, strIP);
    if (addr == IPAddress(0, 0, 0, 0))
    {
        drawText(IP_X1, IP_Y, strNone);
    }
    else
    {
        drawText(IP_X1, IP_Y, strEmpty);
        drawText(IP_X1, IP_Y, addr.toString().c_str());
    }
}

void Oled::showNpu(HardwareState state)
{
    static const char strNpu[] = "NPU:            ";
    static const char strInitializing[] = "Initializing";
    static const char strReady[] = "Ready";
    static const char strError[] = "Error";

    LOG_TRACE("state=", state);

    drawText(NPU_X0, NPU_Y, strNpu);
    switch (state)
    {
    case HardwareState::StateInitializing:
        drawText(NPU_X1, NPU_Y, strInitializing);
        break;
    case HardwareState::StateReady:
        drawText(NPU_X1, NPU_Y, strReady);
        break;
    case HardwareState::StateError:
        drawText(NPU_X1, NPU_Y, strError);
        break;
    }
}

void Oled::showInference(int16_t score, int16_t target)
{
    char buffer[32];

    if (target >= 0)
    {
        snprintf(buffer, sizeof(buffer), "target: %3d", target);
    }
    else
    {
        strcpy(buffer, "target: ---");
    }
    drawText(INFERENCE1_X0, INFERENCE1_Y1, buffer);

    if (score >= 0)
    {
        snprintf(buffer, sizeof(buffer), "score:  %3d", score);
    }
    else
    {
        strcpy(buffer, "score:  ---");
    }
    drawText(INFERENCE1_X0, INFERENCE1_Y2, buffer);
}

void Oled::showSendResult(bool success)
{
    static const char strSuccess[] = "Send Success";
    static const char strFail[] =    "Send Fail   ";

    if (success)
    {
        drawText(SEND_RESULT_X, SEND_RESULT_Y, strSuccess);
    }
    else
    {
        drawText(SEND_RESULT_X, SEND_RESULT_Y, strFail);
    }
}
