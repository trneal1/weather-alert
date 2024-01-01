#include <Arduino.h>

#include <Wifi.h>
#include <NTPClient.h>
#include <ArduinoOTA.h>

#include <freertos/task.h>

#include <HTTPClient.h>
#define ARDUINOJSON_COMMENTS_ENABLE 1
#include <ArduinoJSON.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <WROVER_KIT_LCD.h>


void task1(void *);

const char *ssid = "TRNNET-2G";
const char *password = "ripcord1";

const char *hostname="ESP_WALERT";

WiFiUDP udp,udp1;
WiFiUDP udpd;

const char areas[100]="NCC183/NCC063/NCC069/SCZ056/MDC027";  //MDC027
const char *codes[]={"NC W","NC D","NC F","SC G", "MD H"};  //MD H

struct SpiRamAllocator {
        void* allocate(size_t size) {
                return ps_malloc(size);

        }
        void deallocate(void* pointer) {
                free(pointer);
        }
};
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;



WiFiClientSecure client;
WROVER_KIT_LCD tft;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,"pool.ntp.org",-4*3600);

void lcd_init(){
tft.fillScreen(WROVER_BLACK);
  tft.setRotation(1);
  tft.setCursor(0, 0);
  tft.setTextColor(WROVER_YELLOW); tft.setTextSize(2); 
}

void connect() {

  lcd_init();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    tft.printf("attempting to connect, %d\n\r",WiFi.status());
    delay(2000);
  }
  lcd_init();
  tft.println("Connected!");
  tft.println(WiFi.localIP());
  tft.println(WiFi.SSID());
  delay(2000);
}

void setup(void) {

    psramInit();
    Serial.begin(9600);
    tft.begin();

    connect();
    timeClient.begin();
    timeClient.update();

    ArduinoOTA.begin();

    client.setInsecure();

    xTaskCreatePinnedToCore(
                    task1,          /* Task function. */
                    "Task1",        /* String with name of task. */
                    10000,            /* Stack size in bytes. */
                    NULL,             /* Parameter passed as input of the task */
                    1,                /* Priority of the task. */
                    NULL,              /* Task handle. */
                     1);                  /* Core */


   udpd.beginPacket("192.168.1.255",6100);
   udpd.println("reboot!!");
   udpd.endPacket();

   pinMode(25,INPUT);

}

void loop(void) {
   timeClient.update();
   ArduinoOTA.handle();
}

void task1( void * parameter )
{
   HTTPClient http;
   char areasbuff[100];
   char mergebuff[100];
   char* area;
   int areanum;
   int oldarea;

   int localnum;
   char localcode[10];
   char localcode_l[10];

   int textcolor;

   SpiRamJsonDocument doc(4000000);

   long start;
   for(;;){

    areanum=0;
    oldarea=999;
    localnum=0;

    udpd.beginPacket("192.168.1.255",6100);
    
    lcd_init();
    tft.print("Weather Alerts   ");
    tft.println(timeClient.getFormattedTime());

    //udp1.beginPacket("192.168.1.255",5100);
    //udp1.print("0 500 4x000000 ");
    //udp1.endPacket();
   
   
    strcpy(areasbuff,areas);
    area=strtok(areasbuff,"/");
    while(area!=NULL){

      start=millis();

      strcpy(mergebuff,"https://api.weather.gov/alerts/active?zone=");
      strcat(mergebuff,area);
      Serial.print(mergebuff);
      Serial.print("    ");
      Serial.println(millis());
      http.useHTTP10(true);
      http.begin(client, mergebuff);
      http.setTimeout(60000);
      http.GET();

      udpd.printf("%ld   %ld   ",start,millis());

      DeserializationError error = deserializeJson(doc, http.getStream());
   
      if (error) {
         //lcd_init();
         tft.setTextSize(1);
         tft.setTextColor(WROVER_ORANGE);
         tft.print(area);
         tft.print(": ");
         tft.print(F("deserializeJson() failed: "));
         tft.println(error.f_str());

         http.end();
         client.stop();
         
         http.end();
         areanum++;
         area=strtok(NULL,"/");
         continue;
      }

   http.end();
   client.flush();
   client.stop();

    int i=0;
    while (strlen(doc["features"][i]["properties"]["event"]|"")!=0){
       if(oldarea!=areanum && oldarea!=999){
         tft.setTextSize(1);
         tft.println("");
       }
       oldarea=areanum;

       if (areanum==0) {

         strcpy(localcode,"00ff00");  strcpy(localcode_l,"000200");

         if (strstr(doc["features"][i]["properties"]["event"],"Watch")!=NULL){
           strcpy(localcode,"808000");   strcpy(localcode_l,"020200");//yellow

         } else if (strstr(doc["features"][i]["properties"]["event"],"Warning")!=NULL){
              if (strstr(doc["features"][i]["properties"]["event"],"Tornado")!=NULL){
                 strcpy(localcode,"FFFFFF");   strcpy(localcode_l,"FFFFFF");//white
              } else {
                 strcpy(localcode,"FF0000");   strcpy(localcode_l,"020000");//red
              }
         } else if (strstr(doc["features"][i]["properties"]["event"],"Statement")!=NULL){
            strcpy(localcode,"0000FF");         strcpy(localcode_l,"000002");//blue
         }

         int hour=timeClient.getHours();
         if ((hour>=21 || hour<6) && strcmp(localcode_l,"FFFFFF")!=0){
             strcpy(localcode_l,"000000");
             strcpy(localcode, "000000");
       }

         udp.beginPacket("192.168.1.255",4100);
         udp.print(localnum);
         udp.print(" 10000 12x");
         udp.print(localcode);
         udp.print(" 0x000000");
         udp.endPacket();

         vTaskDelay(500);

         //udp1.beginPacket("192.168.1.255",5100);
         //udp1.print("0 + 000000 ");
         //udp1.print(localcode_l);
         //udp1.endPacket();

         localnum++;
       }

       if (areanum==0)
          textcolor=WROVER_MAGENTA;
        else 
          textcolor=WROVER_GREEN;

       tft.setTextColor(textcolor);tft.setTextSize(1);
       tft.print(codes[areanum]);
       tft.print("  ");

        textcolor=WROVER_GREEN;
        if (strstr(doc["features"][i]["properties"]["event"],"Watch")!=NULL){
           textcolor=WROVER_YELLOW; //yellow
         } else if (strstr(doc["features"][i]["properties"]["event"],"Warning")!=NULL){
              if (strstr(doc["features"][i]["properties"]["event"],"Tornado")!=NULL)
                 textcolor=WROVER_WHITE; //white
              else
                 textcolor=WROVER_RED; //red
         } else if (strstr(doc["features"][i]["properties"]["event"],"Statement")!=NULL){
              textcolor=WROVER_BLUE; //blue
         }

       tft.setTextColor(textcolor);tft.setTextSize(1);
       tft.print(doc["features"][i]["properties"]["event"].as<const char*>());
       tft.print("  ");

       memset(mergebuff,0,100);
       strncpy(mergebuff,doc["features"][i]["properties"]["expires"].as<const char*>(),16);
       tft.println(mergebuff);
       //tft.println(doc["features"][i]["properties"]["expires"].as<const char*>());
       i++;
    }
    areanum++;
    area=strtok(NULL,"/");
}
    tft.setTextColor(WROVER_GREEN);tft.setTextSize(2);
    tft.println("$$");

    udpd.println();
    udpd.endPacket();

    vTaskDelay(60000);
   
 }

}