/*  counter down untuk traffic light dengan input MERAH, KUNING, HIJAU, 12V
 *  
 *  mode flashing: pesan HATI-HATI dihilangkan
 * 
 */
 
#define USE_ADAFRUIT_GFX_LAYERS

#include <MatrixHardware_ESP32_V0.h>                // This file contains multiple ESP32 hardware configurations, edit the file to define GPIOPINOUT (or add #define GPIOPINOUT with a hardcoded number before this #include)

#include <SmartMatrix.h>
#include <WiFi.h>
#include <Preferences.h>

#include <Fonts/Roboto_Bold_44.h>
#include <Fonts/Open_Sans_Condensed_Bold_20.h>
#include <Fonts/URW_Gothic_L_Demi_20.h>

#define COLOR_DEPTH 24                  // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24)
const uint16_t kMatrixWidth = 64;       // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight = 48;      // Set to the height of your display
const uint8_t kRefreshDepth = 36;       // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On ESP32: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
const uint8_t kPanelType = SM_PANELTYPE_HUB75_16ROW_32COL_MOD4SCAN; //SM_PANELTYPE_HUB75_32ROW_MOD16SCAN;   // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);        // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);

const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);
SMARTMATRIX_ALLOCATE_GFX_MONO_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, 6*1024, 1, COLOR_DEPTH, kScrollingLayerOptions);

#if 1
  const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
  SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
#else
  const uint8_t kMonoLayerOptions = (SM_GFX_MONO_OPTIONS_NONE);
  SMARTMATRIX_ALLOCATE_GFX_MONO_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kMonoLayerOptions);  
#endif

#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define ORANGE  0xFC00

// enable this to see the intermediate drawing steps, otherwise screen will only be updated at the end of each test (this slows the tests down considerably)
const bool swapAfterEveryDraw = false;

const int redPin = 34; 
const int yelPin = 35; 
const int grePin = 32;

int bacaIn[3];

int cdRY = 3;
int cdY = 3;
int cdR = 80;
int cdG = 25;

bool runTeks = 0;
bool hatiOn = false;

unsigned long ms_current  = 0;
unsigned long ms_previous = 0;

unsigned int countR = 1;
unsigned int durR = 120;
unsigned int countY = 1;
unsigned int durY = 1;
unsigned int countG = 1;
unsigned int durG = 25;

//Wi-Fi
//const char* ssid     = "cd-sg1";
//const char* ssid     = "cd-sg2";
//const char* ssid     = "cd-sg3";
//const char* ssid     = "cd-sg4";

const char* ssid     = "amp-sg1";
//const char* ssid     = "amp-sg2";

const char* password = "dishub2022";

char teksRed1[60] = "SILAHKAN BERHENTI DI BELAKANG ZEBRA CROSS"; 
char teksRed2[60] = "PATUHI RAMBU LALU LINTAS"; 
char teksRed3[60] = "JAGA KESELAMATAN BERLALULINTAS"; 
char teksRed[180];
char teksGreen[60] = "SILAHKAN LANJUTKAN PERJALANAN ANDA"; 

String strTeksRed;
String memInfoSatu, memInfoDua, memInfoTiga, memInfoGreen;

WiFiServer server(80);
String header;

// Preferences /EEPROM
Preferences preferences;


void setup() {
  pinMode(redPin, INPUT);
  pinMode(yelPin, INPUT);
  pinMode(grePin, INPUT);

  Serial.begin(115200);
  startup_wifi();
  
  matrix.addLayer(&backgroundLayer); 
  matrix.addLayer(&scrollingLayer); 
  matrix.begin();
  matrix.setBrightness(200); //max = 255

  backgroundLayer.drawPixel(1,1, {0xff, 0, 0});
  backgroundLayer.swapBuffers();


  preferences.begin("teksrun", false);
//  preferences.putString("infosatu", String(teksRed1));
//  preferences.putString("infodua", String(teksRed2));
//  preferences.putString("infotiga", String(teksRed3));
//  preferences.putString("infogreen", String(teksGreen));
  
  memInfoSatu = preferences.getString("infosatu", String(teksRed1));
  memInfoDua = preferences.getString("infodua", String(teksRed2));
  memInfoTiga = preferences.getString("infotiga", String(teksRed3));
  memInfoGreen = preferences.getString("infogreen", String(teksGreen));
  
  memInfoSatu.toCharArray(teksRed1, 60);
  memInfoDua.toCharArray(teksRed2, 60);
  memInfoTiga.toCharArray(teksRed3, 60);
  memInfoGreen.toCharArray(teksGreen,60);

  strTeksRed = String(teksRed1)+ "          " + 
               String(teksRed2)+ "          " + 
               String(teksRed3);
  strTeksRed.toCharArray(teksRed, strTeksRed.length() + 1);
  
}

