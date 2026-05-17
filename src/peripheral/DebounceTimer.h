/* Copyright 2025 teamprof.net@gmail.com
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
#pragma once
#include "../ArduProfApp.h"
#include "./DebounceDef.h"

#define ButtonListSize 10

class DebounceButton;

class DebounceTimer : public ardufreertos::SoftwareTimer, ardufreertos::MessageQueue
{
public:
    DebounceTimer(QueueHandle_t queue,
                  int _eventValue,
                  int _paramValue) : MessageQueue(queue),
                                     ardufreertos::SoftwareTimer(
                                         "Debounce Timer",
                                         DebounceTimerInterval,
                                         pdTRUE, // The timers will auto-reload themselves when they expire.
                                         nullptr,
                                         [](TimerHandle_t xTimer)
                                         // [_instance](TimerHandle_t xTimer)
                                         {
                                             if (_instance != nullptr)
                                             {
                                                 _instance->isr(xTimer);
                                             }
                                         }),
                                     _eventValue(_eventValue),
                                     _paramValue(_paramValue)
    {
        _instance = this;
        memset(_buttonList, 0, sizeof(_buttonList));
    }

    bool attachButton(DebounceButton *button);
    bool detachButton(DebounceButton *button);

    virtual void onEventTimer(void);

protected:
    virtual void isr(TimerHandle_t xTimer)
    {
        sendMessageFromIsrToTask(_eventValue, _paramValue, 0, (uint32_t)xTimer);
    }
    // virtual void isr(TimerHandle_t xTimer);
    // virtual void IRAM_ATTR isr(TimerHandle_t xTimer);

private:
    static DebounceTimer *_instance;
    DebounceButton *_buttonList[ButtonListSize];

    int _eventValue;
    int _paramValue;
};
