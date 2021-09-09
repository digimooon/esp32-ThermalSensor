#include "Arduino.h"
#include <Wire.h>
#include <WiFi.h>
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

#define TA_SHIFT 8 

float EMMISIVITY = 0.95f;

const char* ssid     = "Temp";
const char* password = "12345678";
WiFiServer server(80);
WiFiClient client;

paramsMLX90640 mlx90640;
const byte MLX90640_address = 0x33; //Default 7-bit unshifted address of the MLX90640
static float tempValues[32 * 24];
static uint8_t sendArray[32 * 24 * 4];

//TaskHandle_t TaskA;

typedef union { //结构体进行float到byte的转化
    float floatTemp;
    byte  byteArrTemp[4];
} uTemp;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000); 
  Wire.beginTransmission((uint8_t)MLX90640_address);
  if (Wire.endTransmission() != 0) {
    Serial.println("MLX90640 not detected at default I2C address. Starting scan the device addr...");
    Device_Scan();
//    while(1);
  }
  else {
    Serial.println("MLX90640 online!");
  }
  int status;
  uint16_t eeMLX90640[832];
  status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
  if (status != 0) Serial.println("Failed to load system parameters");
  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  if (status != 0) Serial.println("Parameter extraction failed");
  MLX90640_SetRefreshRate(MLX90640_address, 0x04);  //温度检测器帧率
  Wire.setClock(800000);

//  WiFi.softAP(ssid);
  
  WiFi.begin(ssid, password);
  server.begin();
}

void loop(void) {
   client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    bool needConsist = false;
    Serial.println("New Client.");           // print a message out the serial port
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,

        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor

        if (c == 'A'){  //测试
          client.print("Recieved:");
        }

        if (c == 'B'){  //单张闪照
          client.write('B');
          float * dataArray;
          dataArray = TempValues();
          for (int num=0; num<768; num++){
//            uTemp temp;
//            temp.floatTemp = dataArray[num];
//            client.write(temp.byteArrTemp, 4);
            memcpy(sendArray + num*4, (uint8_t*) &dataArray[num], 4);
          }
          client.write(sendArray, 4*768);
          
//          for (int num=0; num<768; num++)
//          client.printf("%.2f",dataArray[num]);
          delay(2000);
        }

        if (c == 'C'){  //发送连续动画
          needConsist = true;
          
//          xTaskCreate(  //线程会使读取速率跟不上
//            consistTrans,
//            "ConsistTrans",
//            100000,
//            (void*)&client,
//            1,
//            &TaskA);
        }

        if (c == 'D')
          MLX90640_SetRefreshRate(MLX90640_address, 0x04);

        if (c == 'E')
          MLX90640_SetRefreshRate(MLX90640_address, 0x05);

          
        if (c == 'L')
          EMMISIVITY = 0.84f;

          
        if (c == 'M')
          EMMISIVITY = 0.95f;

          
        if (c == 'H')
          EMMISIVITY = 0.97f;

        if (c == 'P') //暂停发送
          needConsist = false;

        if (c == 'Q') //断开连接
          break;
      }

      if (needConsist) {  //连续发送
        client.write('B');
        float * dataArray;
        dataArray = TempValues();
        for (int num=0; num<768; num++){
//          uTemp temp;
//          temp.floatTemp = dataArray[num];
          memcpy(sendArray + num*4, (uint8_t*) &dataArray[num], 4);
        }
        client.write(sendArray, 4*768);
          
//        client.printf("%.2f",dataArray[num]);      
      }
      
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}

//void consistTrans (void *parameter){
//  while (true){
//  float * dataArray;
//  dataArray = TempValues();
//  for (int num=0; num<768; num++)
//  (*((WiFiClient*)parameter)).print(dataArray[num]);
//  delay(250);
//  }
//}

float * TempValues() {
  for (byte x = 0 ; x < 2 ; x++) 
  {
    uint16_t mlx90640Frame[834];
    int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
    if (status < 0)
    {
      Serial.print("GetFrame Error: ");
      Serial.println(status);
    }

    float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);  //外壳温度

    float tr = Ta - TA_SHIFT; //计算环境温度

    MLX90640_CalculateTo(mlx90640Frame, &mlx90640, EMMISIVITY, tr, tempValues);

    MLX90640_BadPixelsCorrection((&mlx90640)->brokenPixels, tempValues, 1, &mlx90640);

    MLX90640_BadPixelsCorrection((&mlx90640)->outlierPixels, tempValues, 1, &mlx90640);
  }
  return tempValues;
}

void Device_Scan() {
  byte error, address;
  int nDevices;
  Serial.println("Scanning...");
  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");
      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print("Unknow error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found");
  else
    Serial.println("done");
}
