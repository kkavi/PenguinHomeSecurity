// NOTE: if user name is larger than max size, will not be able to find (early NULL terminator)

// Include libararies
#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h> // UDP library from: bjoern@cs.stanford.edu 12/30/2008
#include <SoftwareSerial.h>

#define PIR_EVENT 'M'
#define CAM_EVENT 'C'
#define LIGHT_EVENT 'I'
#define LOCK_EVENT 'L'
#define RFID_EVENT 'R'
#define HELLO_EVENT 'H'

#define NOTIFY 'n'
#define UPDATE 'u'
#define QUERY 'q'
#define REQUEST 'r'

#define START_BYTE '~'
#define END_BYTE 4  // i.e. number 0x04, EOT

#define NUM_USERS 10
#define USER_NAME_LENGTH 25

#define UDP_TX_PACKET_MAX_SIZE 24

/**********************************************************************************************************/
// CAMERA SETUP:

    // SD card chip select line varies among boards/shields:
    #define chipSelect 4
    
    // Here we select the pins for camera TX, RX:
      // We will use: camera TX connected to pin 8, camera RX to pin 9:
    //  SoftwareSerial cameraconnection = SoftwareSerial(8, 9);
      
   // Adafruit_VC0706 cam = Adafruit_VC0706(&cameraconnection);

/**********************************************************************************************************/
// GLOBAL VARIABLE DECLARATIONS/INITIALIZATIONS

  // XBee (hardware serial port -> pins 0 and 1, i.e. Tx,Rx)
  byte xbeeByte;
  SoftwareSerial XBeeSerial(6, 7); // 6 = rx (DOUT), 7 = tx (DIN)
  char prev_RFID[5];  
  
  char msg_header[4] = "000";
  char nextChar[2] = "0";
  byte nextByte;
  
  boolean XBee_to_tx = false;
  boolean Ethernet_to_tx = false;
  
//  String inputXBee;
//  String outputXBee;
//  byte inputByte;
  int i;
  
  //String packetBuffer;
  

/**********************************************************************************************************/
// ETHERNET SETUP:

  // Enter a MAC address and IP address for your controller below.
  // The IP address will be dependent on your local network:
  byte my_mac[] = {  0x2A, 0x00, 0x00, 0x20, 0x08, 0x54 }; // from azriel
  
  IPAddress my_ip(158,130,37,88); // IP addr for detkin (when using above MAC addr)
  IPAddress all_IPs[NUM_USERS]; //remote(130,79,192,146); //linux server
  
  unsigned int localPort = 8888;      // local port to listen on
  unsigned int all_ports[NUM_USERS]; //remotPort = 9930;      // server port (to send to)
  //unsigned int remotPort = 36135;  // server port detkin
   
  char* users[NUM_USERS];
  char user_name[USER_NAME_LENGTH];
  int num_users;
  //uint32_t all_IPs[NUM_USERS]; //remote(130,79,192,146); //linux server
  //all_IPs[0] = (130,79,192,146);
  
  int ss_ethernet = 10;
  int ss_sdcard = 4;
  
  // buffers for receiving and sending data
  char recvBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
  char sendBuffer[UDP_TX_PACKET_MAX_SIZE];       // a string to send back
  
  // An EthernetUDP instance to let us send and receive packets over UDP
  EthernetUDP Udp;

/**********************************************************************************************************/
// SETUP LOOP
void setup() {
  
  // SET UP ETHERNET CONNECTION ///////////////////////////////////////
  
  // setup users/IPs/ports
  num_users = 0;  // set total number of users to 0 
  for (i=0; i<NUM_USERS; i++) {
    all_IPs[i] = (0,0,0,0);
    all_ports[i] = 0;
    users[i] = 0;
  }
  // test, debugging
  all_IPs[0] = (130,79,192,146);
  all_ports[0] = 9930;
  users[0] = ("test"); 
  
  // handle sd card de-select
  pinMode(ss_sdcard,OUTPUT);
  digitalWrite(ss_sdcard,HIGH);
  
  //pinMode(ss_ethernet,OUTPUT);
  //digitalWrite(ss_sdcard,LOW);
  
  // serial connection to MAIN2/AUX arduino
  Serial.begin(9600);
  
  // start the Ethernet and UDP:
 // /* 
  Serial.println("Attempting to get an IP address using DHCP:");
  if (Ethernet.begin(my_mac) == 0) {
    // if DHCP fails, start with a hard-coded address:
    Serial.println("failed to get an IP address using DHCP, trying manually");
    Ethernet.begin(my_mac, my_ip);
  }
  Serial.print("My address:");
  Serial.println(Ethernet.localIP());
  
  Udp.begin(localPort);
  
  //Serial.print("My address:");
  //Serial.println(Ethernet.localIP());
  
  //packetBuffer.reserve(UDP_TX_PACKET_MAX_SIZE);
// */
  
  
  // SET UP XBEE ///////////////////////////////////////
//    inputXBee.reserve(64);
//    outputXBee.reserve(64);
    XBeeSerial.begin(9600);
    
}

