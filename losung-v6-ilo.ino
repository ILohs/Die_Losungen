// Die Losungen - every day a new bible verse
// ScruffR & Ingo Lohs
// v1.0, 24.07.2017
// works with Particle Photon v0.6.2 and Electron v0.6.1
// Display: Hoverlabs Beam

// Include LIBs from Particle ---------------------

#include <SparkJson.h> // v0.0.2
#include <blynk.h>     // v0.4.7

// Include LIBs via Tabs ---------------------

#include "application.h"
#include "beam.h"
#ifdef IGNORE 
#include "charactermap.h"
#include "frames.h"
#endif

SYSTEM_MODE(SEMI_AUTOMATIC)
SYSTEM_THREAD(ENABLED)

// USERs CONFIG AREA: WIFI & BLYNK ---------------------

#if ( PLATFORM_ID != PLATFORM_ELECTRON_PRODUCTION )
const char defaultSSID[] = "Ingo Heim";
const char defaultPWD[]  = "9209360667609651";
const int  defaultAUTH   = WPA2;
const int  defaultWCIPH  = WLAN_CIPHER_AES_TKIP;
#endif 

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
const char auth[] = "6e59a31b78554bed8e9599d313f19a41";

// DEFINITIONS ---------------------

// Attach virtual serial terminal to Virtual Pin V0
WidgetTerminal terminal(V0);

const int pinSYNC   = D7;
const int pinRESET  = D3;
const int pinIRQ    = D2; 
const int BEAMCOUNT =  1; // USERs CONFIG: set number of beams daisy chained together - works with 1, 2 or 3

const int loopCount             =   1;
const int msDelayPerCharPlay    = 460;

enum DISPLAY_METHOD {
    NONE,
    PLAY,
    DISPLAY
};

Beam b = Beam(pinRESET, pinIRQ, BEAMCOUNT);

/*
      "Datum": "2017-05-05T00:00:00",
      "Wtag": "Freitag", 
      "Sonntag": {},
      "Losungstext": "Du sollst dein Herz nicht verhärten und deine Hand nicht zuhalten gegenüber deinem armen Bruder.",
      "Losungsvers": "5.Mose 15,7",
      "Lehrtext": "Wenn jemand dieser Welt Güter hat und sieht seinen Bruder darben und verschließt sein Herz vor ihm, wie bleibt dann die Liebe Gottes in ihm?",
      "Lehrtextvers": "1.Johannes 3,17"
*/

// TESTING ---------------------

char datum[20];        //= "2017-01-01T00:00:00";
char wochentag[12];    //= "XXXXXXXXtag";
char sonntag[12];      //= "YYYYYYYYtag";
char losungstext[256]; //= "LosungsText: Das ist ein längerer Text ÄÖÜ";
char losungsvers[32];  //= "LosungVers ÄÖÜ";
char lehrtext[256];    //= "LehrText: Das ist ein längerer Text ÄÖÜ";
char lehrtextvers[32]; //= "LehrTextVers ÄÖÜ";
char customMessage[256] = "Meine Gemeinde: www.emf-bielefeld.de";

uint32_t msLastTime;
uint32_t msDelay;
int      lastDay = 0;

// SERIAL LOG ---------------------

SerialLogHandler myLogger(LOG_LEVEL_INFO);

// BLYNK Terminal Widget ---------------------

BLYNK_WRITE(V0)
{
  if (strlen((const char*)param.asStr())) {
    strncpy(customMessage, (const char*)param.asStr(), sizeof(customMessage));
    customMessage[sizeof(customMessage)-1] = '\0';    
    terminal.print(customMessage);
  } 
  else {
    terminal.print(customMessage);
    memset(customMessage, 0, sizeof(customMessage)); 
  }
  terminal.flush();
}

// FUNCTION ---------------------

volatile bool proceede = false;
void isrBeam() {
    proceede = true;
}

// SETUP ---------------------

