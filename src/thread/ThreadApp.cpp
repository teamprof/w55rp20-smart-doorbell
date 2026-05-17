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

#include "./ThreadApp.h"
#include "../AppContext.h"
#include "../util/RtcHelper.h"
#include "../type/ConstAi.h"

////////////////////////////////////////////////////////////////////////////////////////////
ThreadApp *ThreadApp::_instance = nullptr;

ThreadApp *ThreadApp::getInstance(void)
{
    if (!_instance)
    {
        static ThreadApp instance;
        _instance = &instance;
    }
    return _instance;
}

#if defined ARDUPROF_FREERTOS && defined ARDUINO_ARCH_RP2040
////////////////////////////////////////////////////////////////////////////////////////////
// Thread for FreeRTOS RP2040/RP2350
////////////////////////////////////////////////////////////////////////////////////////////

static constexpr UBaseType_t uxCoreAffinityMask = ((1 << 0)); // task only run on core 0
// static constexpr UBaseType_t uxCoreAffinityMask = ((1 << 1)); // task only run on core 1
// static constexpr uxCoreAffinityMask = ( ( 1 << 0 ) | ( 1 << 2 ) );  // e.g. task can only run on core 0 and core 2

#define TASK_NAME "ThreadApp"
#define TASK_STACK_SIZE (8192 / sizeof(StackType_t))
// #define TASK_STACK_SIZE (4096 / sizeof(StackType_t))
#define TASK_PRIORITY 6    // Priority, (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
#define TASK_QUEUE_SIZE 16 // message queue size for app task
// #define TASK_QUEUE_SIZE 8 // message queue size for app task
static_assert(TASK_PRIORITY <= configMAX_PRIORITIES, "TASK_PRIORITY exceeds configMAX_PRIORITIES");

#define TASK_INIT_NAME "taskDelayInit"
#define TASK_INIT_STACK_SIZE (4096 / sizeof(StackType_t))
#define TASK_INIT_PRIORITY 0
static_assert(TASK_INIT_PRIORITY <= configMAX_PRIORITIES, "TASK_INIT_PRIORITY exceeds configMAX_PRIORITIES");

static uint8_t ucQueueStorageArea[TASK_QUEUE_SIZE * sizeof(Message)];
static StaticQueue_t xStaticQueue;

static StackType_t xStack[TASK_STACK_SIZE];
static StaticTask_t xTaskBuffer;

///////////////////////////////////////////////////////////////////////
ThreadApp::ThreadApp() : ThreadBase(TASK_QUEUE_SIZE, ucQueueStorageArea, &xStaticQueue),
                         _state({.lastTarget = -1}),
                         _oled(),
                         _ledGreen(),
                         //  _ledLD7(),
                         //  _ledLD8(),
                         _buttonTrig(queue()),
                         _buttonMode(queue()),
                         //  _buttonFac(queue()),
                         _debounceTimer(queue(), EventSystem, SysSoftwareTimer),
                         _ntpSync(false),
                         _timer1Hz("Timer 1Hz",
                                   pdMS_TO_TICKS(1000),
                                   [](TimerHandle_t xTimer)
                                   {
                                       if (_instance)
                                       {
                                           auto context = reinterpret_cast<AppContext *>(_instance->context());
                                           if (context && context->threadApp)
                                           {
                                               static_cast<ThreadApp *>(context->threadApp)->postEvent(EventSystem, SysSoftwareTimer, 0, (uint32_t)xTimer);
                                           }
                                       }
                                   }),
                         _handlerMap()
{
    _instance = this;

    _handlerMap = {
        __EVENT_MAP(ThreadApp, EventApp),
        __EVENT_MAP(ThreadApp, EventAi),
        __EVENT_MAP(ThreadApp, EventEth),
        __EVENT_MAP(ThreadApp, EventSystem),
        __EVENT_MAP(ThreadApp, EventGpioISR),
        __EVENT_MAP(ThreadApp, EventNull), // {EventNull, &ThreadApp::handlerEventNull},
    };
}