/**********************************************************************************************************/
// MAIN LOOP
void loop() {
  
  // (1) check to see if data incoming on Ethernet
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    // read in packet
    Serial.println("MSG Recv'd");
    recv_UDP();
    Serial.println(recvBuffer);
    // check validity of received message
    if (recvBuffer[0] == START_BYTE) {
      
      // HELLO packet
      if (recvBuffer[1] == HELLO_EVENT) {
        // send packet onward to MAIN2 to be recorded in the event log
        for (i=0; i<packetSize; i++) {
          Serial.write(recvBuffer[i]);
        }
        // read in packet, respond appropriately
        recv_hello_packet();
      }
      // RFID or LOCK event
      else if ( (recvBuffer[1] == RFID_EVENT) || (recvBuffer[1] == LOCK_EVENT) ) {
        // output packet to XBee (i.e. send to AUX)
        for (i=0; i<packetSize; i++) {
          XBeeSerial.write(recvBuffer[i]);
        }
        if (recvBuffer[2] == UPDATE) {
          // if message is an UPDATE, also send to MAIN2 (to be recorded in datalog)
          for (i=0; i<packetSize; i++) {
            Serial.write(recvBuffer[i]);
          }
        }
      }
      // All others... (go to MAIN2)
      else {
        // output to Serial (i.e. send to MAIN2)
        for (i=0; i<packetSize; i++) {
          Serial.write(recvBuffer[i]);
        }
      }
      
    }  // END if(recvBuffer[0] == START_BYTE)
    // empty buffer
    clear_recv_buffer(); 
  }

  // (2) check to see if data incoming on XBee
  byte buff_index;
  while (XBeeSerial.available()) {
    nextByte = XBeeSerial.read();
    //Serial.print("XBee heard!"); Serial.println(nextByte);  // debugging
    
    // if TX start bit found (BEGIN msg TX routine)
    if (nextByte == START_BYTE) {
      //Serial.println("XBee STARTs!");  // debugging
      // prepare "sendBuffer" to Tx across ethernet, or Serial
      clear_send_buffer();
      sendBuffer[0] = START_BYTE;
      
      byte numRead = XBeeSerial.readBytes(&sendBuffer[1],3);
      if (numRead < 3) {
        break;
      }
      
      buff_index = 4;
      // read in rest of message into "sendBuffer"
      while( (numRead = XBeeSerial.readBytes(nextChar,1)) && (nextChar[0] != END_BYTE) && (buff_index < (UDP_TX_PACKET_MAX_SIZE-1)) ) {
        // write nextByte to UDP pckt buffer
        sendBuffer[buff_index] = nextChar[0];
        buff_index++;
      }
      // finish message by adding END_BYTE to tail
      sendBuffer[buff_index] = END_BYTE;
      //Serial.println(buff_index);  // debugging
      
      // check packet destination
      if( (sendBuffer[1] == RFID_EVENT) && (sendBuffer[2] == NOTIFY) && (sendBuffer[3] == '1') ) {
        // if msg is a notification of an RFID card swipe, also send to MAIN2 (to be recorded in data log)
        for (i=0; i<buff_index; i++) {
          Serial.write(sendBuffer[i]);
        }
      }
      else if ( (sendBuffer[1] == LOCK_EVENT) && (sendBuffer[2] == NOTIFY) ) {
        // if msg is a notification of the state of the door being locked, also send to MAIN2 (to be recorded in data log)
        for (i=0; i<buff_index; i++) {
          Serial.write(sendBuffer[i]);
        }
      }
      // send message across Ethernet
      send_UDP();
      
    } // END msg TX routine
  } // END while XbeeSerial.available()
  
  // (3) done main loop, repeat from beginning
}

/**********************************************************************************************************/
// FUNCTIONS

