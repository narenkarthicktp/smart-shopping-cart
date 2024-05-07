#include <Arduino.h>
#include <WiFi.h>
#include "HardwareSerial.h"
#include "esp32-hal-gpio.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include "HX711.h"
#include "camera.h"

#define LOADCELL_DOUT_PIN 13
#define LOADCELL_SCLK_PIN 15
#define BUILTIN_LED_PIN 33
// #define FLASH_LED_PIN 4

#define REQUEST_TIMEOUT 10000
#define LOOP_COOLDOWN 5000
#define SSID "    "
#define PASSWD "DeployNow"

HX711 scale;
WiFiClient client;
// const IPAddress server_ip = IPAddress(192, 168, 1, 22);
const IPAddress server_ip = IPAddress(192, 168, 188, 159);
const int server_port = 3000;

void init_wifi(const char* ssid, const char* passwd)
{
    WiFi.begin(ssid, passwd);
    Serial.print("Connecting");
    while(WiFi.status() != WL_CONNECTED)
    {
        Serial.print("...");
        delay(500);
        Serial.print("\b\b\b   \b\b\b");
        delay(500);
    }
    Serial.print("\nConnection established as ");
    Serial.println(WiFi.localIP());
}

bool init_camera()
{
	camera_config_t config;
	config.ledc_channel = LEDC_CHANNEL_0;
	config.ledc_timer = LEDC_TIMER_0;
	config.pin_d0 = Y2_GPIO_NUM;
	config.pin_d1 = Y3_GPIO_NUM;
	config.pin_d2 = Y4_GPIO_NUM;
	config.pin_d3 = Y5_GPIO_NUM;
	config.pin_d4 = Y6_GPIO_NUM;
	config.pin_d5 = Y7_GPIO_NUM;
	config.pin_d6 = Y8_GPIO_NUM;
	config.pin_d7 = Y9_GPIO_NUM;
	config.pin_xclk = XCLK_GPIO_NUM;
	config.pin_pclk = PCLK_GPIO_NUM;
	config.pin_vsync = VSYNC_GPIO_NUM;
	config.pin_href = HREF_GPIO_NUM;
	config.pin_sccb_sda = SIOD_GPIO_NUM;
	config.pin_sccb_scl = SIOC_GPIO_NUM;
	config.pin_pwdn = PWDN_GPIO_NUM;
	config.pin_reset = RESET_GPIO_NUM;
	config.xclk_freq_hz = 20000000;
	config.pixel_format = PIXFORMAT_JPEG;

	if(psramFound())
	{
		config.frame_size = FRAMESIZE_SVGA;
		config.jpeg_quality = 10;
		config.fb_count = 2;
		config.grab_mode = CAMERA_GRAB_LATEST;
	}
	else
	{
		config.frame_size = FRAMESIZE_CIF;
		config.jpeg_quality = 12;
		config.fb_count = 1;
		Serial.println("PSRAM is gone?!");
		delay(2000);
	}

	esp_err_t err = esp_camera_init(&config);
	return (err != ESP_OK);
}

void init_scale()
{
    Serial.println("The");
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCLK_PIN);
    scale.set_scale(228.f);
    Serial.println("end");
    delay(2000);
    scale.tare();
    Serial.println("is here");
}

void setup()
{
    Serial.begin(115200);

    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    pinMode(BUILTIN_LED_PIN, OUTPUT);
    pinMode(T6, OUTPUT);
    // pinMode(FLASH_LED_PIN, OUTPUT);

    Serial.println("initializing camera");
    if(init_camera())
    {
        Serial.println("Camera initiallization failed - restarting ESP32...");
        delay(1000);
        ESP.restart();
    }

    Serial.println("initializing Wifi");
    init_wifi(SSID, PASSWD);

    Serial.println("initializing scale");
    init_scale();
}

String send_nukes(double weight)
{
    String getAll;
    String getBody;
    String weight_string = String(weight);
    camera_fb_t* fb = NULL;
    esp_camera_fb_return(fb);
    fb = esp_camera_fb_get();

    if(!fb)
    {
        Serial.println("capture failed - restarting device");
        delay(1000);
        ESP.restart(); // <--!!
    }
    Serial.println("Connecting to " + server_ip.toString());
    if(client.connect(server_ip, server_port))
    {
        Serial.println("\rConnected to" + server_ip.toString());

            String header = "--WebKitFormBoundaryTHEENDISHERE\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
            String footer = "\r\n--WebKitFormBoundaryTHEENDISHERE--\r\n";

            String wheader = "--WebKitFormBoundaryTHEENDISHERE\r\nContent-Disposition: form-data; name=\"weight\"\r\n\r\n";

            uint32_t content_length = fb->len + header.length() + wheader.length() + weight_string.length() + footer.length();

            client.println("POST /upload HTTP/1.1");
            client.println("Host: " + server_ip.toString());
            client.println("Content-Length: " + String(content_length));
            client.println("Content-Type: multipart/form-data; boundary=WebKitFormBoundaryTHEENDISHERE");
            client.println();
            client.print(header);

            uint8_t* fb_buffer = fb->buf;
            size_t fb_length = fb->len;
            for(size_t n = 0; n < fb_length; n += 1024)
            {
                if(n+1024 < fb_length)
                {
                    client.write(fb_buffer, 1024);
                    fb_buffer += 1024;
                }
                else if(fb_length % 1024 > 0)
                {
                    size_t remainder = fb_length % 1024;
                    client.write(fb_buffer, remainder);
                }
            }
            client.println();
            client.print("--WebKitFormBoundaryTHEENDISHERE\r\nContent-Disposition: form-data; name=\"weight\"\r\n\r\n");
            client.print(weight_string);
            client.print(footer);

            esp_camera_fb_return(fb);

            long begining = millis();
            bool state;

            while(begining + REQUEST_TIMEOUT > millis())
            {
                while(client.available())
                {
                    char c = client.read();
                    if(c == '\n')
                    {
                        if(getAll.length() == 0)
                            state = true;
                        getAll = "";
                    }
                    else if(c != '\r')
                        getAll += String(c);
                    if(state == true)
                        getBody += String(c);
                    begining = millis();
                }
                if(getBody.length() > 0)
                    break;
            }
            Serial.println("\n" + getBody);
            client.stop();
    }
    else
        Serial.println("connection failed");
    return getBody;
}

unsigned long last_weight = 0;

void loop()
{
    double x = scale.get_units(20);
    int dweight = (x - last_weight);
    if(dweight > 1 || dweight < -1)
    {
        digitalWrite(BUILTIN_LED_PIN, LOW);
        send_nukes(x - last_weight);
    }
    else
        digitalWrite(BUILTIN_LED_PIN, HIGH);
    last_weight = x;
    Serial.printf("dw: %d\t| mean: %.2f\t| instant: %.2f\n", dweight, x, scale.get_units(1));
    delay(1000);
}
