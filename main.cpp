/*
Simple program for displaying BMPs hosted online diretly 
on the underrated 4.7" LilyGo T5 e-paper display.

Look at settings.h file to configure it for you needs.

Made by TheUnityGuy, 
thanks David Bird (https://github.com/G6EJD) for ESP32 
deep-sleep time correction code.
*/

//system libs
#include <Arduino.h>
#include <epd_driver.h>
#include <esp_adc_cal.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <SPI.h>
#include <time.h>
#include <SPIFFS.h>
#include <DHT.h>
#include <esp_heap_caps.h>
#include <libjpeg/libjpeg.h>

//settings (variables) file
#include "settings.h"

//allocate enough for BMP storage (~2.2MB max, for 32bit RGBA even to they're not supported)
#define BMP_BUFFER_SIZE 2200000

int wifiSignal, currentMin = 0, currentSec = 0, vref = 1135;
float roomTemp = 0.0, roomHumidity = 0.0;

long startTime = 0;
long sleepTimer = 0;
//ESP32 rtc speed compensation, prevents display at xx:59:yy and then xx:00:yy (one minute later) to save power <- IMPORTANT!
long Delta = 30; 

#define DHT11_PIN  15
DHT dht11(DHT11_PIN, DHT11);

uint8_t *framebuffer;
uint8_t *bmpBuffer;

#include "opensans8b.h"
GFXfont chosenFont = OpenSans8B;

//ESP specific functions

void ESPinit() {
    startTime = millis();
    Serial.begin(115200);
    while (!Serial) ;
    Serial.println(String(__FILE__) + "\nStarting...");
    epd_init();

    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT);
    bmpBuffer = (uint8_t *)heap_caps_calloc(BMP_BUFFER_SIZE, sizeof(uint8_t), MALLOC_CAP_SPIRAM);

    if (!framebuffer || !bmpBuffer) {
        Serial.println("Memory alloc for framebuffer or bmpBuffer failed!");
        while(1);
    }

    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT);
    memset(bmpBuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT);
}

void ESPsleep() {
    epd_poweroff_all();
    sleepTimer = (sleepDuration * 60 - ((currentMin % sleepDuration) * 60 + currentSec)) + Delta;
    esp_sleep_enable_timer_wakeup(sleepTimer * 1000000LL);
    Serial.println("Operations took: " + String((millis() - startTime) / 1000.0, 3) + " seconds.");
    Serial.println("Deep-sleeping for " + String(sleepTimer) + "seconds.");
    esp_deep_sleep_start();
}

boolean setupTime() {
    configTime(gmtOffset_sec, daylightOffset_sec, const_cast<const char *>(ntpServer.c_str()), "0.pool.ntp.org");
    setenv("TZ", const_cast<const char *>(Timezone.c_str()), 1);
    tzset();
    delay(100);
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo, 5000)) {
        Serial.println("Failed to obtain time.");
        return false;
    }
    currentMin = timeinfo.tm_min;
    currentSec = timeinfo.tm_sec;
    return true;
}

uint8_t enableWiFi() {
    if (enableDebug) Serial.println("\r\nConnecting to: " + String(ssid));
    IPAddress dns(1, 1, 1, 1); //cloudflare dns for speed
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.printf("WiFi connection failed, retrying...\n");
        WiFi.disconnect(false);
        delay(500);
        WiFi.begin(ssid, password);
    } if (WiFi.status() == WL_CONNECTED) {
        wifiSignal = WiFi.RSSI();
        if (enableDebug) Serial.println("WiFi connected, your IP: " + WiFi.localIP().toString());
    } else {
        Serial.println("WiFi connection FAILED");
    }
    return WiFi.status();
}

float getBatteryVoltage() {
    esp_adc_cal_characteristics_t adc_chars;
    //calibrate adc reader based on values stored in efuse (provided by manufacturer, should be 1135 mV but who knows)
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        if (enableDebug) Serial.printf("manufacturer vref:%u mV\n", adc_chars.vref);
        vref = adc_chars.vref;
    }
    const uint8_t bat_adc_pin = 36;
    float voltage = analogRead(bat_adc_pin) / 4096.0 * 6.566 * (vref / 1000.0);
    return voltage;
}

float voltageToSoC(float v) {
    if (v >= 4.20) return 1.0; //full
    if (v <= 3.20) return 0.0; //dead

    float p = 2836.9625 * pow(v, 4)
            - 43987.4889 * pow(v, 3)
            + 255233.8134 * pow(v, 2)
            - 656689.7123 * v
            + 632041.7303;

    p /= 100.0;

    if (p > 1.0) p = 1.0;
    if (p < 0.0) p = 0.0;
    return p;
}