void serialEvent()  // this function is called in between "loop()" runs if data is available (on Serial)
{
  byte nextByte; // = Serial.read();
  while(Serial.available()) {
    nextByte = Serial.read();
    
    // NOTE: traffic from the Serial port (i.e. from MAIN2) will ALWAYS go to the Ethernet connection
    
    // so, if TX start bit found (BEGIN msg TX routine)
    if (nextByte == START_BYTE) {
      // prepare "sendBuffer" to Tx across ethernet
      clear_send_buffer();
      sendBuffer[0] = START_BYTE;
      
      byte buff_index = 1;
      // read in rest of message into "sendBuffer"
      while( (Serial.readBytes(nextChar,1)) && (nextChar[0] != END_BYTE) && (buff_index < (UDP_TX_PACKET_MAX_SIZE-1)) ) {
        // write next msg char (i.e. byte) to UDP packet buffer
        sendBuffer[buff_index] = nextChar[0];
        buff_index++;
      }
      // finish message by adding END_BYTE to tail
      sendBuffer[buff_index] = END_BYTE;
      // send UDP packet
      send_UDP();
      //Serial.print(sendBuffer);  // debugging
      
    } // END msg TX routine
  } // END while Serial.available()
  
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
void recv_UDP()
{
  // if there's data available, read a packet
  //Serial.println("Waiting...");
  //int packetSize = Udp.parsePacket();
  //Serial.println("Done.");
  //if(packetSize)
  {
    // check incoming IP address, if "new", then
      // see if header matches "HELLO" header
      // check username, see if username is already in system
      // 
    
    // read the packet into packetBufffer
    Udp.read(recvBuffer,UDP_TX_PACKET_MAX_SIZE);
    
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
void send_UDP()
{
  // send UDP packet to all IP addresses (i.e. all Androids) being tracked
  for (i=0; i<NUM_USERS; i++) {
    if (users[i] != 0) {
      Udp.beginPacket(all_IPs[i], all_ports[i]);
      Udp.write(recvBuffer);
      Udp.endPacket();
    }
  }
  
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
void recv_hello_packet()
{
  // get IP address
  IPAddress user_IP = Udp.remoteIP();
  // get port number
  int user_port = Udp.remotePort();
  // read in user id
  if (recvBuffer[4] == '|') {
    for (i=0; (i<USER_NAME_LENGTH) && (recvBuffer[i+4] != END_BYTE); i++) {
      user_name[i] = recvBuffer[i+4];
    }
    user_name[i] = 0;  // add NULL terminating char
  }
  
  // see if user name already stored in database
  int index = find_user_index(user_name);
  
  // figure out whether to add/remove user (what is the msg action type)
  if (recvBuffer[3] == '1') {
    // add/update user in database
    if (index >= 0) {
      // if user already in database
      all_IPs[index] = user_IP;
      all_ports[index] = user_port;
    }
    else {
      // if user NOT already in database
      for (index = 0; index<NUM_USERS; index++) {
        if (users[index] == 0) {
          // index is empty
          break;
        }
      }
      if (index == NUM_USERS) {
        // error, database full, no room for more users
      }
      else {
        // put new user into next available index
        for (i=0; (i<USER_NAME_LENGTH-1) && (user_name[i] != 0); i++) {
          users[index][i] = user_name[i];
        }
        users[index][i] = 0;  // add NULL terminator
        all_IPs[index] = user_IP;
        all_ports[index] = user_port;
        // update num_users
        num_users++;
      }
      
    }
  }
  else if (recvBuffer[3] == '2') {
    // remove user from database
    if (index < 0) {
      // if user does not exist in database, need not do anything
    }
    else {
      // if user IS in database, need to remove
      users[index] = 0;
      all_IPs[index] = (0,0,0,0);
      all_ports[index] = 0;
      // update num_users
      num_users--;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
int find_user_index(char* my_name)
{
  int i;
  for (i=0; i<NUM_USERS; i++) {
    // check name, see if it matches
    if (compare_strings(user_name, users[i]) == 0) {
      return i;
    }
  }
  return -1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
void clear_recv_buffer()
{
  byte i;
  for (i=0; i<UDP_TX_PACKET_MAX_SIZE ; i++) {
    recvBuffer[i] = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
void clear_send_buffer()
{
  byte i;
  for (i=0; i<UDP_TX_PACKET_MAX_SIZE ; i++) {
    sendBuffer[i] = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// returns 0 if strings are the same, otherwise, number of characters that are different (up to and including first NULL char)
int compare_strings(char* s1, char* s2)
{
  int count = 0;
  int i = 0;
  while(1) {
    if (s1[i] != s2[i]) {
      count++;
    }
    if ( (s1[i] == 0) || (s2[i] == 0) ) {
      break;
    }
    i++;
  }
  return count;
}


