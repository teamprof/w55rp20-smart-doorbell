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
#include "./ThreadAi.h"
#include "../AppContext.h"
#include "../pins.h"
#include "../type/ConstAi.h"


////////////////////////////////////////////////////////////////////////////////////////////
ThreadAi *ThreadAi::_instance = nullptr;

ThreadAi *ThreadAi::getInstance(void)
{
    if (!_instance)
    {
        static ThreadAi instance;
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

#define TASK_NAME "ThreadAi"
#define TASK_STACK_SIZE (16384 / sizeof(StackType_t))
// #define TASK_STACK_SIZE (4096 / sizeof(StackType_t))
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

///////////////////////////////////////////////////////////////////////
ThreadAi::ThreadAi() : ThreadBase(TASK_QUEUE_SIZE, ucQueueStorageArea, &xStaticQueue),
                       _ai(),
                       _kf(KF_E_MEA, KF_E_EST, KF_Q),
                       _ledLD7(),
                       _timer1Hz("Timer 1Hz",
                                 pdMS_TO_TICKS(1000),
                                 [](TimerHandle_t xTimer)
                                 {
                                     if (_instance)
                                     {
                                         auto context = reinterpret_cast<AppContext *>(_instance->context());
                                         if (context && context->threadAi)
                                         {
                                             static_cast<ThreadAi *>(context->threadAi)->postEvent(EventSystem, SysSoftwareTimer, 0, (uint32_t)xTimer);
                                         }
                                     }
                                 }),
                       _timerInference("Timer Inference",
                                       pdMS_TO_TICKS(500),
                                       [](TimerHandle_t xTimer)
                                       {
                                           if (_instance)
                                           {
                                               auto context = reinterpret_cast<AppContext *>(_instance->context());
                                               if (context && context->threadAi)
                                               {
                                                   static_cast<ThreadAi *>(context->threadAi)->postEvent(EventSystem, SysSoftwareTimer, 0, (uint32_t)xTimer);
                                               }
                                           }
                                       }),
                       _handlerMap()
{
    _instance = this;

    _handlerMap = {
        __EVENT_MAP(ThreadAi, EventSystem),
        __EVENT_MAP(ThreadAi, EventNull), // {EventNull, &ThreadAi::handlerEventNull},
    };
}

void ThreadAi::start(void *ctx)
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

#define TASK_NAME "ThreadAi"
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
ThreadAi::ThreadAi() : ardufreertos::ThreadBase(TASK_QUEUE_SIZE, ucQueueStorageArea, &xStaticQueue),
                       _handlerMap()
{
    _instance = this;

    // setup event handlers
    _handlerMap = {
        __EVENT_MAP(ThreadAi, EventNull), // {EventNull, &ThreadAi::handlerEventNull},
    };
}

void ThreadAi::start(void *ctx)
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
ThreadAi::ThreadAi() : ardumbedos::ThreadBase(&threadQueue),
                       _handlerMap(),
                       _ledGreen(),
                       _kf(KF_E_MEA, KF_E_EST, KF_Q),
                       _state({0})
/////////////////////////////////////////////////////////////////////////////
// threadQueue is dynamically allocate from heap
// ThreadAi::ThreadAi() : ThreadBase(THREAD_QUEUE_SIZE),
//                          _handlerMap()
/////////////////////////////////////////////////////////////////////////////
{
    _handlerMap = {
        __EVENT_MAP(ThreadAi, EventNull), // {EventNull, &ThreadAi::handlerEventNull},
    };
}

void ThreadAi::start(void *ctx)
{
    LOG_TRACE("core", get_core_num(), ", ctx=(hex)", DebugLogBase::HEX, (uint32_t)ctx);
    ThreadBase::start(ctx);
}

#endif

void ThreadAi::setup(void)
{
    ThreadBase::setup();

    _ledLD7.off();

    Wire1.setSDA(PIN_I2C1_SDA);
    Wire1.setSCL(PIN_I2C1_SCL);
    Wire1.begin();

    auto ctx = reinterpret_cast<AppContext *>(context());
    if (!_ai.begin(&Wire1))
    {
        LOG_DEBUG("Failed to initialize NPU");
        postEvent(ctx->threadApp, EventAi, AiErrHardware);
        _handlerMap.clear(); // clear handler map to disable event handling when no hardware detected
        return;
    }
    LOG_DEBUG("NPU initialized successfully");
    postEvent(ctx->threadApp, EventAi, AiReady);

    _timer1Hz.start();
    // _timer1Hz.stop();

    _timerInference.start();
    // _timerInference.stop();

    // vTaskDelay(pdMS_TO_TICKS(1000));
}

/////////////////////////////////////////////////////////////////////////////
void ThreadAi::onMessage(const Message &msg)
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
__EVENT_FUNC_DEFINITION(ThreadAi, EventSystem, msg) // void ThreadAi::handlerEventSystem(const Message &msg)
{
    // LOG_TRACE("EventSystem(", msg.event, "), iParam = ", msg.iParam, ", uParam = ", msg.uParam, ", lParam = ", msg.lParam);
    enum SystemTriggerSource src = static_cast<SystemTriggerSource>(msg.iParam);
    switch (src)
    {
    case SysSoftwareTimer:
        handlerSoftwareTimer((TimerHandle_t)(msg.lParam));
        break;
    // case SysButtonClick:
    //     handlerButtonClick(msg);
    //     break;
    default:
        LOG_TRACE("unsupported SystemTriggerSource=", src);
        break;
    }
}

// define EventNull handler
__EVENT_FUNC_DEFINITION(ThreadAi, EventNull, msg) // void ThreadAi::handlerEventNull(const Message &msg)
{
    LOG_TRACE("EventNull(", msg.event, "), iParam=", msg.iParam, ", uParam=", msg.uParam, ", lParam=", msg.lParam);
}
/////////////////////////////////////////////////////////////////////////////
void ThreadAi::handlerSoftwareTimer(TimerHandle_t xTimer)
{
    if (xTimer == _timer1Hz.timer())
    {
        // LOG_TRACE("_timer1Hz");
    }
    else if (xTimer == _timerInference.timer())
    {
        // LOG_TRACE("_timerInference");
        actionInference();
    }
    else
    {
        LOG_WARN("unsupported timer handle=0x%04x", (uint32_t)(xTimer));
    }
}

void ThreadAi::actionInference(void)
{
    auto ret = _ai.invoke();
    // LOG_DEBUG("AI.invoke() returns ", ret, ", CMD_OK=", CMD_OK, ", CMD_ETIMEDOUT=", CMD_ETIMEDOUT);
    if (ret != CMD_OK)
    {
        LOG_WARN("invoke failed");
        _ledLD7.off();
        return;
    }

    // LOG_DEBUG("invoke success");
    _ledLD7.on();

    auto ctx = reinterpret_cast<AppContext *>(context());

    auto boxesSize = _ai.boxes().size();
    auto classesSize = _ai.classes().size();
    auto pointsSize = _ai.points().size();

    LOG_DEBUG("perf: prepocess=", _ai.perf().prepocess, "ms, inference=", _ai.perf().inference, "ms, postprocess=", _ai.perf().postprocess, "ms");
    // LOG_DEBUG("boxes size=", boxesSize, ", classes size=", classesSize, ", points size=", pointsSize);
    // if (boxesSize == 0)
    // {
    //     postEvent(ctx->threadApp, EventAi, AiInference, -1, -1);
    //     return;
    // }

    auto score = -1;
    auto target = -1;
    if (getTargetWithHighestScore(&score, &target))
    {
        LOG_DEBUG("Inference: target=", target, ", score=", score);
        postEvent(ctx->threadApp, EventAi, AiInference, score, target);
    }
    // else
    // {
    //     LOG_DEBUG("No target detected with score above threshold");
    //     postEvent(ctx->threadApp, EventAi, AiInference, -1, -1);
    // }

    for (int i = 0; i < classesSize; i++)
    {
        LOG_DEBUG("class ", i, ": target=", _ai.classes()[i].target, ", score=", _ai.classes()[i].score);
    }
    for (int i = 0; i < pointsSize; i++)
    {
        LOG_DEBUG("point ", i, ": x=", _ai.points()[i].x, ", y=", _ai.points()[i].y, ", z=", _ai.points()[i].z, ", score=", _ai.points()[i].score, ", target=", _ai.points()[i].target);
    }
}

bool ThreadAi::getTargetWithHighestScore(int *score, int *target)
{
    int s = THRESHOLD_INFERENCE - 1;
    int t = -1;
    for (int i = 0; i < _ai.boxes().size(); i++)
    {
        if (_ai.boxes()[i].score > s)
        {
            s = _ai.boxes()[i].score;
            t = _ai.boxes()[i].target;
        }
    }
    if (s >= THRESHOLD_INFERENCE)
    {
        *score = s;
        *target = t;
        return true;
    }
    return false;
}
