#include <SoftwareSerial.h>
#include <DFRobot_SIM808.h>

// char RECEIVING_PHONE[12] = "09683879596";
char RECEIVING_PHONE[12];
char violation[15];
char plate_number[10];
char email1[30];
char email2[30];

// --------------------------------------- SIM808
class Sim808 {
  private:
    SoftwareSerial ss;
    DFRobot_SIM808 sim808;
    static const int gps_interval = 4000;


    // Sends commands to SIM808
    void ss_cmd(char *cmd, int cmd_delay = 500) {
      ss.println(cmd);
      delay(cmd_delay);

      while(ss.available()!=0) {
        // ss.read();
        Serial.write(ss.read());
      }
    }

    // For looping in GPS (MUST NOT DO ANYTHING IN BETWEEN)
    void loopDontDoAnythingInBetween() {
      ss.listen();
      if (sim808.getGPS()) {
        this->latitude = sim808.GPSdata.lat;
        this->longitude = sim808.GPSdata.lon;
      }
    }

    

    // SMS SETUP
    void sms_setup() {
      while(!sim808.init()) {
        Serial.print("Sim808 init error\r\n");
        delay(800);
      }
    }

    // NET SETUP
    void net_setup() {
      ss_cmd("AT"); // See if the SIM900 is ready
      ss_cmd("AT+CPIN?"); // SIM card inserted and unlocked?
      ss_cmd("AT+CREG?"); // Is the SIM card registered?
      ss_cmd("AT+CGATT?"); // Is GPRS attached?
      ss_cmd("AT+CSQ "); // Check signal strength
      ss_cmd("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", 1000); // Set connection type to GPRS
      ss_cmd("AT+SAPBR=3,1,\"APN\",\"internet\"", 1000); // Set the APN
      ss_cmd("AT+SAPBR=0,1", 2000); // Enable GPRS
      ss_cmd("AT+SAPBR=1,1", 2000); // Enable GPRS
      ss_cmd("AT+SAPBR=2,1", 1000); // Check to see if connection is correct and get your IP address
    }

    // GPS SETUP
    void gps_setup() {
      this->gps_attach();
    }

    

  public:
    float latitude;
    float longitude;

    Sim808(byte txPin, byte rxPin) : ss(txPin, rxPin), sim808(&ss) {}

    void listen() {
      ss.listen();
    }

    void setup(bool withSMS, bool withGPS, bool withNet) {
      ss.listen();
      ss.begin(9600);
      ss.flush();

      delay(4000);

      // SMS SETUP
      if (withSMS) {
        Serial.println("sms...");
        this->sms_setup();
      }

      // GPS SETUP
      if (withGPS) {
        Serial.println("gps...");
        this->gps_setup();
      }

      // // NET SETUP
      // if (withNet) {
      //   Serial.println("net...");
      //   this->net_setup();
      // }
    }

    // GET GPS DATA IN LOOP
    bool gps_loop() {
      static unsigned long startGPS = millis();
      while (millis() - startGPS < gps_interval) {
        loopDontDoAnythingInBetween();
        return true;
      }
      startGPS = millis();
      gps_attach();
      return false;
    }


    // NET HTTP GET REQUEST
    String http_get(char *url) {
      ss.listen();
      net_setup();
      // String to_send = "AT+HTTPPARA=\"URL\",\"" + url + "\"";
      ss_cmd("AT+HTTPINIT", 1000); // initialize http service
      ss_cmd("AT+HTTPPARA=\"CID\",1", 1000);
      ss.print("AT+HTTPPARA=\"URL\",\"");
      Serial.println(url);
      ss.print(url);
      ss.println("\"");
      delay(5000);
      ss_cmd("AT+HTTPACTION=0", 10000); // set http action type 0 = GET, 1 = POST, 2 = HEAD
      // ss_cmd("AT+HTTPREAD", 1000); // read server response

      ss.println("AT+HTTPREAD");
      delay(5000);

      // String response = "";
      // while(ss.available()!=0) {
      //   String _response = ss.readStringUntil('\n');

      //   if (_response.length() == 27 && _response.charAt(0) == '-') {
      //     response = _response;
      //   }
      // }

      ss_cmd("AT+HTTPTERM", 300);
      ss_cmd("", 1000);
      return String("DONE");
    }

