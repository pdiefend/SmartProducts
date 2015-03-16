// based on: http://zeflo.com/2014/esp8266-weather-display/
// HaD post: http://hackaday.com/2014/09/17/a-proof-of-concept-project-for-the-esp8266/

#define SSID "________" // insert your SSID
#define PASS "________" // insert your password
#define LOCATIONID "5201470" // location id
#define DST_IP "188.226.224.148" //api.openweathermap.org


void setup()
{
  // flash the LED to indicate a restart and give the user (me) time to open
  // a COM monitor
  pinMode(13,OUTPUT);
  for(int i = 0; i<3; i++){
    digitalWrite(13, HIGH);
    delay(500);
    digitalWrite(13, LOW);
    delay(500);
  }

  Serial.begin(115200);
  Serial.println("Begin");

  Serial1.begin(115200); // ESP8266
  Serial1.println("AT+RST"); // reset and test if module is ready

  // This doesn't work...
  /*
  if(Serial1.find("ready")) {
   Serial.println("WiFi - Module is ready");
   }
   else{
   Serial.println("Module doesn't respond.");
   while(1);
   }
   */

  // so this wall of code replaces it
  char rdy[5] = {
    'r', 'e', 'a', 'd', 'y'      };
  char chk[5] = {
    0,0,0,0,0      };
  boolean moduleReady = false; 

  // look for 2 seconds (plus a little...)
  for(int i = 0; i < 2000; i ++){
    while(Serial1.available()){
      chk[0] = chk[1];
      chk[1] = chk[2];
      chk[2] = chk[3];
      chk[3] = chk[4];
      chk[4] = Serial1.read();
      //Serial.println(chk[4]);
      if(rdy[0] == chk[0] &&
        rdy[1] == chk[1] &&
        rdy[2] == chk[2] &&
        rdy[3] == chk[3] &&
        rdy[4] == chk[4]){
        moduleReady = true;
      }
    }
    delay(1);
  }
  if(moduleReady)
    Serial.println("WiFi - Module is ready");
  else{
    Serial.println("Module didn't respond.");
    while(1);
  }

  // Okay, the module is working, lets connect to the network
  delay(1000);
  // try to connect to wifi 5 times before giving up
  boolean connected=false;
  for(int i=0;i<5;i++){
    if(connectWiFi()){
      connected = true;
      break;
    }
  }
  if (!connected){
    Serial.println("Connect failed permanently");
    while(1);
  }
  delay(5000);
  Serial.println("AT+CIPMUX=0"); // set to single connection mode
}
void loop()
{
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += DST_IP;
  cmd += "\",80";
  Serial1.println(cmd);
  Serial.println(cmd);
  if(Serial1.find("Error")) return;
  cmd = "GET /data/2.5/weather?id=";
  cmd += LOCATIONID;
  cmd += " HTTP/1.0\r\nHost: api.openweathermap.org\r\n\r\n";
  Serial1.print("AT+CIPSEND=");
  Serial1.println(cmd.length());
  if(Serial1.find(">")){
    Serial.print(">");
  }
  else{
    Serial1.println("AT+CIPCLOSE");
    Serial.println("connection timeout");
    delay(1000);
    return;
  }
  Serial1.print(cmd);
  unsigned int i = 0; //timeout counter
  int n = 1; // char counter
  char json[100]="{";
  while (!Serial1.find("\"main\":{")){
  } // find the part we are interested in.
  while (i<60000) {
    if(Serial1.available()) {
      char c = Serial1.read();
      json[n]=c;
      if(c=='}') break;
      n++;
      i=0;
    }
    i++;
  }
  Serial.println(json);
  delay(60000);
}

boolean connectWiFi()
{
  Serial1.println("AT+CWMODE=1");
  String cmd="AT+CWJAP=\"";
  cmd+=SSID;
  cmd+="\",\"";
  cmd+=PASS;
  cmd+="\"";
  Serial.println(cmd);
  Serial1.println(cmd);
  delay(2000);
  if(Serial1.find("OK")){
    Serial.println("OK, Connected to WiFi.");
    return true;
  }
  else{
    Serial.println("Can not connect to the WiFi.");
    return false;
  }
}

