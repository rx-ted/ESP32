#include <Arduino.h>
#include "WiFi.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#define PART_BOUNDARY "123456789000000000000987654321"

const char *ssid = "wifi's ssid";
const char *password = "wifi's password";
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
httpd_handle_t stream_httpd = NULL;
//定义 pin
#define CAMERA_MODEL_AL_THINKER //SELECT MODEL
#if defined(CAMERA_MODEL_AL_THINKER)
#define pwdn_gpio_num 32
#define reset_gpio_num -1
#define xclk_gpio_num 0
#define siod_gpio 26
#define sioc_gpio_num 27
#define y9 35
#define y8 34
#define y7 39
#define y6 36
#define y5 21
#define y4 19
#define y3 18
#define y2 5
#define vsync_gpio_num 25
#define href_gpio_num 23
#define pclk_gpio_num 22

#else
#error "camera model not selected"
#endif

static esp_err_t stream_handler(httpd_req_t *req)
{
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[64];
  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK)
  {
    return res;
  }
  while (true)
  {
    fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("camera capture failed!");
      res = ESP_FAIL;
    }
    else
    {
      if (fb->width > 400)
      {
        if (fb->format != PIXFORMAT_JPEG)
        {
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if (!jpeg_converted)
          {
            Serial.println("JPEG compression failed!");
            res = ESP_FAIL;
          }
          else
          {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
          }
        }
      }
      if (res == ESP_OK)
      {
        size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
        res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
      }
      if (res == ESP_OK)
      {
        res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
      }
      if (res == ESP_OK)
      {
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
      }
      if (fb)
      {
        esp_camera_fb_return(fb);
        fb = NULL;
        _jpg_buf = NULL;
      }
      else if (_jpg_buf)
      {
        free(_jpg_buf);
        _jpg_buf = NULL;
      }
      if (res != ESP_OK)
      {
        break;
      }
    }
  }
  return res;
}

void startCameraServer()
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = stream_handler,
      .user_ctx = NULL};

  if (httpd_start(&stream_httpd, &config) == ESP_OK)
  {
    httpd_register_uri_handler(stream_httpd, &index_uri);
  }

}

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  // Serial.setDebugOutput(false);
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0; //esp_idf中提供 了LEDC来产生pwm信号，LEDC主要是用来做灯控的，因为其比pwm功能更加丰富，所以esp-idf并没有提供pwm相关的模块。
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = y2;
  config.pin_d1 = y3;
  config.pin_d2 = y4;
  config.pin_d3 = y5;
  config.pin_d4 = y6;
  config.pin_d5 = y7;
  config.pin_d6 = y8;
  config.pin_d7 = y9;
  config.pin_pclk = pclk_gpio_num;
  config.pin_xclk = xclk_gpio_num;
  config.pin_vsync = vsync_gpio_num;
  config.pin_href = href_gpio_num;
  config.pin_sscb_sda = siod_gpio;
  config.pin_sscb_scl = sioc_gpio_num;
  config.pin_pwdn = pwdn_gpio_num;
  config.pin_reset = reset_gpio_num;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //Select lower framesize if the camera doesn't support PSRAM
  if (psramFound())
  {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10; //Quality of JPEG output. 0-63 lower means higher quality
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 5;
    config.fb_count = 2;
  }
  esp_err_t err = esp_camera_init(&config); // // camera init
  if (err != ESP_OK)
  {
    Serial.printf("camera init failed with error 0x%x", err);
    return;
  }
  //wifi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500); //延时500毫秒
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.print("wifi ip!go to http://");
  Serial.print(WiFi.localIP());
  startCameraServer();
}

void loop()
{
  // put your main code here, to run repeatedly:
}