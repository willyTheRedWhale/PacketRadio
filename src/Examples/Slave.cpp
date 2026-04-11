//Teensy 4.1 used as slave
// Make sure to include https://github.com/nRF24/RF24.git


#include <Arduino.h>
#include "Packet.hpp"
#include "Connection.hpp"
#include <nRF24L01.h>
#include <RF24.h>
#include <SPI.h>
#include <RFSlave.hpp>

#define CE_PIN  9
#define CSN_PIN 10
RF24 radio(CE_PIN, CSN_PIN); // CE, CSN pins
RFSlave slave(radio);


void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    //while (!Serial) { delay(10); }
    Serial.println("Initializing RFSlave...");
    
    
    Connection conn;
    uint8_t addr[5] = { 'R', 'F', 'T', 'E', 'S' };
    conn.setAddress(addr);
    conn.setChannel(76);
    conn.setDataRate(RF24_250KBPS);
    conn.setPALevel(RF24_PA_MAX);
    slave.init(2, conn);

    // Example: preload an ACK payload before the master sends anything
    ConfirmationPacket confirm(1); // ID 1 matches the CommandPacket's MyID
    slave.queueAckPacket(&confirm); 
    delete &confirm; 
    Serial.print(radio.isChipConnected() ? "RF24 detected and connected!" : "RF24 not detected or not connected!");
    radio.printPrettyDetails();
}

void loop() {
    slave.update();
    if (slave.hasNewPacket()) {
        Packet* pkt = slave.takePacket();
        if (pkt) {
        Serial.println("Received packet:");
        pkt->serialOut();

        // Example: reply with a telemetry packet
        TelemetryPacket reply(37.7749, -122.4194, 15000, 14950);
        slave.queueAckPacket(&reply);
        delete &reply; // Clean up after processing

        delete pkt; // Clean up after processing
        }
    }
}