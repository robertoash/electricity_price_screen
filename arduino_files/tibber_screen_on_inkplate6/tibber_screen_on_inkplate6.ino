/*
   Web_BMP_pictures example for e-radionica Inkplate6
   For this example you will need a micro USB cable, Inkplate6, and an available WiFi connection.
   Select "Inkplate 6(ESP32)" from Tools -> Board menu.
   Don't have "Inkplate 6(ESP32)" option? Follow our tutorial and add it:
   https://e-radionica.com/en/blog/add-inkplate-6-to-arduino-ide/

   You can open .bmp files that have color depth of 1 bit (BW bitmap), 4 bit, 8 bit and
   24 bit AND have resoluton smaller than 800x600 or otherwise it won't fit on screen.

   This example will show you how you can download a .bmp file (picture) from the web and
   display that image on e-paper display.

   Want to learn more about Inkplate? Visit www.inkplate.io
   Looking to get support? Write on our forums: http://forum.e-radionica.com/en/
   23 July 2020 by e-radionica.com
*/

// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#ifndef ARDUINO_ESP32_DEV
#error "Wrong board selection for this example, please select Inkplate 6 in the boards menu."
#endif

#include "Inkplate.h"            //Include Inkplate library to the sketch
#include "WiFi.h"                //Include library for WiFi
#include "driver/rtc_io.h"       //ESP32 library used for deep sleep and RTC wake up pins
#include "ezTime.h"              //Library to handle time and timezones
#include "arduino_secrets.h"             //Secrets file to store WiFi info

Inkplate display(INKPLATE_3BIT); // Create an object on Inkplate library and also set library into 1 Bit mode (BW)

const char ssid[] = SECRET_SSID;
const char *password = SECRET_PASS;

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "192.168.1.100";

Timezone Stockholm;

bool ready_to_sleep = false;

void init_wifi()
{
    Serial.println("Connecting to WiFi...");
    //display.print("Connecting to WiFi...");
    //display.partialUpdate();

    // Connect to the WiFi network.
    WiFi.mode(WIFI_MODE_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        display.print(".");
        display.partialUpdate();
    }
    Serial.println("WiFi OK! Downloading...");
    //display.println("\nWiFi OK! Downloading...");
    //display.partialUpdate();
}

void init_ezTime()
{
    setServer("europe.pool.ntp.org");
    waitForSync();
    Serial.println("UTC: " + UTC.dateTime());
    Stockholm.setLocation("Europe/Stockholm");
    Serial.println("Stockholm time: " + Stockholm.dateTime());
}

void display_image()
{
    // Draw the first image from web.
    // Monochromatic bitmap with 1 bit depth. Images like this load quickest.
    // NOTE: Both drawImage methods allow for an optional fifth "invert" parameter. Setting this parameter to true
    // will flip all colors on the image, making black white and white black. This may be necessary when exporting
    // bitmaps from certain softwares. Forth parameter will dither the image. Photo taken by: Roberto Fernandez

    if (!display.drawImage("http://192.168.1.161:8999/elpris.png", 0, 0))
    {
        // If is something failed (wrong filename or wrong bitmap format), write error message on the screen.
        // REMEMBER! You can only use Windows Bitmap file with color depth of 1, 4, 8 or 24 bits with no compression!
        display.println("Image open error");
        display.display();
    }
    display.display();

    Serial.println("Image displayed...");
}

