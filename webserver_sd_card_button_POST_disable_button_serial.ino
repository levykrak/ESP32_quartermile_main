/*
ESP32-CAM
*/

#define Debug
// #define AP

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "FS.h"
#include "SD_MMC.h"

//PAddress local_ip(192, 168, 10, 1);
//IPAddress gateway(192, 168, 10, 1);
//IPAddress subnet(255, 255, 255, 0);

int licznik = 0;
char bufor[50];

// Replace with your network credentials
#ifdef AP
const char* ssid = "arek_mistrz";
const char* password = "123456789";
#else
const char* ssid = "bb9_optout_nomap";
const char* password = "aperVEMALB";
#endif

// Auxiliar variables to store the current output state
//String outputLEDState = "off";
const int led = 33;

// Set to true to define Relay as Normally Open (NO)
const char* PARAM_INPUT_1 = "lewa";
const char* PARAM_INPUT_2 = "prawa";

// Variables to save values from HTML form
String lewa;
String prawa;

// Variable to detect whether a new request occurred
bool newRequest = false;
bool stan;

String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

char id1[2];
//String id1;
int id2, id3;
char buf[60];


const char admin_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Dwie zmienne</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <h1>rawliteral</h1>
    <form action="/admin.html" method="POST">
      <label for="lewa">Lewa:</label>
      <input type="number" name="lewa">

      <br><br>

      <label for="prawa">Prawa:</label>
      <input type="number" name="prawa">
      <br><br>
  
  %BUTTONPLACEHOLDER%
  </form>
<script>

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      
      var outputStateM;
      if( this.responseText == 1){ 
        outputStateM = "<input type=\"submit\" value=\"GO!\" disabled=\"disabled\">";
      }
      else { 
        outputStateM = "<input type=\"submit\" value=\"GO!\">";
      }
        document.getElementById("outputState").innerHTML = outputStateM;
    }
  };
  xhttp.open("GET", "/state", true);
  xhttp.send();
}, 2000 ) ;
</script>
</body>
</html>
)rawliteral";

// Replaces placeholder with button section in your web page
String processor(const String& var) {
  //Serial.println(var);
  if (var == "BUTTONPLACEHOLDER") {
    String buttons = "";
    //String outputStateValue = outputState();
    buttons += "<span id=\"outputState\"></span></h4>";
    return buttons;
  }
  return String();
}

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

void initSDCard() {
  if (!SD_MMC.begin()) {
#ifdef Debug
    Serial.println("Card Mount Failed");
#endif
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
#ifdef Debug
    Serial.println("No SD card attached");
#endif
    return;
  }

#ifdef Debug
  Serial.print("SD_MMC Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
#endif
}

void deinitSDCard() {
  SD_MMC.end();
}


void setup() {
  pinMode(led, OUTPUT);
  Serial.begin(115200);
  inputString.reserve(10);
  //initWiFi();
  initSDCard();
// Remove the password parameter, if you want the AP (Access Point) to be open
#ifdef AP
  WiFi.softAP(ssid, password);
#else
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println(WiFi.localIP());
#endif

  //WiFi.softAPConfig(local_ip, gateway, subnet);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(SD_MMC, "/www/csv/print.html", "text/html");
  });


  /*
  server.on("/foo.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(SD_MMC, "/www/foo.html", "text/html");
  });
  */
  server.serveStatic("/", SD_MMC, "/");

  // Handle request (form)
  server.on("/admin.html", HTTP_POST, [](AsyncWebServerRequest* request) {
    int params = request->params();
    for (int i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->isPost()) {
        // HTTP POST input1 value (direction)
        if (p->name() == PARAM_INPUT_1) {
          lewa = p->value().c_str();
          Serial.print("lewa: ");
          Serial.println(lewa);
        }
        // HTTP POST input2 value (steps)
        if (p->name() == PARAM_INPUT_2) {
          prawa = p->value().c_str();
          Serial.print("prawa: ");
          Serial.println(prawa);
        }
      }
    }
    //request->send(SD_MMC, "/www/index.html", "text/html");
    request->send_P(200, "text/html", admin_html, processor);
    newRequest = true;
  });

  // Route for root / web page
  server.on("/admin.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", admin_html, processor);
  });
  // Send a GET request to <ESP_IP>/state
  server.on("/state", HTTP_GET, [](AsyncWebServerRequest* request) {
    //  request->send(200, "text/plain", String(digitalRead(output)).c_str());
    request->send(200, "text/plain", String(stan));
  });

  server.begin();
}