float estimateRuntimeHours(float soc) {
    float remaining_mAh = soc * batCapacity;
    if (enableBMP) return remaining_mAh / 0.44;
    if (enableJPG) return remaining_mAh / 0.81;
    /*
    hard to tell actually, without knowing how much current ESP32
    actually uses we can just guess. from my tests downloading and
    displaying a JPG takes 16 seconds (29 for BMP). lets say 100mA 
    average (wifi, calculations and displaying) so 0.44mAh used for 
    JPG and 0.81mAh for BMP.
    */
}

void gatherSensorsData() {
    roomTemp = dht11.readTemperature();
    roomHumidity = dht11.readHumidity();
    if (enableDebug) {
        Serial.print("Humidity: ");
        Serial.print(roomHumidity);
        Serial.print("%  |  Temperature: ");
        Serial.print(roomTemp);
        Serial.print("°C\n");
    }
}

//e-paper display specific functions

void displayString(int x, int y, String text) {
    char *data = strdup(text.c_str());
    write_string(&chosenFont, data, &x, &y, framebuffer);
}

void displayTopBar(int wifiSignal) {
    float batteryVoltage = getBatteryVoltage();
    displayString(10, 20, "Battery: "+String(batteryVoltage, 2) + " V");
    displayString(160, 20, "WiFi signal: "+String(wifiSignal)+" dBm");
    if (enableDHT) displayString(350, 20, "Room temp: "+String(roomTemp, 1)+" °C");
    if (enableEstimation) {
        float estHrs = estimateRuntimeHours(voltageToSoC(batteryVoltage));
        displayString(enableDHT ? 550 : 350, 20, "Hrs left: "+String(estHrs)+" = "+String(estHrs/24.0)+" days");
    } 
}

void edpUpdate() {
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
}

//HTTP interaction functions

void sendDataToPHP(float roomTemp, float roomHumidity, float batteryVoltage) {
    HTTPClient http;
    String postData = "temperature=" + String(roomTemp) + "&humidity=" + String(roomHumidity) + "&battery=" + String(batteryVoltage);
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.POST(postData);
    if (httpResponseCode > 0) {
        if (enableDebug) Serial.printf("HTTP Response code: %d\n", httpResponseCode);
        String response = http.getString();
        if (enableDebug) Serial.println("Server response: " + response);
    } else {
        Serial.printf("Error sending POST request: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
}

bool downloadBMP(const char *url, uint8_t *bmpBuffer, size_t *bmpSize) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        WiFiClient *stream = http.getStreamPtr();
        size_t total = http.getSize();
        size_t bytesRead = 0;

        if (enableDebug) Serial.println("Downloading BMP");
        uint8_t bmpHeader[14];
        while (http.connected() && bytesRead < 14) {
            if (stream->available()) {
                int r = stream->readBytes(bmpHeader + bytesRead, 14 - bytesRead);
                bytesRead += r;
            }
        }
        uint32_t bmpSizeN = 0;
        bmpSizeN = bmpHeader[2] | (bmpHeader[3] << 8) | (bmpHeader[4] << 16) | (bmpHeader[5] << 24);
        memcpy(bmpBuffer, bmpHeader, 14);
        while (http.connected() && bytesRead < bmpSizeN) {
            size_t available = stream->available();
            if (available) {
                int r = stream->readBytes(bmpBuffer + bytesRead, available);
                bytesRead += r;
            }
        }

        if (enableDebug) Serial.println("BMP Downloaded");
        *bmpSize = bytesRead;
        http.end();
        return true;
    } else {
        Serial.println("Failed to download BMP");
        Serial.println(httpCode);
        http.end();
        return false;
    }
}

bool downloadJPG(const char* url, uint8_t** out_buf, size_t* out_size) {
    *out_buf = nullptr;
    *out_size = 0;

    HTTPClient http;
    http.begin(url);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("HTTP GET failed: %d\n", code);
        http.end();
        return false;
    }

    int len = http.getSize();
    if (len <= 0) {
        Serial.println("No Content-Length, cannot allocate");
        http.end();
        return false;
    }

    //allocate in PSRAM
    uint8_t* buf = (uint8_t*) heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        Serial.println("PSRAM alloc failed");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t bytesRead = 0;
    const size_t chunk = 1024;

    while (bytesRead < (size_t)len) {
        if (!http.connected()) break;
        if (stream->available()) {
            size_t toRead = std::min(chunk, (size_t)len - bytesRead);
            int r = stream->readBytes(buf + bytesRead, toRead);
            if (r > 0) {
                bytesRead += r;
            } else {
                break;
            }
        } else {
            delay(1);
        }
    }

    http.end();

    if (bytesRead != (size_t)len) {
        Serial.printf("Download incomplete: got %u of %d\n", (unsigned)bytesRead, len);
        heap_caps_free(buf);
        return false;
    }

    *out_buf = buf;
    *out_size = bytesRead;
    return true;
}

