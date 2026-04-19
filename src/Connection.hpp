#pragma once

#include <Arduino.h>
#include <nRF24L01.h>
#include <RF24.h>

static constexpr uint32_t CONNECTION_TIMEOUT_MS = 5000; //Timeout time for the connection if no packet is recieved in milliseconds. Can be adjusted.

// Connection tracks RF link configuration and status
class Connection {
public:
    Connection(): Channel(0), MyID(0), RemoteID(0), lastReceivedTime(0), lastSentTime(0), connected(false){
        memset(Address, 0, sizeof(Address));
        spi = &SPI;
    }

    // Configuration setters (call before init of the RFManagers)
    void setAddress(const uint8_t* addr) {
        memcpy(Address, addr, 5);
    }
    void setChannel(uint8_t channel)        { Channel  = channel; }
    void setMyID(uint8_t id)                { MyID     = id; }
    void setRemoteID(uint8_t id)            { RemoteID = id; }
    void setPALevel(rf24_pa_dbm_e level)    { PALevel = level; }
    void setDataRate(rf24_datarate_e rate)  { Datarate = rate;}
    void setSPI(SPIClass* s)                { spi = s;}
    
    // getters
    void    getAddress(uint8_t* addr) const { memcpy(addr, Address, 5); }
    uint8_t getChannel()            const { return Channel; }
    uint8_t getMyID()               const { return MyID; }
    uint8_t getRemoteID()           const { return RemoteID; }
    rf24_datarate_e getDataRate()   const { return Datarate; }
    rf24_pa_dbm_e getPALevel()      const { return PALevel; }
    SPIClass* getSPI()              const { return spi;}

    void updateReceived() {
        lastReceivedTime = millis();
        connected = true;
    } // Call when a valid packet is successfully received

    
    void updateSent() {
        lastSentTime = millis();
    } // Call after each successful write (optional for TX rate tracking)

    
    bool isConnected() const {
        if (!connected) return false;
        return (millis() - lastReceivedTime) < CONNECTION_TIMEOUT_MS;
    } // Returns true if a packet has been received within CONNECTION_TIMEOUT_MS

    
    uint32_t msSinceLastReceived() const {
        if (!connected) return 0;
        return millis() - lastReceivedTime;
    } // Milliseconds since the last received packet (0 if none ever received)

    
    uint32_t msSinceLastSent() const {
        return millis() - lastSentTime;
    } // Milliseconds since the last sent packet (0 if none ever sent)

private:
    uint8_t  Address[5];
    uint8_t  Channel;
    uint8_t  MyID;
    uint8_t  RemoteID;
    rf24_pa_dbm_e PALevel;
    rf24_datarate_e Datarate;
    SPIClass* spi;
    uint32_t lastReceivedTime;
    uint32_t lastSentTime;
    bool     connected; // stays false until the first packet is received
};