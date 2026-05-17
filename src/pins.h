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

#define W55RP20_SMART_DOORBELL_V1_0_0 // for w55rp20-based smart doorbell v1.0.0

///////////////////////////////////////////////////////////////////////////////
// w55rp20-based smart doorbell v1.0.0
#ifdef W55RP20_SMART_DOORBELL_V1_0_0

#define PIN_I2C1_SDA 2u
#define PIN_I2C1_SCL 3u

#define PIN_I2C0_SDA 4u
#define PIN_I2C0_SCL 5u

#define PIN_LED_LD7 10u
#define PIN_LED_LD8 11u

#define PIN_DIP_SWITCH1 12u
#define PIN_DIP_SWITCH2 13u

#define PIN_SW_TRIG 14u
#define PIN_SW_MODE 15u
// #define PIN_SW_FAC 18u
#define PIN_LED_GREEN 19u

#define PIN_ETH_CS 20u // nCS = GPIO20

#endif // W55RP20_SMART_DOORBELL_V1_0_0
