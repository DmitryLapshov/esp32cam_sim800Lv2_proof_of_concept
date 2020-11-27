#include <HardwareSerial.h>
#include <esp.h>
#include "esp_camera.h"
#include "Arduino.h"
//#include "esp_bt_main.h"
//#include "esp_bt.h"
//#include "esp_wifi.h"

// MicroSD
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

static esp_err_t cam_err;
static esp_err_t card_err;
static camera_fb_t *fb;

const unsigned long timeout = 30000;
const unsigned int baudrate = 9600;

static camera_config_t config;
static boolean fresh = true;
static String url, buff, cmd, number;

static HardwareSerial SerialGSM(1);

//You can use command like AT+CFUN=0 and then AT+CFUN=1 to put module in flight mode
// (esp_bluedroid_disable(), esp_bt_controller_disable(), esp_wifi_stop())

void setup() {
  //esp_bluedroid_disable();
  //esp_bt_controller_disable();
  //esp_wifi_stop();

  Serial.begin(baudrate);
  //SerialGSM.begin(baudrate, SERIAL_8N1, 12, 13);
  
  Serial.println("READY");
}

void loop() {
  if (Serial.available()) {
    cmd = Serial.readString();
    cmd.trim();
    if (cmd.startsWith("AT")) {
      if (fresh) SerialGSM.begin(baudrate, SERIAL_8N1, 12, 13);
      SerialGSM.println(cmd);
      fresh = false;
    }
    else if (cmd.startsWith("capture:")) {
      if(!fresh) ESP.restart();
      url = cmd.substring(8);
      save_photo();

      SerialGSM.begin(baudrate, SERIAL_8N1, 12, 13);

      postHTTP();

      esp_camera_fb_return(fb);
      esp_vfs_fat_sdmmc_unmount();

      fresh = false;
      //ESP.restart();???
    }
    else if (cmd.startsWith("insert:")) {
      if (!fresh) SerialGSM.begin(baudrate, SERIAL_8N1, 12, 13);
      url = cmd.substring(7);
      getHTTP();
      fresh = false;
    }
    else if (cmd.startsWith("number")) {
      if (!fresh) SerialGSM.begin(baudrate, SERIAL_8N1, 12, 13);
      getNumber();
      fresh = false;
    }
    else if (cmd.startsWith("reset")) {
      ESP.restart();
    }
    else if (cmd.startsWith("status")) {
      Serial.println(fresh ? "READY" : "RESET NEEDED");
    }
    else {
      Serial.println("Available commands:\ncapture:url\ninsert:url\nnumber\nAT\nreset\nstatus");
    }
  }

  while(SerialGSM.available()) Serial.write(SerialGSM.read());
}

void save_photo() {
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
    config.frame_size = FRAMESIZE_QVGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Init Camera
  cam_err = esp_camera_init(&config);
  if (cam_err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", cam_err);
    return;
  }
  
  // SD camera init
  card_err = init_sdcard();
  if (card_err != ESP_OK) {
    Serial.printf("SD Card init failed with error 0x%x", card_err);
    return;
  }
  
  Serial.println("Taking picture...");
  fb = esp_camera_fb_get();

  String filePath = "/sdcard/";
  filePath.concat(millis());
  filePath.concat(".jpg");

  Serial.println(filePath.c_str());
  FILE *file = fopen(filePath.c_str(), "w");
  if (file != NULL)  {
    size_t err = fwrite(fb->buf, 1, fb->len, file);
    Serial.printf("File saved: %s\n", filePath.c_str());

  }  else  {
    Serial.println("Could not open file");
  }
  fclose(file);

  // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  fresh = false;
}

static esp_err_t init_sdcard()
{
  esp_err_t ret = ESP_FAIL;
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 1,
  };
  sdmmc_card_t *card;

  Serial.println("Mounting SD card...");
  ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret == ESP_OK) {
    Serial.println("SD card mount successfully!");
  }  else  {
    Serial.printf("Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
  }
}

static void waitForResponse() {
  if (wait()) {
    buff = SerialGSM.readString();
    Serial.print(buff);
  }
  else {
    buff = "";
  }
}

