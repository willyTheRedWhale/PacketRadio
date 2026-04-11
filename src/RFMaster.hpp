#pragma once

#include <Arduino.h>
#include "Packet.hpp"
#include "Connection.hpp"
#include <nRF24L01.h>
#include <RF24.h>
#include <SPI.h>

// RFMaster sends packets to the Slave, recieves Ack Payloads as a response.

class RFMaster {
public:
    explicit RFMaster(RF24& radio): radio(radio), outgoingPacket(nullptr), incomingPacket(nullptr), ownOutgoing(false) {}

    ~RFMaster() {
        if (incomingPacket) delete incomingPacket;
        if (outgoingPacket && ownOutgoing) delete outgoingPacket;
    }

    // Configure the radio
    void init(uint8_t myID, Connection initialConnection) {
        connectionDetails = initialConnection;
        connectionDetails.setMyID(myID);
        radio.begin();
        radio.setDataRate(initialConnection.getDataRate()); 
        radio.setPALevel(initialConnection.getPALevel()); 
        radio.enableAckPayload();
        radio.setRetries(5, 5);
        radio.enableDynamicPayloads();
        radio.enableDynamicAck();
        radio.setChannel(connectionDetails.getChannel());

        uint8_t addr[5];
        connectionDetails.getAddress(addr);
        radio.openWritingPipe(addr);
        radio.openReadingPipe(1, addr); // Required for ACK payloads on pipe 1
        radio.startListening();
    }

    // Queue a packet for transmission on the next update(). Older packets will be overwritten.
    // if takeOwnership is true, the master will delete the packet after sending.
    void sendPacket(Packet* packet, bool takeOwnership = false) {
        if (outgoingPacket && ownOutgoing) delete outgoingPacket;
        outgoingPacket = packet;
        ownOutgoing    = takeOwnership;
    }

    // Drive the radio: transmit queued packet and read recieved ACK payload.
    // Call once per loop iteration, or on the interval you want to send packets at, it should be called regularly. 
    void update() {
        bool ok = false;
        if (outgoingPacket) {
            uint8_t buffer[32];
            uint8_t size = outgoingPacket->toBuffer(buffer, sizeof(buffer));
            if (size > 0) {
                radio.stopListening();
                ok = radio.write(buffer, size); // Returns true if the transmission was successful and an ACK was received. 
                radio.startListening();
                if (ok) connectionDetails.updateSent();
            }

            if (ownOutgoing) delete outgoingPacket;
            outgoingPacket = nullptr;
            ownOutgoing    = false;
        } else {
            // If no packet is queued, send a confirmation packet with ID 0 to keep the connection alive and let the slave know we are still here. 
            ConfirmationPacket keepAlivePacket(0);
            uint8_t buffer[32];
            uint8_t size = keepAlivePacket.toBuffer(buffer, sizeof(buffer));
            if (size > 0) {
                radio.stopListening();
                radio.write(buffer, size);
                radio.startListening();
            }
        }

        // Read ACK payload sent back by the slave
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
                    }
                    if (incomingPacket || ok) {
                        // in case ok is true, but the slave didn't send a valid packet back, we still want to update the connection status by calling updateReceived() so that isConnected() returns true.

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
    bool     isConnected()           const { return connectionDetails.isConnected(); }
    uint32_t msSinceLastReceived()   const { return connectionDetails.msSinceLastReceived(); }

private:
    RF24&      radio;
    Connection connectionDetails;

    Packet* outgoingPacket; // Pointer to the Packet that will be sent once update is called. 
    Packet* incomingPacket; // The recent recieved Packet from the Slave using Ack Payloads. 
    bool    ownOutgoing;    // Delete outgoing Packet after sending
};