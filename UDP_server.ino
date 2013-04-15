#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008


// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
//byte mac[] = {    0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte mac[] = {  0x2A, 0x00, 0x00, 0x20, 0x08, 0x54 }; // azriel

//IPAddress ip(192, 168, 1, 177);
IPAddress ip(158,130,37,88); // for detkin
//IPAddress ip(10,0,0,6);

unsigned int localPort = 8888;      // local port to listen on

//unsigned int remotPort = 9930;  // server port
//unsigned int remotPort = 9940; //linux server port
//unsigned int remotPort = 36135;  // server port detkin
 
int ss_ethernet = 10;
int ss_sdcard = 4;

// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
//char  ReplyBuffer[] = "acknowledged";       // a string to send back
char  ReplyBuffer[] = "seethu";
// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup() {
  // handle sd card de-select
  pinMode(ss_sdcard,OUTPUT);
  digitalWrite(ss_sdcard,HIGH);
  Serial.begin(9600);
  // start the Ethernet and UDP:   
  Serial.println("server");
  Serial.println("Attempting to get an IP address using DHCP:");
  if (Ethernet.begin(mac) == 0) {
    // if DHCP fails, start with a hard-coded address:
    Serial.println("failed to get an IP address using DHCP, trying manually");
    Ethernet.begin(mac, ip);
  }
  Serial.print("My address:");
  Serial.println(Ethernet.localIP());
  delay(1000);
  Udp.begin(localPort);
}

void loop() {
  
  recv();
}

void recv()
{
  // if there's data available, read a packet
  //Serial.println("Waiting...");
  int packetSize = Udp.parsePacket();
  //Serial.println("Done.");
  if(packetSize)
  {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remote = Udp.remoteIP(); 
    for (int i =0; i < 4; i++)
    {
      Serial.print(remote[i], DEC);
      if (i < 3)
      {
        Serial.print(".");
      }
    }
    Serial.print(", port ");
    Serial.println(Udp.remotePort());
    // read the packet into packetBufffer
    Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);
    Serial.print("Received: ");
    Serial.println(packetBuffer);
    snd();
  }
  delay(100);
}

void snd()
{
   // send a reply, to the IP address and port that sent us the packet we received
    Serial.print("Sending: ");
    Serial.println(ReplyBuffer);
    Serial.print("\n");
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(ReplyBuffer);
    Udp.endPacket(); 
}
