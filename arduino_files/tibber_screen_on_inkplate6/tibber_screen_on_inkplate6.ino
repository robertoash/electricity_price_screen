// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#ifndef ARDUINO_ESP32_DEV
#error "Wrong board selection for this example, please select Inkplate 6 in the boards menu."
#endif

#include "Inkplate.h"            //Include Inkplate library to the sketch
#include "WiFi.h"                //Include library for WiFi
#include "driver/rtc_io.h"       //ESP32 library used for deep sleep and RTC wake up pins
#include "ezTime.h"              //Library to handle time and timezones
#include "arduino_secrets.h"     //Secrets file to store WiFi info

Inkplate display(INKPLATE_3BIT); // Create an object on Inkplate library and also set library into 1 Bit mode (BW)

const char ssid[] = SECRET_SSID;
const char *password = SECRET_PASS;

Timezone Stockholm;

bool ready_to_sleep = false;

void init_wifi() {
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_MODE_STA);
    WiFi.begin(ssid, password);

    unsigned long connectionTimeout = 15000;  // Set a timeout of 15 seconds (adjust as needed)
    unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        display.print(".");
        display.partialUpdate();

        // Check if the timeout has been reached
        if (millis() - startTime > connectionTimeout) {
            Serial.println("WiFi connection timed out");
            // You can take further actions here, such as setting a wakeup and going to deep sleep.
            set_wakeup(); // Set wakeup for the next cycle
            deep_sleep(); // Go to deep sleep
            break;  // Exit the loop to prevent indefinite waiting
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi OK! Downloading...");
        // Continue with your code
    } else {
        Serial.println("WiFi connection failed");
        // Handle the WiFi connection failure gracefully.
        // You may choose to retry the connection on the next cycle or take other actions based on your requirements.
        set_wakeup(); // Set wakeup for the next cycle
        deep_sleep(); // Go to deep sleep
    }
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

    if (!display.drawImage("http://10.20.10.50:8999/elpris.png", 0, 0))
    {
        // If is something failed (wrong filename or wrong bitmap format), write error message on the screen.
        // REMEMBER! You can only use Windows Bitmap file with color depth of 1, 4, 8 or 24 bits with no compression!
        display.println("Image open error");
        display.display();
        // Give up and try again in the next cycle
        set_wakeup(); // Set wakeup for the next cycle
        deep_sleep(); // Go to deep sleep
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
    // Calculate time to sleep
    long long TIME_TO_SLEEP_SECS = ((refresh_rate_mins - (current_minute % refresh_rate_mins) + 1) * 60) - current_second; // Optional for safety: + 30;

    if (TIME_TO_SLEEP_SECS < 0 || TIME_TO_SLEEP_SECS > 3600) {
        Serial.println("Invalid sleep time calculated");
        // Handle invalid sleep time, e.g., set a default sleep time
        TIME_TO_SLEEP_SECS = 3600;
    }

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

    unsigned long timeoutStartTime = millis();
    unsigned long deepSleepTimeout = 60000; // Set a timeout of 60 seconds (adjust as needed)

    while (!ready_to_sleep) {
        delay(1);
        Serial.println("Waiting to be sleep_ready...");

        // Check if the timeout has been reached
        if (millis() - timeoutStartTime > deepSleepTimeout) {
            Serial.println("Deep sleep timeout reached, no valid wake-up time set. Forcing sleep for 30 minutes...");
            // Implement an alternative action here, e.g., retrying wake-up time setting.
            esp_sleep_enable_timer_wakeup(1800 * 1000000);
            esp_deep_sleep_start();    
        }
    }

    if (ready_to_sleep) {
        esp_deep_sleep_start();
    }
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

void setup()
{
    void (*init_wifi_f)() = init_wifi;
    void (*init_ezTime_f)() = init_ezTime;
    void (*display_image_f)() = display_image;
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
