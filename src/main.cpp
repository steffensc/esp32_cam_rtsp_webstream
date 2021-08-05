#include <Arduino.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>

#include "esp_camera.h"
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_M5STACK_NO_PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_WROVER_KIT
#include "camera_pins.h"


#define DEBUG true

// WIFI
#include "wifi_credentials.h"
const char *wifi_ssid = WIFI_SSID;
const char *wifi_password = WIFI_PASS;
#define WIFI_CONN_CHECK_INTERVAL 1000
#define WIFI_CONN_MAX_CHECK_COUNT 5

#define MAX_UDP_PACKET_SIZE 1400

IPAddress server_ip(185, 162, 249, 50);
uint16_t server_port = 9443;

WiFiUDP udp;

byte frame_no;

/* - - - - - - METHODS - - - - - - */
boolean connectWifi(const char *ssid , const char *password, int interval, int max_con_count) {
  // Try to connect
  #if (DEBUG)
  Serial.println("Connecting to wifi...");
  #endif
  WiFi.begin(ssid, password);

  int wifiConnCheckCount = 0;
  while (WiFi.status() != WL_CONNECTED && wifiConnCheckCount < max_con_count)
  {
    delay(interval);
    wifiConnCheckCount++;
  }

  // Check if connected
  if (WiFi.status() != WL_CONNECTED)
  {
    #if (DEBUG)
    Serial.println("Could not connect to wifi");
    #endif
    WiFi.disconnect();
    return false;
  }
  else
  {
    #if (DEBUG)
    Serial.println("Wifi connected. IP address: " + WiFi.localIP().toString());
    #endif
    return true;
  }
}

boolean sendImageUDP(uint8_t *data_pic, size_t size_pic) {
  if ((WiFi.status() == WL_CONNECTED))
  {
    #if (DEBUG)
    Serial.println("Image size: " + String(size_pic));
    #endif
    /*
    for (size_t i = 0; i < 1500; i++)
    {
      Serial.print(*(data_pic + i), HEX);
      Serial.print(" ");
    }
    Serial.println();
    */

    /*
    byte packet_identicator[2];
    uint8_t* ptr = packet_identicator;
    for (int i = 0; i < 10; i++)
    {
      udp.beginPacket(server_ip, server_port);
      packet_identicator[0] = 0x41;
      packet_identicator[1] = 0x41;
      udp.write(*packet_identicator, 2);
      udp.write(65 + i);
      //udp.write(i);
      udp.endPacket();
    }
    */
    

    byte send_packets_num = 0;
    byte packet_identicator[3];
    uint8_t* packet_identicator_ptr = packet_identicator;
    int remaining_bytes = size_pic;
    byte packet_num = size_pic / MAX_UDP_PACKET_SIZE;

    while (remaining_bytes > 0)
    {
      #if (DEBUG)
      Serial.println("Send packet: " + String(send_packets_num));
      #endif

      packet_identicator[0] = frame_no;
      packet_identicator[1] = send_packets_num;
      packet_identicator[2] = packet_num;
    
      if (remaining_bytes > MAX_UDP_PACKET_SIZE){
        udp.beginPacket(server_ip, server_port);
        udp.write(packet_identicator_ptr, 3);
        udp.write(data_pic + (send_packets_num * MAX_UDP_PACKET_SIZE), MAX_UDP_PACKET_SIZE);
        udp.endPacket();

        remaining_bytes = remaining_bytes - MAX_UDP_PACKET_SIZE;
        send_packets_num ++;
      }
      else{
        udp.beginPacket(server_ip, server_port);
        udp.write(packet_identicator_ptr, 3);
        udp.write(data_pic + (send_packets_num * MAX_UDP_PACKET_SIZE), remaining_bytes);
        udp.endPacket();

        #if (DEBUG)
        Serial.println("Sended last packet.");
        #endif

        break;
      }
    }
    return true;

  }
  return false;
}

void init_camera() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_XGA;
    config.jpeg_quality = 20;
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    #if (DEBUG)
    Serial.printf("Camera init failed with error 0x%x", err);
    #endif
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  
  s->set_vflip(s, 1); //flip it back
  s->set_hmirror(s, 1);
  //s->set_hmirror(s, 0);
  s->set_brightness(s, 1); //up the blightness just a bit
  s->set_saturation(s, 2); //up the saturation
  s->set_contrast(s, 2);

  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_XGA);

  #if (DEBUG)
  Serial.println("Camera initialized");
  #endif
}

esp_err_t camera_capture(){
  bool wifi_success = false;

  //acquire a frame
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
      ESP_LOGE(TAG, "Camera Capture Failed");
      return ESP_ERR_CAMERA_BASE;
  }

    //replace this with your own function
    //process_image(fb->width, fb->height, fb->format, fb->buf, fb->len);

    //Serial.print(String(fb->width) + "x" + String(fb->height));
  

    if(fb->format == PIXFORMAT_JPEG){
      //res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
      Serial.println(" JPEG");
      frame_no ++;

      wifi_success = sendImageUDP(fb->buf, fb->len);
    }

    esp_camera_fb_return(fb); //return the frame buffer back to the driver for reuse

    if(wifi_success == false){
      return ESP_ERR_WIFI_BASE;
    }
    
    return ESP_OK;
}



void setup() {
  // put your setup code here, to run once:

  #if (DEBUG)
  // SERIAL //
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor
  #endif

  init_camera();

  // Start WiFi
  connectWifi(wifi_ssid, wifi_password, WIFI_CONN_CHECK_INTERVAL, WIFI_CONN_MAX_CHECK_COUNT);
}

void loop() {
  // put your main code here, to run repeatedly:

  esp_err_t esp_status = camera_capture();

  // Error hadling
  if(esp_status != ESP_OK){

    if(esp_status == ESP_ERR_WIFI_BASE){
      connectWifi(wifi_ssid, wifi_password, WIFI_CONN_CHECK_INTERVAL, WIFI_CONN_MAX_CHECK_COUNT);
    }

    if(esp_status == ESP_ERR_CAMERA_BASE){
      init_camera();
    }

  }

  delay(100);
}