//function that appends downloaded BMP into existing framebuffer (with top bar and stuff)

void appendBMPToFramebuffer(uint8_t *bmpData, size_t bmpSize, uint8_t *framebuffer) {
    if (bmpData[0] != 'B' || bmpData[1] != 'M') {
        Serial.println("Not a valid BMP file!");
        return;
    }

    uint32_t dataOffset = bmpData[10] | (bmpData[11] << 8) | (bmpData[12] << 16) | (bmpData[13] << 24);
    int32_t width = bmpData[18] | (bmpData[19] << 8) | (bmpData[20] << 16) | (bmpData[21] << 24);
    int32_t height = bmpData[22] | (bmpData[23] << 8) | (bmpData[24] << 16) | (bmpData[25] << 24);
    uint16_t bpp = bmpData[28] | (bmpData[29] << 8);

    if (bpp != 24) {
        Serial.println("Only 24bpp BMP supported");
        return;
    }

    bool topDown = height < 0;
    int absHeight = abs(height);
    int rowSize = ((width * 3 + 3) / 4) * 4;

    int yOffset = 30; //number of pixels to shift down to leave some space for top bar
    for (int y = 0; y < absHeight && (y + yOffset) < EPD_HEIGHT; y++) {
        int bmpRow = topDown ? y : (absHeight - 1 - y);
        int rowStart = dataOffset + bmpRow * rowSize;

        for (int x = 0; x < width && x < EPD_WIDTH; x++) {
            int pixelIndex = rowStart + x * 3;
            if (pixelIndex + 2 >= bmpSize) continue;

            uint8_t B = bmpData[pixelIndex + 0];
            uint8_t G = bmpData[pixelIndex + 1];
            uint8_t R = bmpData[pixelIndex + 2];

            //convert to 4-bit grayscale
            uint8_t gray = (R * 30 + G * 59 + B * 11) / 100;
            uint8_t pixel4bpp = (gray * 15 + 127) / 255;

            int pixelNum = (y + yOffset) * EPD_WIDTH + x;
            int byteIndex = pixelNum / 2;
            if (pixelNum % 2 == 0) {
                framebuffer[byteIndex] = (framebuffer[byteIndex] & 0x0F) | (pixel4bpp << 4);
            } else {
                framebuffer[byteIndex] = (framebuffer[byteIndex] & 0xF0) | (pixel4bpp & 0x0F);
            }
        }
    }
    if (enableDebug) Serial.println("BMP parsed and framebuffer filled.");
}

void loop() {
    delay(1000);
}

void setup() {
    ESPinit();
    if (enableDHT) gatherSensorsData();
    float batteryVoltage = getBatteryVoltage();

    if (enableWiFi() == WL_CONNECTED && setupTime() == true) { //enable wifi, download/send data and quickly disable wifi
        if (enableDHT) sendDataToPHP(roomTemp, roomHumidity, batteryVoltage); //send collected data to http server for analytics

        if (enableBMP) {
            size_t bmpSize = 0; //empty variable to be filled by downloadBMP() function
            bool bmpDownloaded = downloadBMP(strdup(imageUrl.c_str()), bmpBuffer, &bmpSize); //download bmp from http/s server
            if (bmpDownloaded) {
                if (enableDebug) Serial.println("BMP downloaded, parsing...");
                appendBMPToFramebuffer(bmpBuffer, bmpSize, framebuffer); //append downloaded bmp to main framebuffer
                epd_poweron(); //LilyGo-EPD43 lib
                epd_clear(); //LilyGo-EPD43 lib
            }
        }

        if (enableJPG) {
            Rect_t area;
            area.x = 0;
            area.y = 0;
            area.width = EPD_WIDTH;
            area.height = EPD_HEIGHT;
            uint8_t* jpg_buf;
            size_t jpg_size;

            libjpeg_init();

            if (downloadJPG(strdup(imageUrl.c_str()), &jpg_buf, &jpg_size)) {
                epd_poweron(); //LilyGo-EPD43 lib
                epd_clear(); //LilyGo-EPD43 lib
                show_jpg_from_buff(jpg_buf, jpg_size, area);
                heap_caps_free(jpg_buf); //free PSRAM after rendering
            }

            libjpeg_deinit();
        }

        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);

        if (enableTopBar) displayTopBar(wifiSignal); //display room temp, battery level and wifi signal (top bar)
        edpUpdate(); //update display with changes
        epd_poweroff_all(); //LilyGo-EPD43 lib
    }
    ESPsleep();
}













