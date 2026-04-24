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
#include <Arduino.h>

#include "./src/ArduProfApp.h"
#include "./src/AppContext.h"
#include "./src/thread/QueueMain.h"
#include "./src/peripheral/DipSwtich1.h"
#include "./src/peripheral/DipSwtich2.h"

///////////////////////////////////////////////////////////////////////////////
static void initGlobalVar(void)
{
}

static void startTasks(void)
{

    auto ctx = getAppContext();
    if (ctx->queueMain)
    {
        static_cast<QueueMain *>(ctx->queueMain)->start(ctx);
    }
    if (ctx->threadApp)
    {
        ctx->threadApp->start(ctx);
    }
    if (ctx->threadEth)
    {
        auto sw = DipSwitch1();
        auto value = sw.read();
        LOG_TRACE("DIP switch-1 value = ", value);
        if (value)
        {
            ctx->threadEth->start(ctx);
        }
    }    
    if (ctx->threadAi)
    {
        auto sw = DipSwitch2();
        auto value = sw.read();
        LOG_TRACE("DIP switch-2 value = ", value);
        if (value)
        {
            ctx->threadAi->start(ctx);
        }
    }
}

// set debug port to USB/CDC if USB/CDC is found without INIT_DEBUG_PORT_TIMEOUT
// otherwise, set debug port to Serial0 (UART0)
#define INIT_DEBUG_PORT_TIMEOUT 1000 // in unit of ms
static void initDebugPort(void)
{
    int timeout = 0;

    // Serial.setDebugOutput(false);
    Serial.begin(115200);
    while (!Serial && timeout < INIT_DEBUG_PORT_TIMEOUT)
    {
        delay(100);
        timeout += 100;
    }
    delay(500);

    /////////////////////////////////////////////////////////////////////////////
    // set log output to serial port, and init log params such as log_level
    LOG_SET_LEVEL(DefaultLogLevel);
    // LOG_SET_LEVEL(DebugLogLevel::LVL_TRACE); // enable debug log
    // LOG_SET_LEVEL(DebugLogLevel::LVL_NONE);  // disable debug log
    LOG_SET_DELIMITER("");
    /////////////////////////////////////////////////////////////////////////////

    if (Serial)
    {
        LOG_ATTACH_SERIAL(Serial);
        LOG_TRACE("set debug port to USB/CDC");
    }
    else
    {
        Serial1.begin(115200);
        LOG_ATTACH_SERIAL(Serial1);
        LOG_TRACE("set debug port to UART0");
    }
}

///////////////////////////////////////////////////////////////////////////////
void setup()
{
    initDebugPort();

    initGlobalVar();
    startTasks();
}

void loop()
{
    // static int count = 0;
    // LOG_TRACE("count=", count++);

    auto ctx = getAppContext();
    auto qMain = static_cast<QueueMain *>(ctx->queueMain);
    assert(qMain);
    qMain->messageLoop(0); // non-blocking
    // qMain->messageLoop();  // blocking until event received and proceed
    // qMain->messageLoopForever(); // never return

    // delay(1000);    // delay 1s
    delay(100); // delay 0.1s
}
