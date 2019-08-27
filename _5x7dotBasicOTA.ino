#include <NTP.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Adafruit_NeoPixel.h>
#include "font.h"
#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Ticker.h"

#define PIN            14   //WS2813制御ピン
#define NUMPIXELS      35   //LED数/基板
#define NUMPCB         4    //基板の数
#define MSEC2CLOCK(ms)    (ms * 80000L)// ミリ秒をクロック数に換算 (@80MHz)

const char* ssid = "TP-LINK_3D9A";
const char* ssid2 = "aterm-8f1c6a-g";
const char* password = "kotouyokomizo";

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS * NUMPCB, PIN, NEO_GRB + NEO_KHZ800);
Ticker ticker;

int DelayVal = 100; // delay for half a second
int Bright = 50;    //明るさ

int ColorIndex = 0;

MPU6050 Accelgyro;  //加速度センサ

volatile int TopFace = 0;     //上面の面番号 0 bottom、1：top 2,3,4,5 side
volatile bool Abort = false;  //処理中止フラグ
volatile int Mode = 1;

uint32_t curmill = 0;
uint32_t premill = 0;

char msg[] = "Merry Christmas and Happy New Year.";
bool flgIntrpt = false;
int melo = 200;
//-----------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  //自宅
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    //事務所
    WiFi.begin(ssid2, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
      Serial.println("Connection Failed! Rebooting...");
      delay(5000);
      ESP.restart();
    }
  }
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //NTP
  ntp_begin(2390);

  //OTA以外のsetup
  pixels.begin(); // This initializes the NeoPixel library.
  //MPU6050 Initial
  Wire.begin();
  Accelgyro.initialize();

#if 1 //割込み設定
  ticker.attach_ms(100, GetTopFace);
#endif
}
//-----------------------------------------
void loop()
{
  ArduinoOTA.handle();
  //どの面が上にあるか
  //  TopFace = GetTopFace();
  //表示更新は500msec間隔
  if (millis() - premill < 500)
    return;
  premill = millis();

  if (TopFace == 0)
    DispDate();
  else if (TopFace == 1)
    DispClock();
  else if (TopFace == 2)
    DispRandam();
  else if (TopFace == 3)
    rainbow(30);
  else if (TopFace == 4)
    TableLight();
  else if (TopFace == 5)
    Saikoro();
  else
    DispRandam();

}
//-----------------------------------------
void Saikoro()
{
  byte b[7];
  static char c = '0';
  if (Mode == 1)
  {
    ConvertBmp(c, b);
    ClearAllFace();
    DispOneFace(0, b, 0x00303030);
  } else
  {
    for (int i = 0; i < 250; i += 5)
    {
      int r = random(1, 7);
      c = '0' + r;
      ConvertBmp(c, b);
      DispOneFace(0, b, 0x00005000);
      //      delayMicroseconds(i * i*3);
      if (i > 200)
        delay(i * i / 90);
      else
        delay(i);
      if (Abort)
      {
        Abort = false;
        return;
      }
    }
    for (int i = 0; i < 3; i++)
    {
      ClearAllFace();
      delay(100);
      DispOneFace(0, b, 0x00303030);
      delay(100);
    }
    Mode = 1;
  }

}
//-----------------------------------------
void DispMsg(char *msg)
{
  static int index = 0;
  byte bmp[4][7];
  memset(bmp, 0, sizeof(bmp));
  //最後の文字
  if (index >= strlen(msg))
  {
    //最後の文字の消灯し
    ClearAllFace();
    delay(100);
    index = 0;
  } else
  {
    for (int i = 0; i < 4; i++)
    {
      ConvertBmp(msg[index], bmp[i]);
    }
    DispAllFace(bmp, Mode);
    index++;
  }
}
//-----------------------------------------
//未完成　スクロールするはず
void DispScrollMsg(char *msg)
{
  static int index = 0;
  byte bmp[5][7];
  byte newbmp[5][7];
  memset(bmp, 0, sizeof(bmp));
  memset(newbmp, 0, sizeof(bmp));
  //最後の文字
  if (index >= strlen(msg) * 5)
  {
    //最後の文字の消灯し
    ClearAllFace();
    delay(100);
    index = 0;
  } else
  {
    for (int i = 0; i < 5; i++)
    {
      ConvertBmp(msg[index / 5 + i], bmp[i]);
    }
    for (int i = 0; i < 7; i++)
    {
      int j = 0;
      if (i % 2 == 0)
        newbmp[j][i] = (bmp[j][i] << (index % 5)) | (bmp[j + 1][i] >> (7 - index % 5));
      else
        newbmp[j][i] = (bmp[j][i] >> (index % 5)) | (bmp[j + 1][i] << (7 - index % 5));
    }

    DispAllFace(newbmp, Mode);
    index++;
  }
}


