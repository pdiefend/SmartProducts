#define SSID "________" // insert your SSID
#define PASS "________" // insert your password

String response = "Success";

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

  if(resetModule())
    Serial.println("WiFi - Module is ready");
  else{
    Serial.println("Module didn't respond.");
    while(1);
  }

  // Okay, the module is working, lets connect to the network
  delay(1000);
  // try to connect to wifi 5 times before giving up
  if (!connectWiFi(5)){
    Serial.println("Connect failed permanently");
    while(1);
  }

  delay(1000);
  Serial1.println("AT+CIPMUX=1"); // set to multi connection mode
  delay(1000);

  // Get our ip address
  Serial1.println("AT+CIFSR");
  for(int i = 0; i < 1000; i++){
    while(Serial1.available()){
      char tmp = Serial1.read();
      Serial.print(tmp);
    }
    delay(1);
  }

  delay(1000);
  Serial1.println("AT+CIPSERVER=1,80");
  delay(1000);
  Serial1.flush();  
}

int idx = 0;
int chlID = 0;
char buffer[1024];
void loop() {
  if(Serial1.available()){
    String rec = "";
    for(int i =0; i < 500; i ++){
      while(Serial1.available())
        rec+= (char)Serial1.read(); 
      delay(1);   
    }
    //Serial.println(rec);
    strncpy(buffer, rec.c_str(), sizeof(buffer));
    buffer[sizeof(buffer)-1] = 0; // make sure it is mull terminated
    Serial.println(buffer);

    for(idx = 0; idx <sizeof(buffer); idx++){
      if(buffer[idx] == '+')
        break;
    }

    if(idx > 1010){
      //Serial.println("ERROR, idx>1010d");
      Serial.println("Not an IPD");
      return;
    } 
    else {
      Serial.print("Index of + : ");
      Serial.println(idx);
    }

    if(buffer[idx+1] =='I' && buffer[idx+2] =='P' && buffer[idx+3] =='D'){
      // then we found an IPD, +4 is a comma, +5 is the number
      // I AM MAKING AN ASSUMPTION HERE: that there will never be > 10 connections
      chlID = buffer[idx+5] - 48;
      Serial.print("Connection ID: ");
      Serial.println(chlID);
    } 
    else {
      Serial.println("Not an IPD");
      return;
    }

    delay(1000);

    Serial1.print("AT+CIPSEND=");
    Serial1.print(chlID);
    Serial1.print(",");
    Serial1.println(17);
    delay(1000);
    Serial1.println("HTTP/1.1 200 OK"); //17
    //    Serial1.println("Content-Type: text/html"); //25
    //    Serial1.println();
    //    Serial1.println("<!DOCTYPE HTML>"); //17
    //    Serial1.println("<html>"); //8
    //    Serial1.println("<text>"); //8
    //    Serial1.println("Success"); //9
    //    Serial1.println("</text>"); //9
    //    Serial1.println("</html>"); //9
    delay(1000);
    Serial1.print("AT+CIPCLOSE=");
    Serial1.println(chlID);

    int idx_slash = -1;
    int idx_H = -1;
    for(int i = idx; i <sizeof(buffer); i++){
      if(buffer[i] == 'G' && buffer[i+1] == 'E' && buffer[i+2] == 'T'){
        idx_slash = i + 4; 
      }
      if(buffer[i] == 'H' && buffer[i+1] == 'T' && buffer[i+2] == 'T' && buffer[i+3] == 'P'){
        idx_H = i;
      }
    }

    if(idx_slash != -1 && idx_H != -1){
      int len = idx_H - idx_slash;
      char com[len];
      for(int i = 0 ; i < len; i++){
        com[i] = buffer[i+idx_slash];
      }
      Serial.print("Command: ");
      Serial.println(com);
      
      if(com[1] == '1' && com[3] == '1')
      digitalWrite(13, HIGH);
      if(com[1] == '1' && com[3] == '0')
      digitalWrite(13, LOW);
    }

    // end of if available
  } // ====================================================================

  // OKAY, Listen up <=========================================
  // Here is where I am. It kindof works.
  // the data received from the module is a little weird. 
  // First off, sometimes
  // it echos back what you sent and other times it doesn't get all the data you
  // would expect. I suspect the ReceiveMessage code below is to blame for some of this.

  // Second, there are weird requests for favicon.ico. After some searching I learned this is
  // the icon that is next to the url in some browsers. I guess chrome was trying to get the icon.

  // Third, I am not sending the resposne correctly(?), but I am able to terminate the connection
  // telling chrome that there is no more data. It would be nice to at least return a page that
  // says success or error.

  // Finally, I need to both clean up the ReceiveMessage code and start to clean this up to make
  // some proper libraries scavenged from the best of what I've seen. There has been progress, but
  // there remains much work to do.


  //  // to parse data first extract connection number from
  //  // +IPD,0,341:GET /hi HTTP/1.1 (+IPD,<connection>,<length>:Data)
  //  // then get the data, esentially upto the HTTP is what we need
  //  while(Serial1.available()){
  //    //  - load each line one at a time until +IPD is found
  //    //    - can probably use parseInt to get the connection number once we have +IPD or maths
  //    //    - then we can look for the GET /then extract the commands using a manual parser
  //    //    - I should write a parser function since I will be doing this a lot
  //    
  //    //  - once found, keep that line and flush until "Link" has been found (I don't think we need
  //    // anything else from the line)
  //    //  - parse out the connection and "command" from the line
  //    //  - respond to client based on command (start with just a success)
  //    //  - then pull out data such as onboard LED brightness, then expand from there
  //    buffer[idx] = Serial1.read();
  //    if(buffer[idx] == '\n'){
  //      //newline, do your thing
  //      idx = 0;
  //      if(buffer[0] == '+' && buffer[1] == 'I' && buffer[2] == 'P' && buffer[3] == 'D'){
  //        Serial.println(buffer);
  //        Serial1.flush();
  //      }
  //      //if(buffer[0] == 'L' && buffer[1] == 'i' && buffer[2] == 'n' && buffer[3] == 'k'){
  //      //  Serial.println(buffer);
  //      //}
  //    } else{
  //      if(idx != 63) // prevent overflows, this is just ductape while I work on a good solution
  //        // perhaps if I grab the length value I can create a buffer big enough if I need data in it.
  //        idx++;
  //    }
  //
  //    //Serial.print(tmp);
  //  }

  //  int s = ReceiveMessage(buffer);
  //  if(s){
  //    Serial.println(s);
  //    Serial.println(buffer);
  ////    Serial1.print("AT+CIPSEND=");
  ////    Serial1.print(chlID);
  ////    Serial1.print(",");
  ////    Serial1.println(102);
  ////    delay(1000);
  ////      Serial1.print("HTTP/1.1 200 OK\r\n"); //17
  ////      Serial1.print("Content-Type: text/html\r\n"); //25
  ////      Serial1.print("<!DOCTYPE HTML>\r\n"); //17
  ////      Serial1.print("<html>\r\n"); //8
  ////      Serial1.print("<text>\r\n"); //8
  ////      Serial1.print("Success\r\n"); //9
  ////      Serial1.print("</text>\r\n"); //9
  ////      Serial1.print("</html>\r\n"); //9
  ////      delay(1000);
  //      Serial1.print("AT+CIPCLOSE=");
  //      Serial1.println(chlID);
  //      delay(5000);
  //      Serial1.flush();
  //      for(int i =0; i<512; i++)
  //        buffer[i] = 0;
  //        
  //  }

}

