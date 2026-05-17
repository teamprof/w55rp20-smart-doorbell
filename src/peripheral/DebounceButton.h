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
#include <Arduino.h>
#include "../ArduProfApp.h"
#include "../AppEvent.h"
#include "./DebounceDef.h"
#include "./DebounceTimer.h"

class DebounceTimer;

class DebounceButton : public ardufreertos::MessageQueue
{
public:
    DebounceButton(uint8_t pin,
                   uint8_t activeState,
                   uint8_t ioMode,
                   QueueHandle_t queue) : _PIN(pin),
                                          pinStateActive(activeState),
                                          isIntrEnable(false),
                                          _debounceTimer(nullptr),
                                          _eventValue(EventNull),
                                          _buttonClick(EventNull),
                                          _buttonDoubleClick(EventNull),
                                          _buttonLongPress(EventNull),
                                          MessageQueue(queue)
    {
        debounceCount = 0;
        debounceActive = false;

        _clickCount = 0;

        _last_state = (activeState == LOW ? HIGH : LOW);
        _last_isr_ms = 0;

        pinMode(_PIN, ioMode);
    }

    ~DebounceButton()
    {
        disableInterrupt();
    }

    bool init(int16_t evValue, int16_t clickValue, int16_t doubleClickValue, int16_t longPressValue)
    {
        _eventValue = evValue;
        _buttonClick = clickValue;
        _buttonDoubleClick = doubleClickValue;
        _buttonLongPress = longPressValue;
        return true;
    }

    void enableInterrupt(PinStatus intrMode)
    {
        if (!isIntrEnable)
        {
            attachInterrupt(digitalPinToInterrupt(_PIN), isr, intrMode, this);
            isIntrEnable = true;
        }
    }

    void disableInterrupt(void)
    {
        if (isIntrEnable)
        {
            detachInterrupt(digitalPinToInterrupt(_PIN));
            isIntrEnable = false;
        }
    }

    void setDebounceActive(bool active)
    {
        debounceCount = 0;
        debounceActive = active;
    }

    bool isDebounceActive(void)
    {
        return debounceActive;
    }

    void onEventIsr(uint8_t value, uint32_t ms)
    {
        if (value == getActiveState())
        {
            _timeBegin = ms;
            if (_clickCount == 0)
            {
                setDebounceActive(true);
                if (_debounceTimer)
                {
                    _debounceTimer->start();
                }
            }
        }
        else if (isDebounceActive())
        {
            _timeEnd = ms;
            uint32_t delta = (_timeEnd > _timeBegin) ? (_timeEnd - _timeBegin) : (_timeBegin - _timeEnd);
            if (delta > DebounceDuration)
            {
                _clickCount++;
            }
        }
    }

    void onEventTimer(void)
    {
        if (!isDebounceActive())
        {
            return;
        }

        if (digitalRead(_PIN) == pinStateActive)
        {
            if (debounceCount >= LongPressDuration)
            {
                _clickCount = 0;
                setDebounceActive(false);
                sendMessageToTask(_eventValue, _buttonLongPress, _PIN);
            }
            else
            {
                debounceCount += DebounceTimerInterval;
            }
        }
        else if (debounceCount >= DoubleClickDuration)
        {
            int16_t event;
            if (_clickCount == 1)
            {
                sendMessageToTask(_eventValue, _buttonClick, _PIN);
            }
            else if (_clickCount == 2)
            {
                sendMessageToTask(_eventValue, _buttonDoubleClick, _PIN);
            }

            _clickCount = 0;
            setDebounceActive(false);
        }
        else
        {
            debounceCount += DebounceTimerInterval;
            // setDebounceActive(false);
        }
    }

    uint8_t getPin(void)
    {
        return _PIN;
    }
    uint8_t getActiveState(void)
    {
        return pinStateActive;
    }
    int read(void)
    {
        return digitalRead(_PIN);
    }

    uint16_t getClickCount(void)
    {
        return _clickCount;
    }

protected:
    uint32_t debounceCount;
    bool debounceActive;
    uint8_t pinStateActive;
    bool isIntrEnable;

private:
    static void isr(DebounceButton *instance)
    {
        if (instance)
        {
            auto pin = instance->getPin();
            uint32_t ms = millis();
            uint8_t state = digitalRead(pin);

            // soft Schmitt-triggers
            if (instance->_last_state != state)
            {
                instance->_last_isr_ms = ms;
                instance->_last_state = state;
                instance->sendMessageFromIsrToTask(EventGpioISR, pin, state, ms);
            }
        }
    }

    // void isr(void)
    // {
    //     // soft Schmitt-triggers
    //     uint32_t ms = millis();
    //     uint8_t state = digitalRead(_PIN);
    //     if (_last_state != state)
    //     // if (_last_isr_ms != ms || _last_state != state)
    //     {
    //         _last_isr_ms = ms;
    //         _last_state = state;
    //         sendMessageFromIsrToTask(EventGpioISR, _PIN, state, ms);
    //         // sendMessageFromIsrToTask(EventGpioISR, _PIN, digitalRead(_PIN), millis());
    //     }
    // }

    int16_t _eventValue;
    int16_t _buttonClick;
    int16_t _buttonDoubleClick;
    int16_t _buttonLongPress;

    friend DebounceTimer;
    DebounceTimer *_debounceTimer;

    uint16_t _clickCount;
    uint32_t _timeBegin, _timeEnd;
    uint8_t _last_state;
    uint32_t _last_isr_ms;

    const uint8_t _PIN;
    QueueHandle_t _queue;
};