//-----------------------------------------
void DispClock()
{
  time_t n = now();
  time_t t;

  char s[20];
  const char* format = "%02d:%02d'%02d";

  // JST
  t = localtime(n, 9);
  sprintf(s, format, hour(t), minute(t), second(t));

  DispMsg(s);
}
//-----------------------------------------
void DispDate()
{
  time_t n = now();
  time_t t;

  char s[20];
  const char* format = "%02d/%02d ";
  // JST
  t = localtime(n, 9);
  sprintf(s, format, month(t), day(t));
  s[2] = 0x90;
  s[5] = 0x91;

  DispMsg(s);
  //DispScrollMsg(s);
}
//-----------------------------------------
//
void TableLight()
{
  for (int j = 0; j < 4; j++)
  {
    for (int i = 0; i < NUMPIXELS; i++)
    {
      if (j == 1)
        pixels.setPixelColor(i + j * NUMPIXELS, 0xff, 0xff, 0xff);
      else
        pixels.setPixelColor(i + j * NUMPIXELS, 0, 0, 0);
    }
  }
  pixels.show();
}
//-----------------------------------------
void DispRandam()
{
  for (int j = 0; j < 4; j++)
  {
    for (int i = 0; i < NUMPIXELS; i++)
    {
      byte r = random(0, 30);
      byte g = random(0, 30);
      byte b = random(0, 30);

      if (r + g + b > 40)
        pixels.setPixelColor(i + j * NUMPIXELS, r,  g,  b);
      else
        pixels.setPixelColor(i + j * NUMPIXELS, 0);
      delayMicroseconds(10);

      if (Abort)
      {
        Abort = false;
        return;
      }
    }
  }
  pixels.show(); // This sends the updated pixel color to the hardware.
}
//-----------------------------------------
void DispFirstLight()
{
  for (int j = 0; j < 4; j++)
  {
    for (int i = 0; i < NUMPIXELS; i++)
    {
      pixels.setPixelColor(i + j * NUMPIXELS, 50,  0, 0);
      pixels.show(); // This sends the updated pixel color to the hardware.
      delayMicroseconds(10);
      if (Abort)
      {
        Abort = false;
        return;
      }
    }
  }
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < pixels.numPixels(); i++) {
    pixels.setPixelColor(i, c);
    pixels.show();
    delay(wait);
    if (Abort)
    {
      Abort = false;
      return;
    }
  }
}
void rainbow(uint8_t wait)
{
  uint16_t i, j;
  for (j = 0; j < 256; j++)
  {
    for (i = 0; i < pixels.numPixels(); i++)
    {
      pixels.setPixelColor(i, Wheel((i + j) & 255));
      if (Abort)
      {
        Abort = false;
        return;
      }
    }
    pixels.show();
    delay(wait);
  }
}
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < pixels.numPixels(); i++) {
      pixels.setPixelColor(i, Wheel(((i * 256 / pixels.numPixels()) + j) & 255));
      if (Abort)
      {
        Abort = false;
        return;
      }
    }
    pixels.show();
    delay(wait);
  }
}
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j = 0; j < 10; j++) { //do 10 cycles of chasing
    for (int q = 0; q < 3; q++) {
      for (uint16_t i = 0; i < pixels.numPixels(); i = i + 3) {
        pixels.setPixelColor(i + q, c);  //turn every third pixel on
        if (Abort)
        {
          Abort = false;
          return;
        }
      }
      pixels.show();

      delay(wait);

      for (uint16_t i = 0; i < pixels.numPixels(); i = i + 3) {
        pixels.setPixelColor(i + q, 0);      //turn every third pixel off
        if (Abort)
        {
          Abort = false;
          return;
        }
      }
    }
  }
}
//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j = 0; j < 256; j++) {   // cycle all 256 colors in the wheel
    for (int q = 0; q < 3; q++) {
      for (uint16_t i = 0; i < pixels.numPixels(); i = i + 3) {
        pixels.setPixelColor(i + q, Wheel( (i + j) % 255)); //turn every third pixel on
        if (Abort)
        {
          Abort = false;
          return;
        }
      }
      pixels.show();

      delay(wait);

      for (uint16_t i = 0; i < pixels.numPixels(); i = i + 3) {
        pixels.setPixelColor(i + q, 0);      //turn every third pixel off
        if (Abort)
        {
          Abort = false;
          return;
        }
      }
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return pixels.Color((255 - WheelPos * 3) / 10, 0, (WheelPos * 3) / 10);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return pixels.Color(0, (WheelPos * 3) / 10, (255 - WheelPos * 3)) / 10;
  }
  WheelPos -= 170;
  return pixels.Color((WheelPos * 3) / 10, (255 - WheelPos * 3) / 10, 0);
}

