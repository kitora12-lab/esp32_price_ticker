/********************************************
     ESP32 BTC and USD/JPY Price  ADD Realtime Clock(JST)
     Prints current time on real time
     Forked from https://github.com/joysfera/esp32-btc
     Forked by kitora12 in '21 JAN 4
     Rebuild by kitora12 in '24 JUL 20
     released under the GNU GPL

     Tested board manager: Arduino core for the ESP32 2.0.2
     Tested boards: ESP32 DevModule, NODE32S
*********************************************/
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <NTPClient.h>  // https://github.com/arduino-libraries/NTPClient
#include <Tasker.h>     // https://github.com/joysfera/arduino-tasker
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

WiFiClientSecure client;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
Tasker tasker;

Adafruit_SSD1306 disp(128, 64, &Wire, -1);

const char* ssid     = WIFI_SSID;     // your network SSID (name of wifi network)
const char* password = WIFI_PASSWORD; // your network password
const char* server = "api.coinbase.com";  // Server URL

struct Currency {
    const char* name;
    const char* symbol;
    float value;
    const char* format;
};

Currency currencies[] = {
    {"BTC-USD", "", 888888, "%.0f"},
    {"WLD-USD", "", 888888, "%.2f"},
    {"USD-JPY", "", 888888, "%.1f"}
};
const int numCurrencies = sizeof(currencies) / sizeof(currencies[0]);

void setup()
{
    Serial.begin(115200);
    delay(100);
    pinMode(2, OUTPUT); // GPIO2 BLUE LED@Hiletgo esp-32
    digitalWrite(2, HIGH);
    delay(1000);
    digitalWrite(2, LOW);

    Wire.begin(21, 22); // SDA SCL
    if (!disp.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
        Serial.println(F("SSD1306 init failed"));
        for (;;) ; // no display no fun
    }
    disp.setTextColor(WHITE);

    disp.print("Connecting to WiFi");
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    // attempt to connect to Wifi network:
    while (WiFi.status() != WL_CONNECTED) {
        disp.print('.');
        Serial.print('.');
        // wait 1 second for re-trying
        delay(1000);
    }
    Serial.println(" OK::::::");
    Serial.println(ssid);

    timeClient.begin();
    timeClient.setTimeOffset(3600 * 9); // JST 3600 * 9 (1h *9)
                                        // CST 3600 * 2
    timeClient.update();

    displayClock();
    for (int i = 0; i < numCurrencies; i++) {
        updateCurrencyValue(i);
    }
}

int position_price_x = 256;
int position_time_x = 1;
int position_time_scroll_direction = 1;
// move L =1  move R=0
int position_time_speed_count = 0;

void loop()
{
    timeClient.update(); // keep time up-to-date
    tasker.loop();

    if (digitalRead(0) == LOW) // if button on GPIO0 is pressed then
        tasker.setTimeout(displayClock, 250); // update display almost instantly
}

void displayClock(void)
{
    disp.clearDisplay();

    // TIME Scroll
    position_time_speed_count++;
    if (position_time_speed_count > 100) { // Speed Control

        switch (position_time_scroll_direction) {
        case 0:
            position_time_x--;
            if (position_time_x < 0) {
                position_time_scroll_direction = 1;
            }
            break;

        case 1:
            position_time_x++;
            if (position_time_x > 30) {
                position_time_scroll_direction = 0;
            }
            break;
        }
        position_time_speed_count = 0;
    }

    // Price Scroll
    position_price_x--;
    if (position_price_x <= 0) {
        position_price_x = 128*numCurrencies;
    }

    disp.setTextWrap(0);
    disp.setTextSize(2);
    disp.setCursor(1 + position_time_x, 1);
    disp.print(timeClient.getFormattedTime());

    for (int i = 0; i < numCurrencies; i++) {
        displayCurrency(i, position_price_x - 128*numCurrencies - 128 * i, 20);
        displayCurrency(i, position_price_x - 128 * i, 20);
    }

    disp.display();
    tasker.setTimeout(displayClock, 1); // next round in 100msec
}

void displayCurrency(int index, int x, int y)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), currencies[index].format, currencies[index].value);

    // Replace '-' with '/'
    char nameWithSlash[16];
    strcpy(nameWithSlash, currencies[index].name);
    for (int i = 0; i < strlen(nameWithSlash); i++) {
        if (nameWithSlash[i] == '-') {
            nameWithSlash[i] = '/';
        }
    }

    disp.setTextSize(2);
    disp.setCursor(x, y);
    disp.print(nameWithSlash);

    disp.setTextSize(3);
    disp.setCursor(x, y + 20);
    disp.print(currencies[index].symbol);
    disp.print(buffer);
}

void updateCurrencyValue(int index)
{
    digitalWrite(2, HIGH);
    disp.invertDisplay(1);

    float value = 0.0;
    Serial.println("\nStarting connection to server...");
    uint32_t time_out = millis();

    while (1) {
        client.setInsecure(); // skip verification
        client.setHandshakeTimeout(30);
        if (!client.connect(server, 443)) {
            Serial.println("Connection failed!");
            value = -1;
        } else {
            Serial.println("Connected to server!");
            // Make a HTTP request:
            client.printf("GET https://api.coinbase.com/v2/prices/%s/spot HTTP/1.0\n", currencies[index].name);
            client.println("Host: api.coinbase.com");
            client.println();
            
            while (client.connected()) {
                String line = client.readStringUntil('\n');
                if (line == "\r") break; // end of HTTP headers
            }
            while (client.available()) {
                String line = client.readStringUntil('\n');
                int a = line.indexOf("amount"); // naive parsing of the JSON reply
                if (a > 0) {
                    String amount = line.substring(a + 9);
                    value = amount.toFloat();
                    Serial.printf("%s = %s%.2f\n", currencies[index].name, currencies[index].symbol, value);
                }
            }

            delay(10);
            client.stop();
            delay(10);
            Serial.print("Connection Close");
            break;
        }
        if ((millis() - time_out) > 400) break;
        delay(10);
    }
    currencies[index].value = value;
    tasker.setTimeout(updateCurrencyValueWrapper, 120000, index); // next round in 120 seconds
    digitalWrite(2, LOW);
    disp.invertDisplay(0);
}

void updateCurrencyValueWrapper(int index) {
    updateCurrencyValue(index);
}
