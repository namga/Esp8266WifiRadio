/*
Project by Tech Talkies YouTube channel.
If you distribute this code, please credit us.

https://www.youtube.com/@techtalkies1
*/

#include <ESP8266WiFi.h>
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"
#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// To run, set your ESP8266 build to 160MHz, update the SSID info, and upload.
// Enter your WiFi setup here:
#ifndef STASSID
#define STASSID "KRIXI HOME F3"
#define STAPSK "88888888"
#endif

WiFiServer configWebServer(80);

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const char *wifi_ssid = STASSID;
const char *wifi_password = STAPSK;

//BBC World news - Change to your URL
//const char *URL = "http://utulsa.streamguys1.com/KWGSHD1-MP3";
const char *URL = "http://rfienvietnamien64k.ice.infomaniak.ch/rfienvietnamien-64.mp3";
//const char *URL = "https://c13.radioboss.fm:18127/autodj";
AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2SNoDAC *out;

bool clear_buff = false;

// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void)isUnicode;  // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2) - 1] = 0;
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  scrollText(s2);
  Serial.flush();
}

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}


void setup()
{
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  delay(1000);
  Serial.println("Connecting to WiFi");
  scrollText("Connecting");

  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.begin(wifi_ssid, wifi_password);
  WiFi.waitForConnectResult();
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println();
    Serial.println("Could not connect to WiFi. Starting configuration AP...");
    configAP();
  }
  Serial.println("WiFi connected");
  scrollText("Connected");

  radio_start();
}

void replace(char *str)
 {
   while (*str)
   {
     if (*str == '+')
       *str = ' ';
     str++;
   }
 }

void configAP()
{
  // Starts the default AP (factory default or setup as persistent)
  //WiFi.mode(WIFI_AP_STA);

  WiFi.softAP("NamGa NetWork", "0968387974");
  delay(500);

  Serial.print("Connect your computer to the WiFi network: ");
  Serial.print(WiFi.softAPSSID());
  Serial.println();
  IPAddress ip = WiFi.softAPIP();
  Serial.print("and enter http://");
  Serial.print(ip);
  Serial.println(" in a Web browser");
  scrollText("Connecting");

  configWebServer.begin();

  while (true)
  {
    WiFiClient client = configWebServer.available();
    if (client)
    {
      char line[64];
      int l = client.readBytesUntil('\n', line, sizeof(line));
      line[l] = 0;
      client.find((char*) "\r\n\r\n");
      if (strncmp_P(line, PSTR("POST"), strlen("POST")) == 0)
      {
        l = client.readBytes(line, sizeof(line));
        line[l] = 0;

        // parse the parameters sent by the html form
        const char* delims = "=&";
        strtok(line, delims);
        char* ssid = strtok(NULL, delims);

        //replace the + charater in the ssid name
        replace(ssid);
        strtok(NULL, delims);
        char* pass = strtok(NULL, delims);

        // send a response before attemting to connect to the WiFi network
        // because it will reset the SoftAP and disconnect the client station
        client.println(F("HTTP/1.1 200 OK"));
        client.println(F("Connection: close"));
        client.println(F("Refresh: 10")); // send a request after 10 seconds
        client.println();
        client.println(F("<html><body><h3>Configuration AP</h3><br>connecting...</body></html>"));
        client.stop();

        Serial.println();
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        WiFi.persistent(true);
        WiFi.setAutoConnect(true);
        WiFi.begin(ssid, pass);
        WiFi.waitForConnectResult();
      }
      else
      {
        // Configuration continues with the next request
        client.println(F("HTTP/1.1 200 OK"));
        client.println(F("Connection: close"));
        client.println();
        client.println(F("<html><body><h3>Configuration AP</h3><br>"));

        int status = WiFi.status();
        if (status == WL_CONNECTED)
        {
          client.println(F("Connection successful. Ending AP."));
        }
        else
        {
          client.println(F("<form action='/' method='POST'>WiFi connection failed. Enter valid parameters, please.<br><br>"));
          client.println(F("SSID:<br><input type='text' name='i'><br>"));
          client.println(F("Password:<br><input type='password' name='p'><br><br>"));
          client.println(F("<input type='submit' value='Submit'></form>"));
        }
        client.println(F("</body></html>"));
        client.stop();

        if (status == WL_CONNECTED)
        {
          delay(1000); // to let the SDK finish the communication
          Serial.println("Connection successful. Ending AP.");
          configWebServer.stop();
          WiFi.mode(WIFI_STA);
          break;
        }
      }
    }
  }
}

void loop()
{
  if(WiFi.status() == WL_CONNECTED)
  {
    static int lasts = 0;

    if (mp3->isRunning())
    {
      if ((millis()/1000) - lasts > 1)
      {
        lasts = millis()/1000;
        Serial.printf("Running for %d second...\n", lasts);
        Serial.flush();
      }
      if (!mp3->loop()) mp3->stop();
    }
    else
    {
      clear_buff = true;
      Serial.printf("Error when load the resource. MP3 done\n");

      delay(1000);
      
      // reload the radio chanel
      if (clear_buff)
      {
        clearBuff();
        radio_start();
        clear_buff = false;
      }
    }
  }
}

void clearBuff()
{
  if (file)
  {
    delete file;
    file = nullptr;
  }
  if (buff)
  {
    delete buff;
    buff = nullptr;
  }
  if (out)
  {
    delete out;
    out = nullptr;
  }
  if (mp3)
  {
    delete mp3;
    mp3 = nullptr;
  }
}

void radio_start()
{
  audioLogger = &Serial;
  file = new AudioFileSourceICYStream(URL);
  file->RegisterMetadataCB(MDCallback, (void *)"ICY");
  buff = new AudioFileSourceBuffer(file, 2048);
  buff->RegisterStatusCB(StatusCallback, (void *)"buffer");
  out = new AudioOutputI2SNoDAC();
  out->SetOutputModeMono(true);
  mp3 = new AudioGeneratorMP3();
  mp3->RegisterStatusCB(StatusCallback, (void *)"mp3");
  mp3->begin(buff, out);
}

void scrollText(String text2)
{
  display.clearDisplay();

  display.setTextSize(2);  // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(text2);
  display.display();  // Show initial text
  delay(100);
}