//--------------------------------------
void GetTopFace()
{
  int16_t ax, ay, az;//加速度
  int16_t gx, gy, gz;//ジャイロ(角速度）
  int16_t vals[3];
  int maxval;
  int top;
  static int premaxval ;

  Accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
#if 1
  char s[128];
  sprintf(s, "%4d %4d %4d", ax, ay, az);
  Serial.println(s);
#endif
  vals[0] = abs(ax);
  vals[1] = abs(ay);
  vals[2] = abs(az);
  if (vals[0] > vals[1] && vals[0] > vals[2])
  {
    top = (ax > 0) ? 0 : 1;
    maxval = vals[0] ;
  }
  if (vals[1] > vals[2] && vals[1] > vals[0])
  {
    top = (ay > 0) ? 2 : 3;
    maxval = vals[1] ;
  }
  if ((vals[2] > vals[0]) && (vals[2] > vals[1]))
  {
    top = (az > 0) ? 4 : 5;
    maxval = vals[2] ;
  }
  //トップ面が変更されたらModeを初期化
  if (TopFace != top)
  {
    TopFace = top;
    Mode = 1;
  } else
  {
    if (abs(maxval - 16384) > 2000 && abs(premaxval - 16384) < 4000 )
    {
      //   Serial.println(abs(maxval - 16384) );
      Mode++;
      Abort = true;
      if (Mode > 7)
        Mode = 1;
    }
  }
  premaxval = maxval;
}
//-----------------------------------------
// LEDをbmpパターンに点灯
// faceはどの面か　
//-----------------------------------------
void DispAllFace(byte bmp[][7], int clr)
{
  ClearAllFace();
  delay(20);
  uint8_t r = (clr & 0x01) * Bright;
  uint8_t g = (clr >> 1 & 0x01) * Bright;
  uint8_t b = (clr >> 2 & 0x01) * Bright;

  for (int j = 0; j < 4; j++)
  {
    for (int i = 0; i < NUMPIXELS; i++)
    {
      if (bmp[j][i / 5] & (0x01 << (i % 5)))
        pixels.setPixelColor(i + j * NUMPIXELS, r, g, b);
      else
        pixels.setPixelColor(i + j * NUMPIXELS, 0);
      delayMicroseconds(10);
    }
  }
  pixels.show(); // This sends the updated pixel color to the hardware.
}
//-----------------------------------------
void ClearAllFace()
{
  for (int j = 0; j < 4; j++)
  {
    //一旦消灯
    for (int i = 0; i < NUMPIXELS; i++)
    {
      pixels.setPixelColor(i + j * NUMPIXELS, 0);
      delayMicroseconds(10);
    }
  }
  pixels.show(); // This sends the updated pixel color to the hardware.
}
//-----------------------------------------
// LEDをbmpパターンに点灯
// faceはどの面か　
//-----------------------------------------
void DispOneFace(int face, byte bmp[], uint32_t clr)
{
  //データ転送
  for (int i = 0; i < NUMPIXELS; i++)
  {
    if (bmp[i / 5] & (0x01 << (i % 5)))
      pixels.setPixelColor(i + face * NUMPIXELS, clr);
    else
      pixels.setPixelColor(i + face * NUMPIXELS, 0);
    delayMicroseconds(10);
  }
  pixels.show(); // This sends the updated pixel color to the hardware.
}

//-----------------------------------------
void DispOneFace(int face, byte bmp[], uint8_t r, uint8_t g, uint8_t b)
{
  DispOneFace(face, bmp, r << 16 | g << 8 | b );
}
//-----------------------------------------
int ConvertBmp(char ch, byte * numbits)
{
  int index = ch - 0x20;
  int cnt_on = 0;//点灯するLEDの数

  for (int r = 0; r < 7; r++)
  {
    numbits[r] = 0;
    for (int c = 0; c < 5; c++)
    {
      if (r % 2 == 0)
      {
        if (TopFace % 2 == 0)
          numbits[r] |= ((Font[index][c] >> r) & 0x01) << (4 - c);
        else
          numbits[r] |= ((Font[index][c] >> (6 - r)) & 0x01) << c;
      }
      else
      {
        if (TopFace % 2 == 0)
          numbits[r] |= ((Font[index][c] >> r) & 0x01) << c;
        else
          numbits[r] |= ((Font[index][c] >> (6 - r)) & 0x01) << (4 - c);
      }
      cnt_on += ((Font[index][c] >> (6 - r)) & 0x01);
    }
  }
  return cnt_on;
}