boolean connectWiFi(int retries) {
  for(int i=0;i<retries;i++){
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
      Serial.print(i);
      Serial.print(": ");
      Serial.println("Can not connect to the WiFi.");
    }
  }
  return false;

}

boolean resetModule(){
  Serial1.println("AT+RST"); // restet and test if module is redy
  char rdy[5] = {
    'r', 'e', 'a', 'd', 'y'                        };
  char chk[5] = {
    0,0,0,0,0                        };
  boolean moduleReady = false; 

  // look for 2 seconds for cue (plus a little...)
  for(int i = 0; i < 2000; i ++){
    while(Serial1.available()){
      chk[0] = chk[1];
      chk[1] = chk[2];
      chk[2] = chk[3];
      chk[3] = chk[4];
      chk[4] = Serial1.read();
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
  return moduleReady;  
}

// ITEAD's Receive Message Function.
int ReceiveMessage(char *buf)
{
  //+IPD,<len>:<data>
  //+IPD,<id>,<len>:<data>
  String data = "";
  if (Serial1.available()>0)
  {

    unsigned long start;
    start = millis();
    char c0 = Serial1.read();
    if (c0 == '+')
    {

      while (millis()-start<5000) 
      {
        if (Serial1.available()>0)
        {
          char c = Serial1.read();
          data += c;
        }
        if (data.indexOf("\nOK")!=-1)
        {
          break;
        }
      }
      //Serial.println(data);
      int sLen = strlen(data.c_str());
      int i,j;
      for (i = 0; i <= sLen; i++)
      {
        if (data[i] == ':')
        {
          break;
        }

      }
      boolean found = false;
      for (j = 4; j <= i; j++)
      {
        if (data[j] == ',')
        {
          found = true;
          break;
        }

      }
      int iSize;
      //DBG(data);
      //DBG("\r\n");
      if(found ==true)
      {
        String _id = data.substring(4, j);
        chlID = _id.toInt();
        String _size = data.substring(j+1, i);
        iSize = _size.toInt();
        //DBG(_size);
        String str = data.substring(i+1, i+1+iSize);
        strcpy(buf, str.c_str());	
        //DBG(str);

      }
      else
      {			
        String _size = data.substring(4, i);
        iSize = _size.toInt();
        //DBG(iSize);
        //DBG("\r\n");
        String str = data.substring(i+1, i+1+iSize);
        strcpy(buf, str.c_str());
        //DBG(str);
      }
      return iSize;
    }
  }

  return 0;
}

// Example output:
//
// Begin
// WiFi - Module is ready
// AT+CWJAP="________","_________"
// 0: Can not connect to the WiFi.
// AT+CWJAP="________","_________"
// OK, Connected to WiFi.
//
// AT+CIPMUX=1
//
// OK
// AT+CIFSR
//
// 192.168.254.4
//
// OK
// AT+CIPSERVER=1,80
// 
// OK
// Link
//
// +IPD,0,341:GET /hi HTTP/1.1
// Host: 192.168.254.4
// Connection: keep-alive
// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8
// User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/38.0.2125.111 Safari/537.36
// Accept-Encoding: gzip,deflate,sdch
// Accept-Language: en-US,en;q=0.8
//
// OK
// Link
// Unlink