    // SEND SMS
    void sms_send(char *phone, char *text) {
      ss.listen();
      sim808.sendSMS(phone, text);
    }

    // Attaches the GPS
    void gps_attach() {
      bool isGPSAttached = sim808.attachGPS();
      // if (isGPSAttached) {
      //   Serial.println("GPS-Success");
      // } else {
      //   Serial.println("GPS-Fail");
      // }
    }
};

// --------------------------------------- SET INTERVAL
class SetInterval {
  private:
    unsigned long interval;   // Interval time in milliseconds
    unsigned long lastMillis; // Last time the function was called
    void (*callback)();       // Function pointer to be called

  public:
    SetInterval() {}

    void setup(unsigned long interval, void (*callback)()) {
      this->interval = interval;
      this->callback = callback;
      this->lastMillis = millis();
    }

    void loop() {
      unsigned long currentMillis = millis();
      if (currentMillis - this->lastMillis >= this->interval) {
        this->callback();
        this->lastMillis = currentMillis;
      }
    }
};



// CLASSES
Sim808 sim808(8, 9);


char SMS_MESSAGE[70];
void send_sms_plate_number() {
  sprintf(SMS_MESSAGE, "Your vehicle '%s' has received a violation.", plate_number);
  Serial.println(SMS_MESSAGE);
  sim808.sms_send(RECEIVING_PHONE, SMS_MESSAGE);
}

void send_sms_violation() {
  sprintf(SMS_MESSAGE, "Type of violation: %s", violation);
  Serial.println(SMS_MESSAGE);
  sim808.sms_send(RECEIVING_PHONE, SMS_MESSAGE);
}

char violation_email[4] = "000";

void http_get() {
  char URL[150];
  Serial.println("Sending HTTP request...");
  // sprintf(URL, "http://email-nestjs-server.fly.dev/?p=UOO957&v=010&e=timkristianllanto.tk@gmail.com");
  sprintf(URL, "http://email-nestjs-server.fly.dev/?p=%s&v=%s&e=%s%s",
   plate_number, violation_email, email1, email2);
  //  email, email, email);

  String response = sim808.http_get(URL);
  Serial.println(response);
  Serial.println("Finished HTTP Request!");
} 


void setup() {
  Serial.begin(9600);
  Serial.println("\n>>> Setting up Sim808...");
  sim808.setup(true, false, true);
  Serial.println("Finished setting up Sim808!!!");
  // send_sms();
  // Serial.println("Sent sms");
  // http_get();

}


void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');

    // PLATE NUMBER
    if (command.startsWith("1-")) {
      sprintf(plate_number, command.substring(2).c_str());

    // SEND PLATE NUMBER SMS
    } else if (command.startsWith("5-")) {
      sprintf(violation_email, "000");
      send_sms_plate_number();

    // VIOLATION
    } else if (command.startsWith("2-")) {
      sprintf(violation, command.substring(2).c_str());
      send_sms_violation();

      if (command.equals("2-Obstruction")) {
        violation_email[0] = '1';
      } else if (command.equals("2-Unregistered")) {
        violation_email[1] = '1';
      } else if (command.equals("2-Coding")) {
        violation_email[2] = '1';
      }

    // EMAIL
    } else if (command.startsWith("31-")) {
      sprintf(email1, command.substring(3).c_str());

    } else if (command.startsWith("32-")) {
      sprintf(email2, command.substring(3).c_str());
      http_get();

    // PHONE
    } else if (command.startsWith("4-")) {
      sprintf(RECEIVING_PHONE, command.substring(2).c_str());
    }
  }
  Serial.print(violation);
  Serial.print("\t");
  Serial.print(plate_number);
  Serial.print("\t");
  Serial.print(email1);
  Serial.print(email2);
  Serial.print("\t");
  Serial.println(RECEIVING_PHONE);

}
