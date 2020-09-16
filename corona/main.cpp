/*
 *  Application note: COVID-19 Corona tracker REST API data with the ESP8266-01S
 *  Version 1.1
 *  Copyright (C) 2020 Ernani Santos
 *
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#include <ArduinoJson.h>

#include <math.h>
#include <time.h>

//#define OLED_DISPLAY

// Enable only one of these below, disabling both is fine too.
#define CHECK_CA_ROOT
//#define CHECK_FINGERPRINT

// Root certificate used by services1.arcgis.com
#include <caCert.h>
//extern const unsigned char caCert[] PROGMEM;
//extern const unsigned int caCertLen;

#if (defined(CHECK_FINGERPRINT) and defined(CHECK_CA_ROOT))
#error Cant have both CHECK_CA_ROOT and CHECK_FINGERPRINT enabled
#endif

#ifdef OLED_DISPLAY
#include <Wire.h>
#include <SSD1306Wire.h>

// create display object
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_64);
#endif

const char *ssid = "";
const char *password = "";

const char *hostName = "services1.arcgis.com";
const char *dataUrl = "/0MSEUqKaxRlEPj5g/arcgis/rest/services/Coronavirus_2019_nCoV_Cases/FeatureServer/1/query?where=Country_Region%20%3D%20%27CABO%20VERDE%27&outFields=Country_Region,Last_Update,Confirmed,Recovered,Deaths,Active&outSR=4326&f=json";
const uint16_t httpsPort = 443;

void setup()
{
    #ifdef OLED_DISPLAY
    // Initialize the display
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 22, "WiFi Connecting...");
    display.display(); // Write the buffer to the display
    #endif

    Serial.begin(115200);
    Serial.println();
    Serial.println();

    // We start by connecting to a WiFi network
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.print(ssid);

    WiFi.mode(WIFI_STA);        //Station mode
    WiFi.begin(ssid, password); //Connect to WiFi
    while (WiFi.status() != WL_CONNECTED)
    { // Wait for connection
        Serial.print(".");
        delay(500);
    }
    // If connection successfull show IP address of ESP8266 in serial monitor
    Serial.println("");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Set time via NTP, as required for x.509 validation
    Serial.print(F("Setting time using SNTP"));
    configTime(-1 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2)
    {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println("done!");
    Serial.print("Current time: ");
    Serial.println(ctime(&now));
    Serial.println();
}

void printResult(String response)
{
    const size_t capacity = 2 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(6) + 110;
    DynamicJsonDocument doc(capacity);

    Serial.println(F("Parse JSON object\n"));
    // Parse JSON object
    DeserializationError error = deserializeJson(doc, response);
    if (error)
    {
        Serial.printf("deserializeJson() failed, error: %s\n", error.c_str());
        return;
    }

    JsonObject attributes = doc["attributes"];
    const char *Country_Region = attributes["Country_Region"].as<char*>();
    int64_t Last_Update = attributes["Last_Update"];
    int Confirmed = attributes["Confirmed"].as<int>();
    int Recovered = attributes["Recovered"].as<int>();
    int Deaths = attributes["Deaths"].as<int>();
    int Active = attributes["Active"].as<int>();

    // time to string
    char strtime[20];
    Last_Update = (Last_Update / pow(10, 3)); // remove the last 3 digits (000)

    time_t t = Last_Update;
    strftime(strtime, sizeof strtime, "%d/%m/%Y %H:%M:%S", localtime(&t));

    Serial.println("Pela nossa Saude #FiqueEmCasa");
    Serial.println(Country_Region);
    Serial.print("Casos Confirmados:");
    Serial.println(Confirmed);
    Serial.print("Casos Recuperados:");
    Serial.println(Recovered);
    Serial.print("Obitos:");
    Serial.println(Deaths);
    Serial.print("Casos Ativos:");
    Serial.println(Active);
    Serial.print("Atualizado em: ");
    Serial.println(strtime);
    Serial.println();

    #ifdef OLED_DISPLAY
    display.clear(); // Clear the display
    display.setFont(ArialMT_Plain_10);
    display.drawString(2, 1, "COVID-19");
    display.drawString(2, 18, Country_Region);
    display.drawHorizontalLine(0, 40, 20);

    display.setFont(ArialMT_Plain_16);
    display.drawString(2, 20, "Casos:" + Confirmed);
    display.drawString(2, 24, "Recuperados:" + Recovered);
    display.drawString(2, 48, "Obitos:" + Deaths);
    display.drawString(2, 50, "Ativos:" + Active);
    display.setFont(ArialMT_Plain_10);
    display.drawString(2, 50, strtime);

    display.display(); // Write the buffer to the display
    #endif
}
// Try and connect using a WiFiClientBearSSL to specified host:port and dump URL
void loop()
{
    // Wait for WiFi connection
    if ((WiFi.status() == WL_CONNECTED))
    {
        Serial.print(F("Connecting to HTTPS Server "));
        Serial.printf("https://%s\n", hostName);

        std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

        #ifdef CHECK_CA_ROOT
        Serial.println(F("Using root certificate"));
        BearSSL::X509List cert(cacert); // PEM format
        //BearSSL::X509List cert(caCert, caCertLen); // DER format
        client->setTrustAnchors(&cert);
        #endif
        #ifdef CHECK_FINGERPRINT
        Serial.printf("Using fingerprint '%s'\n", fingerprint);
        client->setFingerprint(fingerprint);
        #endif
        #if (!defined(CHECK_CA_ROOT) and !defined(CHECK_FINGERPRINT))
        Serial.println(F("Insecure connection"));
        client->setInsecure();
        #endif
        
        HTTPClient http;

        if (http.begin(*client, hostName, httpsPort, dataUrl))
        {
            int httpCode = http.GET();
            if (httpCode > 0)
            {
                Serial.printf("[HTTPS] GET... code: %d\n\n", httpCode);
                if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
                {
                    String payload = http.getString();
                    
                    payload = payload.substring(payload.indexOf("features\":[") + 11, payload.indexOf("]}"));
                    printResult(payload);
                }
            }
            else
            {
                Serial.printf("[HTTPS] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
            }

            http.end();
        }
        else
        {
            Serial.println(F("[HTTPS] Unable to connect"));
        }
    }
    Serial.println(F("Wait 60s before next round..."));
    delay(60000);
}