void setup() {
  Wire.setSpeed(CLOCK_SPEED_400KHZ);
  Wire.begin();
  b.begin();
  b.setSpeed(1);
  b.print("Die Losungen");
  b.play();
  delay(3000);

  // setup a time zone, which is part of the ISO6801 format 
  Time.zone(isDST() ? +2.00 : +1.00);  

  // Subscribe to the integration response event
  Particle.subscribe("hook-response/tageslosung", myHandler, MY_DEVICES);

  #if ( PLATFORM_ID != PLATFORM_ELECTRON_PRODUCTION )
  WiFi.on();
  if (!WiFi.hasCredentials()) {
    WiFi.setCredentials(defaultSSID, defaultPWD, defaultAUTH, defaultWCIPH);
    for(uint32_t _ms = millis(); millis() - _ms < 500; Particle.process());
    WiFi.off();
  }
  #endif
  
  Particle.connect();
  waitUntil(Particle.connected);

  Blynk.begin(auth);
  terminal.println("Die Losungen");
  terminal.println("Thanks to Richard Klose");
  terminal.println("with special thanks to");
  terminal.println("ScruffR for Coding");
  terminal.println("https://community.particle.io");
  terminal.println("Ingo Lohs");
  
  while(Serial.read() >= 0) Particle.process(); // flush USB Serial RX buffer

  Particle.publish("tageslosung", "MyLosung", PRIVATE);
}

// LOOP ---------------------

void loop() {
  static uint32_t msPublish = millis();

  Blynk.run();

  if (lastDay != Time.day() && millis() - msPublish > 60000) {
    Particle.syncTime(); 
    waitFor(Particle.syncTimeDone, 5000); // works with v0.6.1 with Photon & Electron
    msPublish = millis();
    if (lastDay == Time.day()) return; // if sync brought us back before midnight, try a bit later again
    // request new day's data
    Particle.publish("tageslosung", "MyLosung", PRIVATE);
  }

  print_out_on_Beam();
    
}

// FUNCTION to DISPLAY DATA ---------------------

void print_out_on_Beam() {
  static int step = 0;
  char  dynText[32];
  const char* txt = NULL;
  DISPLAY_METHOD method = NONE; 

  // don't procede if we need to wait a bit
  if(msDelay && millis() - msLastTime < msDelay) return;
  msDelay = 0;
 
  switch (step++) {
    case 0:
      txt = customMessage;
      method = PLAY;
      break;

    case 1:
    case 5:
      Time.zone(isDST() ? +2.00 : +1.00);  
      snprintf(dynText, sizeof(dynText), "%s", (const char*)Time.format("%d.%m.%Y %H:%M"));
      //snprintf(dynText, sizeof(dynText), "%s", (const char*)Time.format("%H:%M"));
      txt = dynText;
      method = DISPLAY;
      break;

    case 2:
      txt = wochentag; 
      method = DISPLAY;
      break;
      
    case 3:
      txt = losungstext;
      method = PLAY;
      break;
      
    case 4:
      txt = losungsvers;
      msDelay = 10000;
      method = DISPLAY;
      break;
      
    case 6:
      txt =lehrtext;
      method = PLAY;
      break;
      
    case 7:
      txt = lehrtextvers;
      msDelay = 10000;
      method = DISPLAY;
      break;
      
    default:
      step = 0;
      break;
  }
  
  Blynk.virtualWrite(V0, txt);
  Blynk.virtualWrite(V0, "\n"); // Carriage Return - every text displays in a new line

  b.print(txt); 
  switch (method) {
    case PLAY:
      b.setLoops(loopCount);
      //b.setScroll(LEFT,4);
      //b.setSpeed(6);
      b.play();
      msDelay = BEAMCOUNT * 2000 + strlen(txt) * msDelayPerCharPlay * loopCount;
      break;
    case DISPLAY:
      if (!msDelay) msDelay = 5000;
      b.display();
      break;
    default:
      break;
  }
  msLastTime = millis();

  // Beam can begin scrolling text with just print() and play() > Beam using ONLY Strings to display  
  /*
  // methods like setSpeed(), setScroll() setLoops(), setMode() can be called 
  //on the fly to change the existing default settings
  b.setSpeed(2);      //increase speed
  b.setSpeed(1);      //increase speed again!
  b.setSpeed(15);     //reduce speed to lowest setting
  */
}

