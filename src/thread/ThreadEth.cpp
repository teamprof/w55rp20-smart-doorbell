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
#include "./ThreadEth.h"
#include "../AppContext.h"
#include "../pins.h"
#include "../secret.h"

////////////////////////////////////////////////////////////////////////////////////////////
#define SSL_TIMEOUT 10000
#define SSL_RX_BUFFER_SIZE 4096
#define SSL_TX_BUFFER_SIZE 1024

////////////////////////////////////////////////////////////////////////////////////////////
ThreadEth *ThreadEth::_instance = nullptr;

ThreadEth *ThreadEth::getInstance(void)
{
    if (!_instance)
    {
        static ThreadEth instance;
        _instance = &instance;
    }
    return _instance;
}

#if defined ARDUPROF_FREERTOS && defined ARDUINO_ARCH_RP2040
////////////////////////////////////////////////////////////////////////////////////////////
// Thread for FreeRTOS RP2040/RP2350
////////////////////////////////////////////////////////////////////////////////////////////

// static constexpr UBaseType_t uxCoreAffinityMask = ((1 << 0)); // task only run on core 0
static constexpr UBaseType_t uxCoreAffinityMask = ((1 << 1)); // task only run on core 1
// static constexpr uxCoreAffinityMask = ( ( 1 << 0 ) | ( 1 << 2 ) );  // e.g. task can only run on core 0 and core 2

#define TASK_NAME "ThreadEth"
// #define TASK_STACK_SIZE (16384 / sizeof(StackType_t))
#define TASK_STACK_SIZE (4096 / sizeof(StackType_t))
#define TASK_PRIORITY 3 // Priority, (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
// #define TASK_PRIORITY 6   // Priority, (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
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
ThreadEth::ThreadEth() : ThreadBase(TASK_QUEUE_SIZE, ucQueueStorageArea, &xStaticQueue),
                         _eth(PIN_ETH_CS),
                         _client(),
                         _callmebot(_client),
                         _handlerMap()
{
    _instance = this;

    _handlerMap = {
        __EVENT_MAP(ThreadEth, EventApp),
        __EVENT_MAP(ThreadEth, EventNull), // {EventNull, &ThreadEth::handlerEventNull},
    };
}

void ThreadEth::start(void *ctx)
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

#define TASK_NAME "ThreadEth"
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
ThreadEth::ThreadEth() : ardufreertos::ThreadBase(TASK_QUEUE_SIZE, ucQueueStorageArea, &xStaticQueue),
                         _handlerMap()
{
    _instance = this;

    // setup event handlers
    _handlerMap = {
        __EVENT_MAP(ThreadEth, EventNull), // {EventNull, &ThreadEth::handlerEventNull},
    };
}

void ThreadEth::start(void *ctx)
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
ThreadEth::ThreadEth() : ardumbedos::ThreadBase(&threadQueue),
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
        __EVENT_MAP(ThreadEth, EventApp),
        __EVENT_MAP(ThreadEth, EventNull), // {EventNull, &ThreadEth::handlerEventNull},
    };
}

void ThreadEth::start(void *ctx)
{
    LOG_TRACE("core", get_core_num(), ", ctx=(hex)", DebugLogBase::HEX, (uint32_t)ctx);
    ThreadBase::start(ctx);
}

#endif

void ThreadEth::setup(void)
{
    LOG_TRACE("core", get_core_num(), ", uxTaskPriorityGet(NULL)=", uxTaskPriorityGet(NULL));

    ThreadBase::setup();

    auto ctx = reinterpret_cast<AppContext *>(context());
    ASSERT(ctx->threadApp);

    if (!_eth.begin())
    {
        LOG_ERROR("No wired Ethernet hardware detected. Check pinouts, wiring.");
        postEvent(ctx->threadApp, EventEth, EthErrHardware);
        _handlerMap.clear(); // clear handler map to disable event handling when no hardware detected
        return;
    }

    LOG_TRACE("Waiting for Ethernet connection...");
    while (!_eth.connected())
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    auto ip = _eth.localIP();
    LOG_TRACE("IP address: ", ip);
    postEvent(ctx->threadApp, EventEth, EthUp, 0, (uint32_t)ip);

    // Set time via NTP
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    LOG_TRACE("Waiting for NTP time sync...");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2)
    { // Wait until time is synced
        delay(500);
        now = time(nullptr);
    }
    LOG_TRACE("Time synced");
    postEvent(ctx->threadApp, EventEth, EthNtpSync);

#if CALLMEBOT_PORT == 443
    _client.setCACert(root_ca);
    // _client.setInsecure();

    _client.setTimeout(SSL_TIMEOUT);
    _client.setBufferSizes(SSL_RX_BUFFER_SIZE, SSL_TX_BUFFER_SIZE);

    LOG_TRACE("SSL initialized");
#endif

    // vTaskDelay(pdMS_TO_TICKS(1000));
}

/////////////////////////////////////////////////////////////////////////////
void ThreadEth::onMessage(const Message &msg)
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
__EVENT_FUNC_DEFINITION(ThreadEth, EventApp, msg) // void ThreadEth::handlerEventApp(const Message &msg)
{
    auto src = static_cast<AppTriggerSource>(msg.iParam);
    switch (src)
    {
    case AppSendAlert:
        handlerSendAlert(msg);
        break;

    default:
        // DBGLOG(Debug, "Unsupported src=%d, uParam=%u, lParam=%lu", src, msg.uParam, msg.lParam);
        LOG_TRACE("Unsupported src=", src, ", uParam=", msg.uParam, ", lParam=", msg.lParam);
        break;
    }
}

// define EventNull handler
__EVENT_FUNC_DEFINITION(ThreadEth, EventNull, msg) // void ThreadEth::handlerEventNull(const Message &msg)
{
    LOG_TRACE("EventNull(", msg.event, "), iParam=", msg.iParam, ", uParam=", msg.uParam, ", lParam=", msg.lParam);
}

/////////////////////////////////////////////////////////////////////////////
void ThreadEth::handlerSendAlert(const Message &msg)
{
    uint16_t tenantNum = msg.uParam;
    uint16_t strangerNum = msg.lParam;
    // LOG_DEBUG("handlerSendAlert: tenantNum=", tenantNum, ", strangerNum=", strangerNum);

    static const char *strTenant = "tenant identified";
    static const char *strStranger = "stranger detected";
    const char *alertMsg = tenantNum > 0 ? strTenant : strangerNum > 0 ? strStranger
                                                                       : nullptr;
    if (!alertMsg)
    {
        LOG_WARN("unsupported tenantNum=", tenantNum, ", strangerNum=", strangerNum);
        return;
    }

    LOG_DEBUG("alertMsg = ", alertMsg);

    auto ctx = reinterpret_cast<AppContext *>(context());
    if(_callmebot.send(alertMsg))
    {
        LOG_DEBUG("Alert sent successfully");
        postEvent(ctx->threadApp, EventApp, AppSentSuccess);
    }
    else
    {
        LOG_WARN("Failed to send alert");
        postEvent(ctx->threadApp, EventApp, AppSentFail);
    }
}
