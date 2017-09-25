#include <ESP8266HTTPClient.h>

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include "FS.h"
#include <CapacitiveSensor.h>
#include <aJSON.h>

const String URL = "http://192.168.100.113:1337";
const String APP_ID = "APPLICATION_ID";

bool confed = false;
CapacitiveSensor   cs_4_2 = CapacitiveSensor(14, 12);

int threshold = 100;
int long_press = 350;
int short_press = 90;
int new_char = 1200;
int new_word = 3000;
int reset = 6000;

String queue = "";
String prev_sent = "";
bool started = false;

String id = "";
String sessionToken = "";
String userId = "";
String deviceName = "";

void setup() {

  // DEBUGGING, REMOVE WHEN DONE!
  //WiFi.disconnect();

  Serial.begin(115200);
  SPIFFS.begin();

  String def = get_name();
  def.trim();
  deviceName = get_device();
  runWifi(def);
  sessionToken = def;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  HTTPClient http;
  http.begin(URL + "/parse/users/me");
  http.addHeader("X-Parse-Application-Id", APP_ID);
  http.addHeader("X-Parse-Session-Token", sessionToken);

  if (http.GET()) {
    String out = http.getString();
    Serial.println(sessionToken);
    aJsonObject* jsonObject = aJson.parse(const_cast<char*> (out.c_str()));
    aJsonObject* objId = aJson.getObjectItem(jsonObject, "objectId");
    userId = objId->valuestring;
  }


  id = get_id();
  Serial.println(id);
}

int getVal() {
  long total1 =  cs_4_2.capacitiveSensor(30);
  delay(100);
  return total1;
}

String writeData(String queue, String i) {
  HTTPClient http;
  if (queue == prev_sent && i != "") {
    return i;
  } else {
    prev_sent = queue;
  }
  if (i == "") {
    http.begin(URL + "/parse/classes/Queue");
  } else {
    http.begin(URL + "/parse/classes/Queue/" + i);
  }
  if (sessionToken == "")
    return i;
  http.addHeader("X-Parse-Application-Id", APP_ID);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Parse-Session-Token", sessionToken);
  String data = "{\"deviceName\": \"" + deviceName + "\", \"queue\": \"" + queue + "\", \"started\": " + (started ? "true" : "false") + ", \"ACL\": {\"" + userId + "\": {\"read\": true, \"write\": true}, \"*\": {}}}";
  if (i == "") {
    http.POST(data);
    String out = http.getString();
    http.end();
    aJsonObject* jsonObject = aJson.parse(const_cast<char*> (out.c_str()));
    aJsonObject* objId = aJson.getObjectItem(jsonObject, "objectId");
    id = objId->valuestring;
    return objId->valuestring;
  } else {
    http.sendRequest("PUT", data);
    String out = http.getString();
    http.end();
    if (strstr(out.c_str(), "\"error\"") != NULL) {
      Serial.println(out);
      id = get_id();
      return id;
    }
    return i;
  }
}

String get_id() {
  HTTPClient http;
  http.begin(URL + "/parse/classes/Queue");
  http.addHeader("X-Parse-Application-Id", APP_ID);
  http.addHeader("X-Parse-Session-Token", sessionToken);
  http.sendRequest("GET", "where={\"deviceName\": \"Blinky\"}");
  String out = http.getString();
  //  if (!out) {
  //    return "";
  //  }
  aJsonObject* jsonObject = aJson.parse(const_cast<char*> (out.c_str()));
  aJsonObject* objId = aJson.getObjectItem(jsonObject, "results");
  Serial.println(out);
  if (aJson.getArraySize(objId) != 0) {
    aJsonObject* result = aJson.getArrayItem(objId, 0);
    return aJson.getObjectItem(result, "objectId")->valuestring;
  } else {
    return "";
  }
}

