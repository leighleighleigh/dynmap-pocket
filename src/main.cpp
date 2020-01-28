#include <Arduino.h>
#include "FS.h"
#include "SD.h"
#include <SPI.h>>
#include <WiFi.h>
#include <WiFiMulti.h>

#include <TFT_eSPI.h>
#include <HTTPClient.h>
//Use new decoder
#include <TJpg_Decoder.h>

#define USE_SERIAL Serial
HTTPClient http;
WiFiMulti wifiMulti;
TFT_eSPI tft = TFT_eSPI();

String example_url = "https://ltg.ddns.net/map/tiles/world/t/0_0/z_0_32.jpg";
String base_url = "https://ltg.ddns.net/map/tiles/world/t/";
String target_filename = "/z_0_32.jpg";

int jpeg_scale = 4;

//String tile_zoom_prefix = "";
//int tile_zoom_stepfactor = 1;

//String tile_zoom_prefix = "zz_";
//int tile_zoom_stepfactor = 4;

//String tile_zoom_prefix = "zzzz_";
//int tile_zoom_stepfactor = 16;

String tile_zoom_prefix = "zzzzzz_";
int tile_zoom_stepfactor = 64;


// create buffer for read (512 so we can write a block nicely)
uint8_t buff[10240] = { 0 };
uint8_t bufsize = 0;

// This next function will be called during decoding of the jpeg file to
// render each block to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode next block
  return 1;
}

void setup() {
  USE_SERIAL.begin(115200);
  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  // SETUP WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin("Domamungi", "cooldude69420");
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // SETUP TFT
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(3);  // portrait

  // The scaling is DOWNSCALING! which is good!.
  // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
  TJpgDec.setJpgScale(jpeg_scale);

  // The byte order can be swapped (set true for TFT_eSPI)
  TJpgDec.setSwapBytes(true);

  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);


  // SETUP SD CARD CHECKS
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

void downloadFile(String url, String fileName) {
  Serial.println("Downloading " + url);

  http.begin(url);
  int httpCode = http.GET();

  // file found at server
  if (httpCode == HTTP_CODE_OK) {

    // get lenght of document (is -1 when Server sends no Content-Length header)
    int len = http.getSize();

    // get tcp stream
    WiFiClient * stream = http.getStreamPtr();

    // read all data from server
    while (http.connected() && (len > 0 || len == -1)) {
      // get available data size
      size_t size = stream->available();
      bufsize = size;

      if (size > sizeof(buff))
      {
        Serial.println("BUFFER TOO SMALL FOR TILE");
      }

      if (size) {
        // read up to 128 byte
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

        if (len > 0) {
          len -= c;
        }
      }
      delay(1);
    }

  }

  http.end();
  Serial.println("Done");
}

String generateTileUrlNew(int x, int y, int stepfactor)
{

  //  base_url + baseStep * x + baseStep * y  + x * baseStep * pixelStep blahblahblah
  // Oh also zoom * 'z'
  String url = base_url;

  x = x * stepfactor;
  y = y * stepfactor;

  //  Need to convert these to String and slap them onto the url
  url = url + "0_0/" + tile_zoom_prefix + x + "_" + y + ".jpg";
  return url;
}


String generateTileNameNew(int x, int y, int stepfactor)
{
  String url = "";
  x = x * stepfactor;
  y = y * stepfactor;
  url = url + "/" + tile_zoom_prefix + x + "_" + y + ".jpg";
  return url;
}

//Map view is centered
int mapViewX = 240;
int mapViewY = 160;

int tileOffsetX = 0;
int tileOffsetY = 0;

void loop() {
  TJpgDec.setJpgScale(jpeg_scale);

  String tileUrl = "";
  String tileName = "";
  String tilePixelPos = "";

  int tileXPos = 0;
  int tileYPos = 0;

  int last_x = 0;
  int last_y = 0;

  for (int y = 12; y > -12; y--)
  {
    for (int x = -16; x < 16; x++)
    {

      tileXPos = mapViewX + (x * 128 / jpeg_scale);
      tileYPos = mapViewY - (y * 128 / jpeg_scale);

      // Check if we should bother drawing/downloading this tile!
      if (tileXPos + (128 / jpeg_scale) < 0 || tileXPos >= 480) {
        continue;
      }

      if (tileYPos >= 320 || tileYPos + (128 / jpeg_scale) <=  0) {
        continue;
      }


      tilePixelPos = "x:";
      tilePixelPos = tilePixelPos + tileXPos + ", y:" + tileYPos;
      tileUrl = generateTileUrlNew(x + tileOffsetX, y + tileOffsetY, tile_zoom_stepfactor);
      tileName = generateTileNameNew(x + tileOffsetX, y + tileOffsetY, tile_zoom_stepfactor);

      Serial.print("GEN URL: ");
      Serial.println(tileUrl);

      // For debugging we are going to do a little red square before each tile
      tft.fillRect(tileXPos, tileYPos, 128 / jpeg_scale, 128 / jpeg_scale, TFT_BLACK);

      // Download file to ram buffer
      downloadFile(tileUrl, tileName);

      // Draw it
      TJpgDec.drawJpg(tileXPos, tileYPos, buff, sizeof(buff));

      // Debug stuff.
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextSize(1);
      //      tft.fillRect(0,290,480,30,TFT_BLACK);

      tft.setCursor(0, 320 - (3 * 10));
      tft.print(tileUrl);
      tft.setCursor(0, 320 - (2 * 10));
      tft.print(tileName);
      tft.setCursor(0, 320 - (1 * 10));
      tft.print(tilePixelPos);
 

    }
  }

  delay(10000);
}