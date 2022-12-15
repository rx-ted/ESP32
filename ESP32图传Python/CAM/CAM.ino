/*
 * BILIBILI：rx-ted
 * 燃霞公司
 */

#include "Arduino.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include <WiFi.h>
#include <WiFiUdp.h>

//宏定义
// 板卡型号选择
//#define CAMERA_MODEL_AI_THINKER
#define CAMERA_MODEL_SPY_MINI

#if defined(CAMERA_MODEL_AI_THINKER)
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

#elif defined(CAMERA_MODEL_SPY_MINI)
  #define PWDN_GPIO_NUM     -1
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM     19
  #define SIOD_GPIO_NUM     23
  #define SIOC_GPIO_NUM     22
  #define Y9_GPIO_NUM       21
  #define Y8_GPIO_NUM       18
  #define Y7_GPIO_NUM       5
  #define Y6_GPIO_NUM       16
  #define Y5_GPIO_NUM       0
  #define Y4_GPIO_NUM       15
  #define Y3_GPIO_NUM       2
  #define Y2_GPIO_NUM       4
  #define VSYNC_GPIO_NUM    36
  #define HREF_GPIO_NUM     39
  #define PCLK_GPIO_NUM     17
#endif


#define toAddress "192.168.1.102" //接收端IP地址
#define toPort 10000 //接收端端口
#define myPort 10000 //本机端口

//全局变量
WiFiUDP udp;


//函数声明
void Communication(void);
camera_fb_t * capture(void);
void sendVideoDate(uint8_t* frame, size_t len, size_t frameCount);



//初始化

void setup() {
  //初始化串口
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  //配置摄像头
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
  config.xclk_freq_hz = 20000000;//摄像头的工作时钟，在允许范围内频率越高帧率就越高
  config.pixel_format = PIXFORMAT_JPEG;//输出VGA图像
  config.frame_size = FRAMESIZE_SVGA;//图像尺寸800x600 FRAMESIZE_VGA;//图像尺寸640x480
  config.jpeg_quality = 12;//图像质量
  config.fb_count = 1;

  //摄像头初始化
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);//摄像头初始化失败，结束程序
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1); // 翻转

  // 连接WIFI
  WiFi.begin("Johan-home", "56886978");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\r\nWiFi connected");

  //打印本地IP
  String localAddress = WiFi.localIP().toString();
  Serial.printf("localAddress:%s\r\n",localAddress);

  //启动服务
  Communication();
}

void loop() {
  // put your main code here, to run repeatedly:

}

// 发送图片数据
void sendVideoDate(uint8_t* frame, size_t len, size_t frameCount)
{
    uint8_t txBuffer[1024] = {0};        
    size_t frameId = frameCount; //帧号
    size_t frameSize = len; //帧大小
    int packetCount=0; //包个数
    int packetId=1; //包号
    int packetLen=1000; //包长
    int packetSize=0; //包大小,小于等于包长
    if(frameSize==0)
    {
        Serial.printf("send buffer len=0.\r\n");
        return;
    }
    //计算包个数
    packetCount=frameSize/packetLen+((frameSize%packetLen)==0?0:1);
    size_t sendOffset=0;
    while(sendOffset<frameSize)
    {
        packetSize=((sendOffset+packetLen)>frameSize)?(frameSize-sendOffset):(packetLen);
        //数据包标志
        txBuffer[0]=0x12;
        //帧号
        txBuffer[1]=(uint8_t)(frameId>>24);
        txBuffer[2]=(uint8_t)(frameId>>16);
        txBuffer[3]=(uint8_t)(frameId>>8);
        txBuffer[4]=(uint8_t)(frameId);
    
        //帧大小
        txBuffer[5]=(uint8_t)(frameSize>>24);
        txBuffer[6]=(uint8_t)(frameSize>>16);
        txBuffer[7]=(uint8_t)(frameSize>>8);
        txBuffer[8]=(uint8_t)(frameSize);

        txBuffer[9]=packetCount; //包个数
        txBuffer[10]=packetId; //包号
        //包长
        txBuffer[11]=(uint8_t)(packetLen>>8);
        txBuffer[12]=(uint8_t)(packetLen);
        //包大小,小于等于包长
        txBuffer[13]=(uint8_t)(packetSize>>8);
        txBuffer[14]=(uint8_t)(packetSize);
        //图像数据
        memcpy(&txBuffer[15], frame+sendOffset, packetSize);    
                 
        //发送
        udp.beginPacket(toAddress,toPort);
        udp.write((const  uint8_t *)txBuffer, 15+packetSize);
        udp.endPacket();

        //指向下一位置
        sendOffset+=packetSize;
        packetId++;
    }      
}

//获取图像
camera_fb_t * capture(void){
    camera_fb_t * fb = esp_camera_fb_get(); //获取图像
    if (!fb)
        Serial.println("Camera capture failed");//图像为空
    return fb;
}

// 通信服务
void Communication(void) {
    uint8_t rBuff[256]; //UDP接收缓存
    int fid=0; //图像帧号
    
    udp.begin(myPort);
    while(1)
    {
        // 图片传输
        camera_fb_t * fbSend = capture();//获取图像指针
        if(fbSend)//非空指针
        {
          sendVideoDate((uint8_t *)fbSend->buf, fbSend->len, ++fid);//将图像发送出去
          Serial.printf("core 0 send ok! Jpg_Len = %u,id=%u\r\n",fbSend->len,fid);//打印信息
        }

        // 接收/运行指令
        int len = udp.parsePacket(); //获取接收到的数据量
        if(len>0)
        {
          if(len>256)
            len = 256;//防止越界
          len = udp.read(rBuff,len);//读取到接收缓存
          Serial.write(rBuff,len);
          Serial.println(" "); 
          //可以在这里解析接收到的数据，执行指令
        }
    }
}