void ThreadApp::start(void *ctx)
{
    LOG_TRACE("core", get_core_num());
    configASSERT(ctx);
    _context = ctx;

    _taskHandle = xTaskCreateStatic(
        [](void *instance)
        { static_cast<ThreadBase *>(instance)->run(); },
        TASK_NAME,
        TASK_STACK_SIZE, // This stack size can be checked & adjusted by reading the Stack Highwater
        this,
        TASK_PRIORITY, // Priority, (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
        xStack,
        &xTaskBuffer);
    configASSERT(_taskHandle);
    vTaskCoreAffinitySet(_taskHandle, uxCoreAffinityMask); // Set the core affinity mask for the task, i.e. set task on running core
}

#elif defined ARDUPROF_FREERTOS && defined ESP_PLATFORM
////////////////////////////////////////////////////////////////////////////////////////////
// Thread for ESP32
////////////////////////////////////////////////////////////////////////////////////////////

// #define RUNNING_CORE 0 // dedicate core 0 for Thread
// #define RUNNING_CORE 1 // dedicate core 1 for Thread
#define RUNNING_CORE ARDUINO_RUNNING_CORE

#define TASK_NAME "ThreadApp"
#define TASK_STACK_SIZE (4096 / sizeof(StackType_t))
#define TASK_PRIORITY 6   // Priority, (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
#define TASK_QUEUE_SIZE 8 // message queue size for app task
static_assert(TASK_PRIORITY <= configMAX_PRIORITIES, "TASK_PRIORITY exceeds configMAX_PRIORITIES");

#define TASK_INIT_NAME "taskDelayInit"
#define TASK_INIT_STACK_SIZE (4096 / sizeof(StackType_t))
#define TASK_INIT_PRIORITY 0
static_assert(TASK_INIT_PRIORITY <= configMAX_PRIORITIES, "TASK_INIT_PRIORITY exceeds configMAX_PRIORITIES");

static uint8_t ucQueueStorageArea[TASK_QUEUE_SIZE * sizeof(Message)];
static StaticQueue_t xStaticQueue;

static StackType_t xStack[TASK_STACK_SIZE];
static StaticTask_t xTaskBuffer;

////////////////////////////////////////////////////////////////////////////////////////////
ThreadApp::ThreadApp() : ardufreertos::ThreadBase(TASK_QUEUE_SIZE, ucQueueStorageArea, &xStaticQueue),
                         _handlerMap()
{
    _instance = this;

    // setup event handlers
    _handlerMap = {
        __EVENT_MAP(ThreadApp, EventNull), // {EventNull, &ThreadApp::handlerEventNull},
    };
}

void ThreadApp::start(void *ctx)
{
    // LOG_TRACE("on core ", xPortGetCoreID(), ", xPortGetFreeHeapSize()=", xPortGetFreeHeapSize());
    ThreadBase::start(ctx);

    _taskHandle = xTaskCreateStaticPinnedToCore(
        [](void *instance)
        { static_cast<ThreadBase *>(instance)->run(); },
        TASK_NAME,
        TASK_STACK_SIZE, // This stack size can be checked & adjusted by reading the Stack Highwater
        this,
        TASK_PRIORITY, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
        xStack,
        &xTaskBuffer,
        RUNNING_CORE);
}

#elif defined ARDUPROF_MBED && defined ARDUINO_ARCH_MBED_RP2040
////////////////////////////////////////////////////////////////////////////////////////////
// Thread for MBed RP2040
////////////////////////////////////////////////////////////////////////////////////////////
#define THREAD_QUEUE_SIZE (64 * EVENTS_EVENT_SIZE) // message queue size for app thread

/////////////////////////////////////////////////////////////////////////////
// use static threadQueue instead of heap
static events::EventQueue threadQueue(THREAD_QUEUE_SIZE);
ThreadApp::ThreadApp() : ardumbedos::ThreadBase(&threadQueue),
                         _handlerMap(),
                         _ledGreen(),
                         _kf(KF_E_MEA, KF_E_EST, KF_Q),
                         _state({0})
/////////////////////////////////////////////////////////////////////////////
// threadQueue is dynamically allocate from heap
// ThreadApp::ThreadApp() : ThreadBase(THREAD_QUEUE_SIZE),
//                          _handlerMap()
/////////////////////////////////////////////////////////////////////////////
{
    _handlerMap = {
        __EVENT_MAP(ThreadApp, EventApp),
        __EVENT_MAP(ThreadApp, EventNull), // {EventNull, &ThreadApp::handlerEventNull},
    };
}