void loop()
{
  int value;
  long change, start_millis;
  if (strstr(queue.c_str(), "..--") != NULL) {
    queue = "";
    started = true;
  } else if (strstr(queue.c_str(), "......") != NULL) {
    started = false;
    queue = "";
  }
  value = getVal();
  Serial.println(value);
  start_millis = millis();
  while (value < threshold) {
    value = getVal();
    change = millis() - start_millis;
    if (new_char < change) {
      break;
    }
  }
  if (new_char < change) {
    if (started == true) {
      if (!queue.endsWith("|") && !queue.endsWith(" ") && queue.length() != 0)
        queue += "|";
    }
  }
  if (value > threshold) {
    start_millis = millis();
    while (value > threshold) {
      value = getVal();
    }
    change = millis() - start_millis;
    if (change > short_press && change < long_press) {
      queue += ".";
    } else if (change > long_press && change < reset) {
      queue += "-";
    } else if (change > reset) {
      queue = "";
    }
  }
  Serial.println(queue);
  writeData(queue, id);
}


///////////////////////////////////////////////

String get_name() {
  File f = SPIFFS.open("/session.txt", "r");
  if (!f) {
    return "";
  } else {
    return f.readString();
  }
}

void write_name(String hostname) {
  File f = SPIFFS.open("/session.txt", "w");
  f.println(hostname);
}

String get_device() {
  File f = SPIFFS.open("/device.txt", "r");
  if (!f) {
    return "Blinky";
  } else {
    String out = f.readString();
    out.trim();
    return out;
  }
}

void write_device(String hostname) {
  File f = SPIFFS.open("/device.txt", "w");
  f.println(hostname);
}

String authenticate(String username, String password) {
  HTTPClient http;
  http.begin(URL + "/parse/login?username=" + urlencode(username) + "&password=" + urlencode(password));
  http.addHeader("X-Parse-Application-Id", APP_ID);
  http.addHeader("Content-Type", "application/json");
  int code = http.GET();
  String out = http.getString();
  Serial.println(out);
  if (code == 200) {
    aJsonObject* jsonObject = aJson.parse(const_cast<char*> (out.c_str()));
    aJsonObject* sessionToken = aJson.getObjectItem(jsonObject, "sessionToken");
    return sessionToken->valuestring;
  } else {
    return "";
  }
}

void setconfed(WiFiManager *myWiFiManager) {
  confed = true;
}

void runWifi(String def) {
  char defa[24];
  def.toCharArray(defa, 24);
  WiFiManagerParameter username("username", "Username (if no account, create one in the app", "", 24);
  WiFiManagerParameter password("password", "Password", "", 24);
  WiFiManagerParameter device("device", "Device Name", "Blinky", 24);
  WiFiManager wifiManager;
  wifiManager.addParameter(&username);
  wifiManager.addParameter(&password);
  wifiManager.addParameter(&device);
  wifiManager.setAPCallback(setconfed);
  wifiManager.autoConnect("Blink Device");
  if (confed) {
    sessionToken = authenticate(username.getValue(), password.getValue());
    write_name(sessionToken);
    write_device(device.getValue());
    ESP.reset();
  }
}




// URL ENCODE ///////////////////////////////////////////////////





String urldecode(String str)
{

  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == '+') {
      encodedString += ' ';
    } else if (c == '%') {
      i++;
      code0 = str.charAt(i);
      i++;
      code1 = str.charAt(i);
      c = (h2int(code0) << 4) | h2int(code1);
      encodedString += c;
    } else {

      encodedString += c;
    }

    yield();
  }

  return encodedString;
}

String urlencode(String str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      code2 = '\0';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
      //encodedString+=code2;
    }
    yield();
  }
  return encodedString;

}

unsigned char h2int(char c)
{
  if (c >= '0' && c <= '9') {
    return ((unsigned char)c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return ((unsigned char)c - 'a' + 10);
  }
  if (c >= 'A' && c <= 'F') {
    return ((unsigned char)c - 'A' + 10);
  }
  return (0);
}