// WEBHOOK HANDLER TO GET DATA ---------------------

void myHandler(const char *event, const char *data)
{
  static int cntEvents = 0;
  char mutableCopy[strlen(data)+1];
  strcpy(mutableCopy, data);

  // Start SparkJSON Example	    
  Log.info("%d: %s, data: %s", ++cntEvents, event, data);
  if (data) {
    terminal.println(data);
    terminal.println("--------end--------");
    terminal.flush();
    /* JSON Source - https://losungen.klose.cloud/api/today
    {
      "Datum": "2017-05-05T00:00:00",
      "Wtag": "Freitag", 
      "Sonntag": {},
      "Losungstext": "Du sollst dein Herz nicht verhärten und deine Hand nicht zuhalten gegenüber deinem armen Bruder.",
      "Losungsvers": "5.Mose 15,7",
      "Lehrtext": "Wenn jemand dieser Welt Güter hat und sieht seinen Bruder darben und verschließt sein Herz vor ihm, wie bleibt dann die Liebe Gottes in ihm?",
      "Lehrtextvers": "1.Johannes 3,17"
    }
    */
    
    // https://bblanchon.github.io/ArduinoJson/faq/how-to-determine-the-buffer-size/
    // https://bblanchon.github.io/ArduinoJson/assistant/
    StaticJsonBuffer<256> jsonBuffer;
	JsonObject& root = jsonBuffer.parseObject(mutableCopy);

	
	if (!root.success()) {
      Log.warn("parseObject() failed");
      return;
    }
             
    strncpy(datum       , root["Datum"       ], sizeof(datum)       );
    strncpy(wochentag   , root["Wtag"        ], sizeof(wochentag)   );
    strncpy(sonntag     , root["Sonntag"     ], sizeof(sonntag)     );
	strncpy(losungstext , root["Losungstext" ], sizeof(losungstext) );
	strncpy(losungsvers , root["Losungsvers" ], sizeof(losungsvers) );
	strncpy(lehrtext    , root["Lehrtext"    ], sizeof(lehrtext)    );
	strncpy(lehrtextvers, root["Lehrtextvers"], sizeof(lehrtextvers));

    lastDay = Time.day();
  }
  else {
    terminal.println("no JSON Records available - check the Webhook under Particle Cloud - Console - Integrations!");
  }
}

// FUNCTION TO CHECK SUMMER/WINTER-TIME ---------------------

bool isDST()
{ // Central European Summer Timer calculation
  int dayOfMonth = Time.day();
  int month = Time.month();
  int dayOfWeek = Time.weekday() - 1; // make Sunday 0 .. Saturday 6

  if (month >= 4 && month <= 9)
  { // March to September definetly DST
    return true;
  }
  else if (month < 3 || month > 10)
  { // before March or after October is definetly normal time
    return false;
  }

  // March and October need deeper examination
  boolean lastSundayOrAfter = (dayOfMonth - dayOfWeek > 24);
  if (!lastSundayOrAfter)
  { // before switching Sunday
    return (month == 10); // October DST will be true, March not
  }

  if (dayOfWeek)
  { // AFTER the switching Sunday
    return (month == 3); // for March DST is true, for October not
  }

  int secSinceMidnightUTC = Time.now() % 86400;
  boolean dayStartedAs = (month == 10); // DST in October, in March not
                                        // on switching Sunday we need to consider the time
  if (secSinceMidnightUTC >= 1 * 3600)
  { // 1:00 UTC (=2:00 CET/3:00 CEST)
    return !dayStartedAs;
  }

  return dayStartedAs;
}