void set_wakeup()
{
    Serial.println("Setting wakeup...");

    int current_hour = hour(Stockholm.now());
    int current_minute = minute(Stockholm.now());
    int current_second = second(Stockholm.now());

    int refresh_rate_mins = 60;

    if (current_hour == 13){
        refresh_rate_mins = 15;
    }

    int MINUTES_LEFT = (refresh_rate_mins - (current_minute % refresh_rate_mins) + 1);
    long long TIME_TO_SLEEP_SECS = ((refresh_rate_mins - (current_minute % refresh_rate_mins) + 1) * 60) - current_second; // Optional for safety: + 30;

    #define uS_TO_S_FACTOR 1000000LL // Conversion factor for micro seconds to seconds

    // DEbug prints
    Serial.print("Current minute: ");
    Serial.println(current_minute);
    Serial.print("Seconds to wakeup: ");
    Serial.println(TIME_TO_SLEEP_SECS);
    Serial.print("Minutes to wakeup: ");
    Serial.println(MINUTES_LEFT);

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_SECS * uS_TO_S_FACTOR); // Set EPS32 to be woken up

    // DEbug prints
    Serial.print("Wakeup set to ");
    Serial.print(TIME_TO_SLEEP_SECS * uS_TO_S_FACTOR);
    Serial.println(" microseconds from now...");

    ready_to_sleep = true;
}

void deep_sleep()
{
    Serial.println("Going to deep sleep...");
    rtc_gpio_isolate(GPIO_NUM_12); // Isolate/disable GPIO12 on ESP32 (only to reduce power consumption in sleep)
    WiFi.mode(WIFI_OFF);
    // Go to sleep
    while (ready_to_sleep == false){
        delay(1);
        Serial.println("Waiting to be sleep_ready...");
    } ;
    esp_deep_sleep_start();
}


void print_wakeup_reason(){
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch(wakeup_reason){
        case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
        default: Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
    }
}

/*
void report_battery_mqtt()
{

    Serial.println("Reporting battery to MQTT...");

    // Connect to MQTT
    WiFiClient espClient;
    PubSubClient client(espClient);
    client.setServer(mqtt_server, 1883);

    int connect_retries = 0;

    while (!client.connected() && connect_retries <= 10) {
        connect_retries++;
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("InkplateClient", "mqtt", "1979198229")) {
            Serial.println("MQTT Connected...");
        } else {
            Serial.print("failed on attempt No. ");
            Serial.print(connect_retries);
            Serial.print(" with reason code ");
            Serial.print(client.state());
            Serial.println(". Trying again in 5 seconds...");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }

    if (client.connected()) {
        client.loop();
    } else {
        Serial.print("Could not connect to MQTT Server...");
        return;
    }

    int temperature;
    float voltage;

    // Read temperature from on-board temperature sensor
    temperature = display.readTemperature();

    // Convert the value to a char array
    char tempString[8];
    dtostrf(temperature, 1, 2, tempString);
    Serial.print("Temperature: ");
    Serial.println(tempString);
    client.publish("devices/inkplate_tibber/temperature", tempString, true);

    // Read battery voltage
    voltage = display.readBattery();

    // Convert the value to a char array
    char voltageString[8];
    dtostrf(voltage, 1, 2, voltageString);
    Serial.print("Voltage: ");
    Serial.println(voltageString);
    client.publish("devices/inkplate_tibber/voltage", voltageString, true);


    Serial.println("Battery reported...");
}
*/

void setup()
{
    void (*init_wifi_f)() = init_wifi;
    void (*init_ezTime_f)() = init_ezTime;
    void (*display_image_f)() = display_image;
    // void (*report_battery_mqtt_f)() = report_battery_mqtt;
    void (*set_wakeup_f)() = set_wakeup;
    void (*deep_sleep_f)() = deep_sleep;
    void (*print_wakeup_reason_f)() = print_wakeup_reason;

    Serial.begin(115200);
    delay(500); //Take some time to open up the Serial Monitor
    Serial.println("Good Morning...");
    print_wakeup_reason_f();

    display.begin();        // Init Inkplate library (you should call this function ONLY ONCE)
    Serial.println("Display started...");
    display.clearDisplay(); // Clear frame buffer of display
    // display.display();      // Put clear image on display

    ready_to_sleep = false;

    init_wifi_f();
    display_image_f();
    // report_battery_mqtt_f();
    init_ezTime_f();
    set_wakeup_f();
    deep_sleep_f();
}

void loop()
{
    // here is where you'd put code that needs to be running all the time.
}