static boolean wait()
{
  unsigned long start = millis();
  unsigned long delta = 0;
  
  while(!SerialGSM.available()) {
    delta = millis() - start;
    if (delta >= timeout) {
      Serial.println("Timed out!");
      break;
    }
  }

  return SerialGSM.available();
}

static void getNumber() {
  SerialGSM.println("AT");
  waitForResponse();
  
  if (buff.indexOf("OK") == -1) { // If not connected to GSM modem
    Serial.print(buff);
    return; 
  }

  SerialGSM.println("AT+CNUM");
  waitForResponse();
  
  /*number = "%2b";
  int idx = buff.indexOf("+CNUM:");
  number.concat(buff.substring(idx + 12, idx + 24));
  Serial.print("NUMBER:");
  Serial.println(number);*/
}

static void getHTTP()
{
  SerialGSM.println("AT");
  waitForResponse();
  
  if (buff.indexOf("OK") == -1) return; // If not connected to GSM modem
  
  SerialGSM.println("AT+CREG?");
  waitForResponse();

  if (buff.indexOf("+CREG: 0,1") == -1) return; // if not connected to the network

  SerialGSM.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  waitForResponse();
  SerialGSM.println("AT+SAPBR=3,1,\"APN\",\"internet\"");
  waitForResponse();
  SerialGSM.println("AT+SAPBR=1,1");
  waitForResponse();
  SerialGSM.println("AT+HTTPINIT");
  waitForResponse();
  SerialGSM.println("AT+HTTPPARA=\"CID\",1");
  waitForResponse();
  
  SerialGSM.print("AT+HTTPPARA=\"URL\",\"");
  SerialGSM.print(url);
  SerialGSM.println("\"");
  waitForResponse();
  SerialGSM.println("AT+HTTPSSL=0");
  waitForResponse();
  
  SerialGSM.println("AT+HTTPACTION=0");
  waitForResponse();
  
  if (buff.indexOf("ERROR") == -1) {
    waitForResponse();
    SerialGSM.println("AT+HTTPREAD");
    waitForResponse();
  }
  
  SerialGSM.println("AT+HTTPTERM");
  waitForResponse();
  SerialGSM.println("AT+SAPBR=0,1");
  waitForResponse();
}

static void postHTTP()
{
  SerialGSM.println("AT");
  waitForResponse();
  
  if (buff.indexOf("OK") == -1) return; // If not connected to GSM modem

  SerialGSM.println("AT+CREG?");
  waitForResponse();

  if (buff.indexOf("+CREG: 0,1") == -1) return; // if not connected to the network

  SerialGSM.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  waitForResponse();
  SerialGSM.println("AT+SAPBR=3,1,\"APN\",\"internet\"");
  waitForResponse();
  SerialGSM.println("AT+SAPBR=1,1");
  waitForResponse();
  SerialGSM.println("AT+HTTPINIT");
  waitForResponse();
  SerialGSM.println("AT+HTTPPARA=\"CID\",1");
  waitForResponse();
  SerialGSM.print("AT+HTTPPARA=\"URL\",\"");
  SerialGSM.print(url);
  SerialGSM.println("\"");
  waitForResponse();
  SerialGSM.println("AT+HTTPPARA=\"CONTENT\",\"image/jpeg\"");
  waitForResponse();
  SerialGSM.print("AT+HTTPDATA=");
  SerialGSM.print(fb->len);
  SerialGSM.println(",120000");
  waitForResponse();
  if (buff.indexOf("DOWNLOAD") != -1) {
    for (unsigned int i = 0; i < fb->len; i++) {
      SerialGSM.write(fb->buf[i]);
    }
    waitForResponse();

    SerialGSM.println("AT+HTTPSSL=0");
    waitForResponse();

    SerialGSM.println("AT+HTTPACTION=1");
    waitForResponse();

    if (buff.indexOf("ERROR") == -1) {
      waitForResponse();
      SerialGSM.println("AT+HTTPREAD");
      waitForResponse();
    }
  }  
  
  SerialGSM.println("AT+HTTPTERM");
  waitForResponse();
  SerialGSM.println("AT+SAPBR=0,1");
  waitForResponse();
}