void startup_wifi()
{
  // wait for Serial to be ready
  delay(50);

  Serial.print("Setting AP (Access Point)...");
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.begin();
}

void loop() {   
  byte counterWIFI = 0; 
  
  WiFiClient client = server.available();
  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      
      ms_current = millis();

      if ( ms_current - ms_previous >= 1000)
      {
        ms_previous = ms_current;
        Serial.println(ms_current);
        count_down();

        Serial.println("in while");
        counterWIFI++;
        Serial.println(counterWIFI);
        if (counterWIFI > 5){
          counterWIFI = 0;
          break;
        }
      } 

      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;

        if (c == '\n') {                    // if the byte is a newline character          
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
   
            if (header.indexOf("cdRedGreen") >= 0) {
              Serial.println("set counterdown");   
              int iCdR = header.indexOf("info1=");
              int iEnd = header.indexOf("HTTP/1.1");
              String teksFromWeb = header.substring(iCdR, iEnd - 1);

              String teksInfo1 = teksFromWeb.substring(teksFromWeb.indexOf("info1=") + 6 , teksFromWeb.indexOf("&info2="));
              String teksInfo2 = teksFromWeb.substring(teksFromWeb.indexOf("info2=") + 6 , teksFromWeb.indexOf("&info3="));
              String teksInfo3 = teksFromWeb.substring(teksFromWeb.indexOf("info3=") + 6 , teksFromWeb.indexOf("&infoG="));
              String teksInfoG = teksFromWeb.substring(teksFromWeb.indexOf("infoG=") + 6 , iEnd);
           
              for(int i = 0; i < teksInfo1.length(); i++ ){
                if (teksInfo1[i] == '+') teksInfo1[i] = ' ';
              }
              for(int i = 0; i < teksInfo2.length(); i++ ){
                if (teksInfo2[i] == '+') teksInfo2[i] = ' ';
              }
              for(int i = 0; i < teksInfo3.length(); i++ ){
                if (teksInfo3[i] == '+') teksInfo3[i] = ' ';
              }
              for(int i = 0; i < teksInfoG.length(); i++ ){
                if (teksInfoG[i] == '+') teksInfoG[i] = ' ';
              }
              
              teksInfo1.toUpperCase();
              teksInfo2.toUpperCase();
              teksInfo3.toUpperCase();
              teksInfoG.toUpperCase();

              preferences.putString("infosatu", String(teksInfo1));
              preferences.putString("infodua", String(teksInfo2));
              preferences.putString("infotiga", String(teksInfo3));
              preferences.putString("infogreen", String(teksInfoG));                           

              teksInfo1.toCharArray(teksRed1, 60); 
              teksInfo2.toCharArray(teksRed2, 60);
              teksInfo3.toCharArray(teksRed3, 60);
              teksInfoG.toCharArray(teksGreen, 60);
              strTeksRed = String(teksRed1)+ "          " + String(teksRed2)+ "          " + String(teksRed3);              
              strTeksRed.toCharArray(teksRed, strTeksRed.length() + 1);   
              
              teksInfo1 = "";
              teksInfo2 = "";
              teksInfo3 = "";
              teksInfoG = "";
              strTeksRed = "";         
              
            } else if (header.indexOf("cdGreen") >= 0) {
              Serial.println("set counterdown hijau");

              int iCdG = header.indexOf("infoG=");
              int iEnd = header.indexOf("HTTP/1.1");
              
              String teksInfoG = header.substring(iCdG, iEnd - 1);
           
              for(int i = 0; i < teksInfoG.length(); i++ ){
                if (teksInfoG[i] == '+') teksInfoG[i] = ' ';
              }
              
              teksInfoG.toUpperCase();              
              teksInfoG.toCharArray(teksGreen, teksInfoG.length() + 1);               
            } 
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println("h5 {text-align: center;}");
            client.println(".button { border-radius: 8px; background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 18px; margin: 2px; cursor: pointer;}");
            client.println(".button1 {background-color: #FF1111;}");
            client.println(".button2 {background-color: #555555;}");
            client.println("input { border-radius: 8px; background-color: #eee; width: 95%; padding: 10px;}");
            client.println("</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>Counter Down 1</h1>");
            client.println("<hr>");                                 
                       
            client.println("<p><h4>Running Text Merah</h4></p>");
            client.println("<form method=\"GET\" action=\"/cdRedGreen\">");
            client.println("  <p><h5>Info 1</h5></p>");
            client.println("  <input type=\"text\" name=\"info1\" value=\"" + String(teksRed1) + "\" maxlength=\"59\">");  
            client.println("  <p><h5>Info 2</h5></p>");
            client.println("  <input type=\"text\" name=\"info2\" value=\"" + String(teksRed2) + "\" maxlength=\"59\">");  
            client.println("  <p><h5>Info 3</h5></p>");
            client.println("  <input type=\"text\" name=\"info3\" value=\"" + String(teksRed3) + "\" maxlength=\"59\">");  
            
            client.println("  <br><br>");                                 
            client.println("  <hr>");                                 

            client.println("<p><h4>Running Text Hijau</h4></p>");
            client.println("  <input type=\"text\" name=\"infoG\" value=\"" + String(teksGreen) + "\" maxlength=\"59\">");     
                        
            client.println("  <p><button class=\"button\">Simpan</button></p>");
            client.println("</form>");
            
                        
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
            
//      delay(1000);
    }

//    Serial.println(header);
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");

//    delay(1000);
  }

  ms_current = millis();

  if ( ms_current - ms_previous >= 1000)
  {
    ms_previous = ms_current;
    Serial.println(ms_current);
    count_down();
  }  
}