void ThreadApp::start(void *ctx)
{
    LOG_TRACE("core", get_core_num(), ", ctx=(hex)", DebugLogBase::HEX, (uint32_t)ctx);
    ThreadBase::start(ctx);
}

#endif

void ThreadApp::setup(void)
{
    ThreadBase::setup();

    _ledGreen.off();
    // _ledLD7.off();
    // _ledLD8.off();
    if (_oled.init())
    {
        LOG_DEBUG("OLED display initialized successfully");
        _oled.showRTC(NULL);
        _oled.showIPAddr(IPAddress(0, 0, 0, 0));
        _oled.showNpu(HardwareState::StateInitializing);
        _oled.showInference(-1, -1);
    }
    else
    {
        LOG_ERROR("Failed to initialize OLED display!");
    }

    _buttonTrig.init(EventSystem, SysButtonClick, SysButtonDoubleClick, SysButtonLongPress);
    _buttonMode.init(EventSystem, SysButtonClick, SysButtonDoubleClick, SysButtonLongPress);
    // _buttonFac.init(EventSystem, SysButtonClick, SysButtonDoubleClick, SysButtonLongPress);
    _debounceTimer.attachButton(&_buttonTrig);
    _debounceTimer.attachButton(&_buttonMode);
    // _debounceTimer.attachButton(&_buttonFac);

    _timer1Hz.start();
    // _timer1Hz.stop();

    // vTaskDelay(pdMS_TO_TICKS(1000));
}

/////////////////////////////////////////////////////////////////////////////
void ThreadApp::onMessage(const Message &msg)
{
    // LOG_TRACE("event=", msg.event, ", iParam=", msg.iParam, ", uParam=", msg.uParam, ", lParam=", msg.lParam);
    auto func = _handlerMap[msg.event];
    if (func)
    {
        (this->*func)(msg);
    }
    else
    {
        LOG_TRACE("Unsupported event=", msg.event, ", iParam=", msg.iParam, ", uParam=", msg.uParam, ", lParam=", msg.lParam);
    }
}

/////////////////////////////////////////////////////////////////////////////
__EVENT_FUNC_DEFINITION(ThreadApp, EventAi, msg) // void ThreadApp::handlerEventAi(const Message &msg)
{
    // LOG_TRACE("EventAi(", msg.event, "), iParam=", msg.iParam, ", uParam=", msg.uParam, ", lParam=", msg.lParam);
    switch (msg.iParam)
    {
    case AiInference:
        handlerInference(msg);
        break;
    case AiErrHardware:
        LOG_ERROR("AI hardware error!");
        _oled.showNpu(HardwareState::StateError);
        break;
    case AiReady:
        LOG_DEBUG("AI hardware is ready");
        _oled.showNpu(HardwareState::StateReady);
        break;
    default:
        LOG_WARN("Unsupported event=", msg.event, ", iParam=", msg.iParam, ", uParam=", msg.uParam, ", lParam=", msg.lParam);
        break;
    }
}

__EVENT_FUNC_DEFINITION(ThreadApp, EventApp, msg) // void ThreadApp::handlerEventApp(const Message &msg)
{
    auto src = static_cast<AppTriggerSource>(msg.iParam);
    switch (src)
    {
    case AppSentSuccess:
        LOG_DEBUG("Alert sent successfully");
        break;

    case AppSentFail:
        LOG_DEBUG("Failed to send alert");
        break;

    default:
        // DBGLOG(Debug, "Unsupported src=%d, uParam=%u, lParam=%lu", src, msg.uParam, msg.lParam);
        LOG_TRACE("Unsupported src=", src, ", uParam=", msg.uParam, ", lParam=", msg.lParam);
        break;
    }
}

