// ESP32 used as master.
// Make sure to include https://github.com/nRF24/RF24.git

#include <Arduino.h>
#include "Packet.hpp"
#include "Connection.hpp"
#include <nRF24L01.h>
#include <RF24.h>
#include <SPI.h>
#include <RFMaster.hpp>

#define CE_PIN  4
#define CSN_PIN 5

RF24 radio(CE_PIN, CSN_PIN);
RFMaster master(radio);



void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }
    Connection conn;
    uint8_t addr[5] = { 'R', 'F', 'T', 'E', 'S' };
    conn.setAddress(addr);
    conn.setChannel(76);
    conn.setDataRate(RF24_250KBPS);
    conn.setPALevel(RF24_PA_MAX);
    master.init(1, conn);

    // Example: send a command packet every second
    CommandPacket* cmd = new CommandPacket(1, 128, 128, 128, 128);
    master.sendPacket(cmd, true); // RFMaster takes ownership and will delete cmd
}

void loop() {
    Serial.println("Sending command packet...");

    master.sendPacket(new CommandPacket(1, 128, 128, 128, 128), true);
    master.update();

    Packet* incoming = master.takePacket();
    if (incoming) {
        Serial.println("Received ACK payload:");
        incoming->serialOut();
        delete incoming;
    }

    delay(1000);
}