void count_down()
{
  baca_pin();    
  if(bacaIn[1] == 0 && bacaIn[0] == 0)  // red yellow
    {
      running_text_none();
      
      teks_hati_hati();
      backgroundLayer.swapBuffers();      
    }
    else if(bacaIn[0] == 0) // red
    {   
      if (countR == 0) runTeks = 0;   //  agar running teks berpindah dari hijau ke merah
                                      //  ada kasus traffic light merah tapi running teks hijau
      font_angka();
                                
      countR++;
      durR = countR;
  
      cdG = durG - 1;
      countG = 0;

      if ( runTeks == 0 ) {
        running_text_red();
        runTeks = 1;
      }

      count_down_red(cdR);
      backgroundLayer.swapBuffers();

      cdR--;
      if (cdR < 0) cdR = 0;
    } 
    else if(bacaIn[1] == 0) // yellow
    {
      if ( hatiOn == true ) {     
        running_text_none();
        
        teks_hati_hati();
        backgroundLayer.swapBuffers();
      }      
    }
    else if(bacaIn[2] == 0) // green
    { 
      if ( hatiOn == false ) hatiOn = true; 
      
      if ( countG == 0 ) runTeks = 0;   //  agar running teks berpindah dari merah ke hijau
                                      //  ada kasus traffic light hijau tapi running teks merah
      font_angka();
      
      countG++;
      durG = countG;
      
      cdR = durR - 1;
      countR = 0;

      if ( runTeks == 0 ) {
        running_text_green();
        runTeks = 1;
      }

      count_down_green(cdG);
      backgroundLayer.swapBuffers();

      cdG--;
      if(cdG < 0) cdG = 0;
    } 
    else {     
      Serial.println("masuk else...");
      delay(100);
      baca_pin();

      if( bacaIn[1] == 1 ) // lampu kuning padam
      {
        if ( hatiOn == true ) hatiOn = false;
      
        running_text_none();
        
        backgroundLayer.fillScreen(BLACK);     
        backgroundLayer.swapBuffers();            
      }
      
    }
}