void readSerial() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag so the main loop can
    // do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}

//void appendFile(fs::FS &fs, const char * path, char * message){
void appendFile(fs::FS& fs, const char* path, const char* message) {
#ifdef Debug
  Serial.printf("Appending to file: %s\n", path);
#endif

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
#ifdef Debug
    Serial.println("Failed to open file for appending");
#endif
    return;
  }
  if (file.print(message)) {
#ifdef Debug
    Serial.println("Message appended");
#endif
  } else {
    Serial.println("Append failed");
  }
}



/*
void split(String wejscie) {
  int maxIndex = wejscie.length() - 1;
  int index = 0;
  int next_index;
  String data_word;
  do {
    next_index = wejscie.indexOf(';', index);
    data_word = wejscie.substring(index, next_index);
    Serial.print("Output line: ");
    Serial.println(data_word);
    index = next_index + 1;
  } while ((next_index != -1) && (next_index < maxIndex));
}
*/


void loop() {
  /*
  licznik++;
  delay(5000);
  stan = 1;
  delay(5000);
  stan = 0;
*/

  if (newRequest) {  //tutaj wchodzimy gdy wcisniemy przycisk GO! przez www
    Serial.println("tu");
    newRequest = false;
  }
  /*
  delay(5000);
  licznik++;
  sprintf(bufor, "%d", licznik);
  Serial.println(licznik);
  appendFile(SD_MMC, "/www/index.html", "dupa");
  appendFile(SD_MMC, "/www/index.html", bufor);
  appendFile(SD_MMC, "/www/index.html", "\n");
  */
  //-----------!!!--------
  readSerial();   //musimy odczytać co mamy w serialu
  if (stringComplete) {
#ifdef Debug
    Serial.println(inputString);
#endif
    id2 = 0;
    id3 = 0;
    inputString.toCharArray(buf, inputString.length());  //przerabiamy inputString na char array
    int n = sscanf(buf, "%c;%d;%d", &id1, &id2, &id3);   //wyciagamy poszczegolne info
#ifdef Debug

    //--------------- zapis na karcie SD --------------
    //strona	numer	fallstart	RT	  ET	  ET+RT
    //id1           id2       id3   id4   id5
    // if (id1[0] == 'L' || id1[0] == 'P') {  //jesli rowne "L"
    if (id1[0] == 'L' || id1[0] == 'P') {  //jesli rowne "L"
      Serial.println(buf);
      Serial.print("n= ");
      Serial.println(n);
      Serial.print("id1= ");
      Serial.println(id1);
      Serial.print("id2= ");
      Serial.println(id2);
      Serial.print("id3= ");
      Serial.println(id3);

      appendFile(SD_MMC, "/CWIARTKA.TXT", id1);
      appendFile(SD_MMC, "/CWIARTKA.TXT", ";");
      if (id1[0] == 'L') {
        sprintf(bufor, "%s;", lewa);  //zmieniamy na char*
        lewa = "";
      } else {
        sprintf(bufor, "%s;", prawa);  //zmieniamy na char*
        prawa = "";
      }
      appendFile(SD_MMC, "/CWIARTKA.TXT", bufor);
      sprintf(bufor, "%d;", id2);  //zmieniamy na char*
      appendFile(SD_MMC, "/CWIARTKA.TXT", bufor);
      sprintf(bufor, "%d", id3);
      appendFile(SD_MMC, "/CWIARTKA.TXT", bufor);
      appendFile(SD_MMC, "/CWIARTKA.TXT", "\n");
    }

#endif
    //split(inputString);
    // clear the string:
    inputString = "";
    stringComplete = false;
  }
}
