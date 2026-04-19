#pragma once

#include <Arduino.h>
#include "Packet.hpp"
#include "Connection.hpp"
#include <nRF24L01.h>
#include <RF24.h>
#include <SPI.h>

// RFSlave: listens for master packets and responds via ACK payloads.
class RFSlave {
public:
    explicit RFSlave(RF24& radio)
        : radio(radio), incomingPacket(nullptr)
    {}

    ~RFSlave() {
        if (incomingPacket) delete incomingPacket;
    }

    // Configure the radio using connection details shared with the master.
    void init(uint8_t myID, Connection initialConnection) {
        connectionDetails = initialConnection;
        connectionDetails.setMyID(myID);
        connectionDetails.getSPI()->begin();
        
        radio.begin(connectionDetails.getSPI());
        radio.setDataRate(initialConnection.getDataRate()); 
        radio.setPALevel(initialConnection.getPALevel()); 
        radio.enableAckPayload();
        radio.setRetries(5, 5);
        radio.enableDynamicPayloads();
        radio.enableDynamicAck();
        radio.setChannel(connectionDetails.getChannel());

        uint8_t addr[5];
        connectionDetails.getAddress(addr);
        radio.openReadingPipe(1, addr);
        radio.startListening();
    }

    /* 
    Preload the ACK payload to be sent when the master's next packet arrives. 
    Call this after consuming an incoming packet (inside your loop, after takePacket()) to prepare the reply for the master's next transmission. 
    If you don't call this, the next ACK payload will be empty.
    */
    void queueAckPacket(const Packet* packet) {
        if (!packet) return;
        uint8_t buffer[32];
        uint8_t size = packet->toBuffer(buffer, sizeof(buffer));
        if (size > 0) {
            radio.writeAckPayload(1, buffer, size);
        }
    } 

    // Drive the radio: read any incoming Packet. Call once per loop iteration, or on the interval you want to receive packets at, it should be called regularly.
    void update() {
        if (radio.available()) {
            uint8_t buffer[32];
            uint8_t size = radio.getDynamicPayloadSize();
            if (size > 0 && size <= sizeof(buffer)) {
                radio.read(buffer, size);
                if (incomingPacket) delete incomingPacket;
                incomingPacket = Packet::create(buffer[0]);
                if (incomingPacket) {
                    if (!incomingPacket->fromBuffer(buffer, size)) {
                        delete incomingPacket;
                        incomingPacket = nullptr;
                    } else {
                        connectionDetails.updateReceived();
                    }
                }
            }
        }
    }

    // Incoming

    bool hasNewPacket() const { return incomingPacket != nullptr; } // checks if incomingPacket is a valid Packet or a nullptr

    Packet* takePacket() {
        Packet* p      = incomingPacket;
        incomingPacket = nullptr;
        return p;
    } // Get the Incoming Packet, nullptr if no packet is waiting. Caller takes the ownership and deletes the pointer when done.

    // Connection Status
    bool     isConnected()         const { return connectionDetails.isConnected(); }
    uint32_t msSinceLastReceived() const { return connectionDetails.msSinceLastReceived(); }

private:
    RF24&      radio;
    Connection connectionDetails;

    Packet* incomingPacket; // most recent received packet is pointed to by this pointer. Caller takes ownership by calling takePacket() and must delete it.
};