unsigned long testFillScreen() {
  unsigned long start = micros();
  backgroundLayer.fillScreen(BLACK);
  backgroundLayer.fillScreen(RED);
  backgroundLayer.fillScreen(GREEN);
  backgroundLayer.fillScreen(BLUE);
  backgroundLayer.fillScreen(BLACK);
  return micros() - start;
}

unsigned long count_down_red(int x) {
  int x_ex_pos = 0;
  
  if (x > 99) x_ex_pos = -7;
  else if (x > 9) x_ex_pos = 8;
  else if (x >= 0) x_ex_pos = 19;
  
  font_angka();
  backgroundLayer.fillScreen(BLACK);

  backgroundLayer.setRotation(0);
  backgroundLayer.setCursor(x_ex_pos , 32);
  backgroundLayer.setTextColor(RED);
  backgroundLayer.setTextSize(1);
  backgroundLayer.print(x);
}

unsigned long count_down_green(int x) {
  int x_ex_pos = 0;
  
  if (x > 99) x_ex_pos = -7;
  else if (x > 9) x_ex_pos = 8;
  else if (x >= 0) x_ex_pos = 19;
  
  font_angka();
  backgroundLayer.fillScreen(BLACK);

  backgroundLayer.setRotation(0);
  backgroundLayer.setCursor(x_ex_pos , 32);
  backgroundLayer.setTextColor(GREEN);
  backgroundLayer.setTextSize(1);
  backgroundLayer.print(x);

}

int teks_hati_hati(){
    backgroundLayer.fillScreen(BLACK);    
    backgroundLayer.setTextSize(2);
    backgroundLayer.setFont(gohufont11b);
    backgroundLayer.setRotation(0);    
    backgroundLayer.setTextColor(ORANGE);
    backgroundLayer.setCursor(9 , 23);
    backgroundLayer.print("HATI");    
    backgroundLayer.setTextColor(ORANGE);
    backgroundLayer.setCursor(9 , 45);
    backgroundLayer.print("HATI");       
}

void baca_pin()
{ 
  bacaIn[0] = digitalRead(redPin);
  bacaIn[1] = digitalRead(yelPin);
  bacaIn[2] = digitalRead(grePin);
  
  Serial.print(bacaIn[0]);
  Serial.print(bacaIn[1]);
  Serial.println(bacaIn[2]);
}

void running_text_red()
{
  scrollingLayer.setMode(wrapForward);
  scrollingLayer.setColor({0xff, 0, 0});
  scrollingLayer.setSpeed(50);
  scrollingLayer.setTextSize(1);
  scrollingLayer.setFont(&URW_Gothic_L_Demi_20);
  scrollingLayer.setOffsetFromTop(kMatrixHeight - 15);
  scrollingLayer.start(teksRed, -1);   
}

void running_text_green()
{
  scrollingLayer.setMode(wrapForward);
  scrollingLayer.setColor({0, 0xff, 0});
  scrollingLayer.setSpeed(80);
  scrollingLayer.setTextSize(1);
  scrollingLayer.setFont(&URW_Gothic_L_Demi_20);
  scrollingLayer.setOffsetFromTop(kMatrixHeight - 15);
  scrollingLayer.start(teksGreen, -1);  
}

void running_text_none()
{
  runTeks = 0;
  
  scrollingLayer.setMode(wrapForward);
  scrollingLayer.setColor({0, 0, 0});
  scrollingLayer.setSpeed(40);
  scrollingLayer.setFont(gohufont11b);
  scrollingLayer.setOffsetFromTop(kMatrixHeight - 12);
  scrollingLayer.start("", 0);  
}

void font_angka()
{
  backgroundLayer.setTextSize(1);
  backgroundLayer.setFont(&Roboto_Bold_44);  
}

void font_huruf()
{
    backgroundLayer.setFont(gohufont11b);
}
