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
#include <Arduino.h>
#include <WiFiClientSecure.h>

#include "../secret.h"

#define CONNECT_TIMEOUT 30 // in unit of seconds

#define MESSAGE_TX_BUFFER_SIZE 256
#define MESSAGE_RX_BUFFER_SIZE 1024

class Callmebot
{
public:
    Callmebot(WiFiClientClass &client);
    bool send(const char *text);

private:
    WiFiClientClass &_client;

    char _messageText[MESSAGE_TX_BUFFER_SIZE];
    char _shareRxBuf[MESSAGE_RX_BUFFER_SIZE];

    bool readHttpResponse(int *ptrResponseCode);
    void writeText(const char *text);
    void prepareText(const char *text);

    static const char _apiHost[];
    static const char _apiPath[];
    static const int _apiPort;
};