__EVENT_FUNC_DEFINITION(ThreadApp, EventEth, msg) // void ThreadApp::handlerEventEth(const Message &msg)
{
    LOG_TRACE("EventEth(", msg.event, "), iParam=", msg.iParam, ", uParam=", msg.uParam, ", lParam=", msg.lParam);
    switch (msg.iParam)
    {
    case EthUp:
    {
        auto ip = IPAddress(msg.lParam);
        LOG_TRACE("EthUp: IP=", ip);
        _oled.showIPAddr(ip);
        break;
    }

    case EthDn:
        LOG_TRACE("EthDn");
        _oled.showIPAddr(IPAddress(0, 0, 0, 0));
        break;

    case EthNtpSync:
        LOG_TRACE("EthNtpSync");
        _ntpSync = true;
        initRTC(true);
        updateRTC();
        break;

    case EthErrHardware:
        LOG_ERROR("EthErrHardware!");
        break;

    default:
        LOG_TRACE("Unsupported event=", msg.event, ", iParam=", msg.iParam, ", uParam=", msg.uParam, ", lParam=", msg.lParam);
        break;
    }
}

__EVENT_FUNC_DEFINITION(ThreadApp, EventGpioISR, msg) // void ThreadApp::handlerEventGpioISR(const Message &msg)
{
    LOG_TRACE("EventGpioISR(", msg.event, "), iParam = ", msg.iParam, ", uParam = ", msg.uParam, ", lParam = ", msg.lParam);

    uint8_t pin = msg.iParam;
    uint8_t value = msg.uParam;
    // if (pin == _buttonBoot.getPin())
    // {
    //     uint16_t clickCount = _buttonBoot.getClickCount();
    //     LOG_TRACE("EventGpioISR: _buttonBoot: clickCount=", clickCount);
    //     uint32_t ms = msg.lParam;
    //     _buttonBoot.onEventIsr(value, ms);
    // }
    // else
    if (pin == _buttonTrig.getPin())
    {
        uint32_t ms = msg.lParam;
        _buttonTrig.onEventIsr(value, ms);
    }
    else if (pin == _buttonMode.getPin())
    {
        uint32_t ms = msg.lParam;
        _buttonMode.onEventIsr(value, ms);
    }
    else
    {
        LOG_TRACE("unsupported button: GPIO", pin);
    }
}

__EVENT_FUNC_DEFINITION(ThreadApp, EventSystem, msg) // void ThreadApp::handlerEventSystem(const Message &msg)
{
    // LOG_TRACE("EventSystem(", msg.event, "), iParam = ", msg.iParam, ", uParam = ", msg.uParam, ", lParam = ", msg.lParam);
    enum SystemTriggerSource src = static_cast<SystemTriggerSource>(msg.iParam);
    switch (src)
    {
    case SysSoftwareTimer:
        handlerSoftwareTimer((TimerHandle_t)(msg.lParam));
        break;
    case SysButtonClick:
    {
        handlerButtonClick(msg);
        break;
    }
    case SysButtonDoubleClick:
    {
        handlerButtonDoubleClick(msg);
        break;
    }
    case SysButtonLongPress:
    {
        handlerButtonLongPress(msg);
        break;
    }
    default:
        LOG_TRACE("unsupported SystemTriggerSource=", src);
        break;
    }
}

// define EventNull handler
__EVENT_FUNC_DEFINITION(ThreadApp, EventNull, msg) // void ThreadApp::handlerEventNull(const Message &msg)
{
    LOG_TRACE("EventNull(", msg.event, "), iParam=", msg.iParam, ", uParam=", msg.uParam, ", lParam=", msg.lParam);
}
/////////////////////////////////////////////////////////////////////////////
void ThreadApp::handlerSoftwareTimer(TimerHandle_t xTimer)
{
    if (xTimer == _debounceTimer.timer())
    {
        // LOG_TRACE("_debounceTimer::timer()");
        _debounceTimer.onEventTimer();
    }
    else if (xTimer == _timer1Hz.timer())
    {
        // LOG_TRACE("_timer1Hz");

        _ledGreen.toggle();
        // _ledLD7.toggle();
        // _ledLD8.toggle();

        if (_ntpSync)
        {
            updateRTC();
        }
    }
    else
    {
        LOG_TRACE("unsupported timer handle=0x%04x", (uint32_t)(xTimer));
    }
}

