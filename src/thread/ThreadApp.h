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
#pragma once
#include <map>
#include "../ArduProfApp.h"
#include "../AppEvent.h"
#include "../peripheral/Oled.h"
#include "../peripheral/LedGreen.h"
#include "../peripheral/DebounceTimer.h"
#include "../peripheral/ButtonTrig.h"
#include "../peripheral/ButtonMode.h"
// #include "../peripheral/ButtonFac.h"

class ThreadApp :
#if defined ARDUPROF_FREERTOS
    public ardufreertos::ThreadBase
#elif defined ARDUPROF_MBED
    public ardumbedos::ThreadBase
#endif
{
public:
    typedef struct _State
    {
        int lastTarget;
    } State;

    ThreadApp();

    static ThreadApp *getInstance(void);

    virtual void start(void *);
    virtual void onMessage(const Message &msg);

protected:
    typedef void (ThreadApp::*handlerFunc)(const Message &);
    std::map<int16_t, handlerFunc> _handlerMap;

private:
    static ThreadApp *_instance;
    State _state;
    Oled _oled;
    LedGreen _ledGreen;
    // LedLD7 _ledLD7;
    // LedLD8 _ledLD8;
    ButtonTrig _buttonTrig;
    ButtonMode _buttonMode;
    // ButtonFac _buttonFac;
    DebounceTimer _debounceTimer;  

    bool _ntpSync;
    ardufreertos::PeriodicTimer _timer1Hz;

    virtual void setup(void);
    void handlerSoftwareTimer(TimerHandle_t xTimer);

    void initRTC(bool from_system_time = false);
    void updateRTC(void);
    void handlerButtonClick(const Message &msg);
    void handlerButtonDoubleClick(const Message &msg);
    void handlerButtonLongPress(const Message &msg);    
    void handlerInference(const Message &msg);

    ///////////////////////////////////////////////////////////////////////
    // declare event handler
    ///////////////////////////////////////////////////////////////////////
    __EVENT_FUNC_DECLARATION(EventAi)
    __EVENT_FUNC_DECLARATION(EventEth)
    __EVENT_FUNC_DECLARATION(EventApp)
    __EVENT_FUNC_DECLARATION(EventSystem)
    __EVENT_FUNC_DECLARATION(EventGpioISR)
    __EVENT_FUNC_DECLARATION(EventNull) // void handlerEventNull(const Message &msg);
};
