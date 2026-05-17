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
#include <UrlEncode.h>
#include "Callmebot.h"
#include "../ArduProfApp.h"

////////////////////////////////////////////////////////////////////////////////////////////
const char Callmebot::_apiHost[] = CALLMEBOT_HOST;
const char Callmebot::_apiPath[] = CALLMEBOT_PATH;
const int Callmebot::_apiPort = CALLMEBOT_PORT;

///////////////////////////////////////////////////////////////////////////////
Callmebot::Callmebot(WiFiClientClass &client) : _client(client)
{
}

bool Callmebot::send(const char *text)
{
    // LOG_TRACE("sending:", text);
    prepareText(text);

    LOG_TRACE("connecting to host=", _apiHost, ", port=", _apiPort);
    if (!_client.connect(_apiHost, _apiPort))
    {
#if CALLMEBOT_PORT == 443
        _client.getLastSSLError(_shareRxBuf, sizeof(_shareRxBuf));
        LOG_TRACE("Fail to connect server=", _apiHost, ", port=", _apiPort, ", SSL error=", (char *)_shareRxBuf);
#else
        LOG_TRACE("Fail to connect server=", _apiHost, ", port=", _apiPort);
#endif

        _client.stop();
        return false;
    }

    writeText(_messageText);
    int code = 0;
    if (!readHttpResponse(&code))
    {
        LOG_TRACE("readHttpResponse() returns false");
        _client.stop();
        return false;
    }
    LOG_TRACE("Response code: ", code);
    return (code == 200);
}

bool Callmebot::readHttpResponse(int *ptrResponseCode)
{
    // Give BearSSL a moment to decrypt the first packet
    unsigned long waitStart = millis();
    while (_client.available() == 0 && millis() - waitStart < 3000)
    {
        vTaskDelay(pdMS_TO_TICKS(100)); // Crucial: gives the background WiFi processor time to work
        if (!_client.connected() && _client.available() == 0)
            break;
    }

    bool isHttpStatusLineReceived = false;
    while (_client.available())
    {
        String line = _client.readStringUntil('\n');
        // LOG_TRACE(line); // Print every header
        if (line == "\r" || line.length() == 0)
            break;

        char *strCode = strstr(line.c_str(), "HTTP/");
        if (!strCode)
        {
            continue;
        }

        isHttpStatusLineReceived = true;
        sscanf(strCode, "HTTP/%*d.%*d %d", ptrResponseCode);
        LOG_DEBUG("HTTP Status Code: ", *ptrResponseCode);
    }

    int rxSize = _client.available();
    while (rxSize > 0)
    {
        // LOG_DEBUG("rxSize=", rxSize);
        if (rxSize >= sizeof(_shareRxBuf))
        {
            rxSize = sizeof(_shareRxBuf) - 1; // truncate data if oversize
        }
        _client.read((uint8_t *)_shareRxBuf, rxSize);
        _shareRxBuf[rxSize] = '\0';
        // LOG_DEBUG("Received ", rxSize, " bytes from ", _client.remoteIP(), ":", _client.remotePort());
        // // LOG_DEBUG("Content: ", (const char *)_shareRxBuf);

        rxSize = _client.available();
    }

    return isHttpStatusLineReceived;
}

void Callmebot::writeText(const char *text)
{
    LOG_DEBUG("_messageText=", text);

    // Make a HTTP request:
    _client.print("GET ");
    _client.print(text);
    _client.println(" HTTP/1.1");
    _client.print("Host: ");
    _client.println(_apiHost);
    _client.println("User-Agent: Team/1.0");
    _client.println("Connection: close");
    _client.println();
}

// make text data to be sent
void Callmebot::prepareText(const char *text)
{
    String encoded = urlEncode(text);
    snprintf(_messageText, sizeof(_messageText), "%s%s", _apiPath, encoded.c_str());
    // LOG_TRACE("_messageText=", _messageText);
}