void ThreadApp::handlerButtonClick(const Message &msg)
{
    int16_t pin = msg.uParam;
    // LOG_TRACE("pin =", pin);
    // if (pin == _buttonBoot.getPin())
    // {
    //     LOG_TRACE("ButtonClick: _buttonBoot");
    //     postEvent(EventApp, AppButton, SysButtonClick, pin);
    // }
    // else
    if (pin == _buttonTrig.getPin())
    {
        LOG_TRACE("ButtonClick: _buttonTrig");
        auto tenantCount = 1;
        auto strangerCount = 0;
        auto ctx = reinterpret_cast<AppContext *>(context());
        postEvent(ctx->threadEth, EventApp, AppSendAlert, tenantCount, strangerCount);
    }
    else if (pin == _buttonMode.getPin())
    {
        LOG_TRACE("ButtonClick: _buttonMode");
        // postEvent(EventApp, AppButton, SysButtonClick, pin);
    }
    else
    {
        LOG_TRACE("SysButtonClick: unsupported pin=", pin);
    }
}
void ThreadApp::handlerButtonDoubleClick(const Message &msg)
{
    int16_t pin = msg.uParam;
    // if (pin == _buttonBoot.getPin())
    // {
    //     LOG_TRACE("SysButtonDoubleClick: _buttonBoot");
    //     postEvent(EventApp, AppButton, SysButtonDoubleClick, pin);
    // }
    // else
    if (pin == _buttonTrig.getPin())
    {
        LOG_TRACE("SysButtonDoubleClick: _buttonTrig");
        // postEvent(EventApp, AppButton, SysButtonDoubleClick, pin);
    }
    else if (pin == _buttonMode.getPin())
    {
        LOG_TRACE("SysButtonDoubleClick: _buttonMode");
        // postEvent(EventApp, AppButton, SysButtonDoubleClick, pin);
    }
    else
    {
        LOG_TRACE("SysButtonDoubleClick: unsupported pin=", pin);
    }
}
void ThreadApp::handlerButtonLongPress(const Message &msg)
{
    int16_t pin = msg.uParam;
    // if (pin == _buttonBoot.getPin())
    // {
    //     LOG_TRACE("SysButtonLongPress: _buttonBoot");
    //     postEvent(EventApp, AppButton, SysButtonLongPress, pin);
    // }
    // else
    if (pin == _buttonTrig.getPin())
    {
        LOG_TRACE("SysButtonLongPress: _buttonTrig");
        // postEvent(EventApp, AppButton, SysButtonLongPress, pin);
    }
    else if (pin == _buttonMode.getPin())
    {
        LOG_TRACE("SysButtonLongPress: _buttonMode");
        // postEvent(EventApp, AppButton, SysButtonLongPress, pin);
    }
    else
    {
        LOG_TRACE("SysButtonLongPress: unsupported pin=", pin);
    }
}

void ThreadApp::handlerInference(const Message &msg)
{
    auto score = (int16_t)(msg.uParam);
    auto target = (int16_t)(msg.lParam);
    LOG_DEBUG("Inference Alarm: target=", target, ", score=", score);
    _oled.showInference(score, target);
    if (_state.lastTarget == target)
    {
        return;
    }
    _state.lastTarget = target;

    int16_t tenantCount = target == ID_TENANT ? 1 : 0;
    int16_t strangerCount = target == ID_STRANGER ? 1 : 0;
    if (tenantCount > 0 || strangerCount > 0)
    {
        LOG_TRACE("Detected ", tenantCount, " tenant(s) and ", strangerCount, " stranger(s)");
        auto ctx = reinterpret_cast<AppContext *>(context());
        postEvent(ctx->threadEth, EventApp, AppSendAlert, tenantCount, strangerCount);
    }
}

/////////////////////////////////////////////////////////////////////////////
void ThreadApp::initRTC(bool from_system_time)
{
    auto result = from_system_time ? rtc::initFromSystemTime() : rtc::initFromCompileTime();
    LOG_DEBUG("init RTC: result=", result ? "success" : "failure");
}
void ThreadApp::updateRTC(void)
{
    datetime_t t;
    char timestamp[9]; // Enough for "HH:MM:SS\0"

    // Retrieve current time from hardware RTC
    if (rtc_get_datetime(&t))
    {
        // Convert to string format hh:mm:ss
        // %02d ensures leading zeros (e.g., 09:05:01)
        snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d", t.hour, t.min, t.sec);

        LOG_TRACE("Current Time: ", timestamp);
        _oled.showRTC(timestamp);
    }
    else
    {
        LOG_ERROR("Failed to retrieve datetime from hardware RTC");
    }
}