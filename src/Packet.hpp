#pragma once

#include <Arduino.h>
#include <stdint.h>

// Packet type identifiers first byte of every packet is used.
typedef enum : uint8_t {
    PKT_KEEP_ALIVE       = 0,
    PKT_COMMAND          = 1,
    PKT_TELEMETRY        = 2,
    PKT_GPS              = 3,
    PKT_BMP              = 4,
    PKT_IMU              = 5,
    PKT_KALMAN           = 6,
    PKT_MESSAGE          = 7,
    PKT_CLIENT_BROADCAST = 8,
    PKT_CONFIRMATION     = 9,
} PacketID;

// Base class, all packets must implement these three methods
class Packet {
public:
    virtual ~Packet() {}

    // Data to buffer. Returns number of bytes written, or 0 on failure.
    virtual uint8_t toBuffer(uint8_t* buffer, uint8_t maxSize) const = 0;

    // Data from buffer. Returns true on success.
    virtual bool fromBuffer(const uint8_t* buffer, uint8_t size) = 0;

    // Print Data to Serial for debugging
    virtual void serialOut() const = 0;

    uint8_t getHeader() const { return header; }

    // Factory: allocate the correct Packet subclass for a given packet ID.
    // Caller takes ownership and must delete the returned pointer.
    // Returns nullptr for unknown IDs. -> invalid Packet
    static Packet* create(uint8_t id);

protected:
    uint8_t header = 0;

    // Serialization helpers advance ptr as bytes are written / read
    static void writeFloat(uint8_t*& ptr, float value) {
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(&value);
        for (uint8_t i = 0; i < 4; i++) *ptr++ = raw[i];
    }
    static void writeInt16(uint8_t*& ptr, int16_t value) {
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(&value);
        for (uint8_t i = 0; i < 2; i++) *ptr++ = raw[i];
    }
    static void writeUint16(uint8_t*& ptr, uint16_t value) {
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(&value);
        for (uint8_t i = 0; i < 2; i++) *ptr++ = raw[i];
    }

    static void readFloat(const uint8_t*& ptr, float& out) {
        uint8_t* raw = reinterpret_cast<uint8_t*>(&out);
        for (uint8_t i = 0; i < 4; i++) raw[i] = *ptr++;
    }
    static void readInt16(const uint8_t*& ptr, int16_t& out) {
        uint8_t* raw = reinterpret_cast<uint8_t*>(&out);
        for (uint8_t i = 0; i < 2; i++) raw[i] = *ptr++;
    }
    static void readUint16(const uint8_t*& ptr, uint16_t& out) {
        uint8_t* raw = reinterpret_cast<uint8_t*>(&out);
        for (uint8_t i = 0; i < 2; i++) raw[i] = *ptr++;
    }

    // Simple additive checksum over len bytes (not currently written into
    // packets call explicitly in toBuffer/fromBuffer if you want it
    static uint8_t calcChecksum(const uint8_t* buf, uint8_t len) {
        uint8_t sum = 0;
        for (uint8_t i = 0; i < len; i++) sum += buf[i];
        return sum;
    }
};

// CommandPacket
//  6 bytes
class CommandPacket : public Packet {
public:
    CommandPacket() { header = PKT_COMMAND; }

    CommandPacket(uint8_t autopilot, uint8_t pitch, uint8_t roll, uint8_t yaw, uint8_t throttle) {
        header = PKT_COMMAND;
        fromInput(autopilot, pitch, roll, yaw, throttle);
    }

    void fromInput(uint8_t autopilot, uint8_t pitch, uint8_t roll, uint8_t yaw, uint8_t thr) {
        autopilot_enabled = autopilot;
        target_pitch      = pitch;
        target_roll       = roll;
        target_yaw        = yaw;
        throttle          = thr;
    }

    uint8_t toBuffer(uint8_t* buffer, uint8_t maxSize) const override {
        if (maxSize < 6) return 0;
        uint8_t* ptr = buffer;
        *ptr++ = header;
        *ptr++ = autopilot_enabled;
        *ptr++ = target_pitch;
        *ptr++ = target_roll;
        *ptr++ = target_yaw;
        *ptr++ = throttle;
        return ptr - buffer; // 6
    }

    bool fromBuffer(const uint8_t* buffer, uint8_t size) override {
        if (size < 6) return false;
        const uint8_t* ptr = buffer;
        header            = *ptr++;
        autopilot_enabled = *ptr++;
        target_pitch      = *ptr++;
        target_roll       = *ptr++;
        target_yaw        = *ptr++;
        throttle          = *ptr++;
        return true;
    }

    void serialOut() const override {
        Serial.print(">Packet_ID: ");   Serial.println(header);
        Serial.print(">Autopilot: ");   Serial.println(autopilot_enabled);
        Serial.print(">Pitch: ");       Serial.println(target_pitch);
        Serial.print(">Roll: ");        Serial.println(target_roll);
        Serial.print(">Yaw: ");         Serial.println(target_yaw);
        Serial.print(">Throttle: ");    Serial.println(throttle);
    } //For Telepot desgined output (VSCode extention)

    // Field accessors
    uint8_t getAutopilot() const { return autopilot_enabled; }
    uint8_t getPitch()     const { return target_pitch; }
    uint8_t getRoll()      const { return target_roll; }
    uint8_t getYaw()       const { return target_yaw; }
    uint8_t getThrottle()  const { return throttle; }

private:
    uint8_t autopilot_enabled = 0;
    uint8_t target_pitch      = 0;
    uint8_t target_roll       = 0;
    uint8_t target_yaw        = 0;
    uint8_t throttle          = 0;
};

// TelemetryPacket
// 17 bytes
class TelemetryPacket : public Packet {
public:
    TelemetryPacket() { header = PKT_TELEMETRY; }

    TelemetryPacket(float lat, float lon, float gps_alt, float baro_alt) {
        header = PKT_TELEMETRY;
        fromInput(lat, lon, gps_alt, baro_alt);
    }

    void fromInput(float _lat, float _lon, float _gps_alt_cm, float _baro_alt_cm) {
        lat         = _lat;
        lon         = _lon;
        gps_alt_cm  = _gps_alt_cm;
        baro_alt_cm = _baro_alt_cm;
    }

    uint8_t toBuffer(uint8_t* buffer, uint8_t maxSize) const override {
        if (maxSize < 17) return 0;
        uint8_t* ptr = buffer;
        *ptr++ = header;
        writeFloat(ptr, lat);
        writeFloat(ptr, lon);
        writeFloat(ptr, gps_alt_cm);
        writeFloat(ptr, baro_alt_cm);
        return ptr - buffer; // 17
    }

    bool fromBuffer(const uint8_t* buffer, uint8_t size) override {
        if (size < 17) return false;
        const uint8_t* ptr = buffer;
        header = *ptr++;
        readFloat(ptr, lat);
        readFloat(ptr, lon);
        readFloat(ptr, gps_alt_cm);
        readFloat(ptr, baro_alt_cm);
        return true;
    }

    void serialOut() const override {
        Serial.print(">Packet_ID: ");          Serial.println(header);
        Serial.print(">Latitude: ");           Serial.println(lat, 6);
        Serial.print(">Longitude: ");          Serial.println(lon, 6);
        Serial.print(">GPS Altitude (cm): ");  Serial.println(gps_alt_cm);
        Serial.print(">Baro Altitude (cm): "); Serial.println(baro_alt_cm);
    }

    float getLat()       const { return lat; }
    float getLon()       const { return lon; }
    float getGpsAlt()    const { return gps_alt_cm; }
    float getBaroAlt()   const { return baro_alt_cm; }

private:
    float lat        = 0.0f;
    float lon        = 0.0f;
    float gps_alt_cm = 0.0f;
    float baro_alt_cm = 0.0f;
};

// GPSPacket
// 14 bytes
class GPSPacket : public Packet {
public:
    GPSPacket() { header = PKT_GPS; }

    GPSPacket(float lat, float lon, float alt_cm, uint8_t sats) {
        header = PKT_GPS;
        fromInput(lat, lon, alt_cm, sats);
    }

    void fromInput(float _lat, float _lon, float _alt_cm, uint8_t _sats) {
        lat        = _lat;
        lon        = _lon;
        gps_alt_cm = _alt_cm;
        satellites = _sats;
    }

    uint8_t toBuffer(uint8_t* buffer, uint8_t maxSize) const override {
        if (maxSize < 14) return 0;
        uint8_t* ptr = buffer;
        *ptr++ = header;
        writeFloat(ptr, lat);
        writeFloat(ptr, lon);
        writeFloat(ptr, gps_alt_cm);
        *ptr++ = satellites;
        return ptr - buffer; // 14
    }

    bool fromBuffer(const uint8_t* buffer, uint8_t size) override {
        if (size < 14) return false;
        const uint8_t* ptr = buffer;
        header = *ptr++;
        readFloat(ptr, lat);
        readFloat(ptr, lon);
        readFloat(ptr, gps_alt_cm);
        satellites = *ptr++;
        return true;
    }

    void serialOut() const override {
        Serial.print(">Packet_ID: ");    Serial.println(header);
        Serial.print(">Latitude: ");     Serial.println(lat, 6);
        Serial.print(">Longitude: ");    Serial.println(lon, 6);
        Serial.print(">Altitude (cm): "); Serial.println(gps_alt_cm);
        Serial.print(">Satellites: ");   Serial.println(satellites);
    }

    float   getLat()        const { return lat; }
    float   getLon()        const { return lon; }
    float   getAlt()        const { return gps_alt_cm; }
    uint8_t getSatellites() const { return satellites; }

private:
    float   lat        = 0.0f;
    float   lon        = 0.0f;
    float   gps_alt_cm = 0.0f;
    uint8_t satellites = 0;
};

// BMPPacket  (barometer + temperature)
// 13 bytes
class BMPPacket : public Packet {
public:
    BMPPacket() { header = PKT_BMP; }

    BMPPacket(float pressure_pa, float altitude_cm, float temperature_c) {
        header = PKT_BMP;
        fromInput(pressure_pa, altitude_cm, temperature_c);
    }

    void fromInput(float _pressure, float _altitude, float _temperature) {
        pressure_pa    = _pressure;
        altitude_cm    = _altitude;
        temperature_c  = _temperature;
    }

    uint8_t toBuffer(uint8_t* buffer, uint8_t maxSize) const override {
        if (maxSize < 13) return 0; // 1 + 3×4 = 13
        uint8_t* ptr = buffer;
        *ptr++ = header;
        writeFloat(ptr, pressure_pa);
        writeFloat(ptr, altitude_cm);
        writeFloat(ptr, temperature_c);
        return ptr - buffer; // 13
    }

    bool fromBuffer(const uint8_t* buffer, uint8_t size) override {
        if (size < 13) return false;
        const uint8_t* ptr = buffer;
        header = *ptr++;
        readFloat(ptr, pressure_pa);
        readFloat(ptr, altitude_cm);
        readFloat(ptr, temperature_c);
        return true;
    }

    void serialOut() const override {
        Serial.print(">Packet_ID: ");      Serial.println(header);
        Serial.print(">Pressure (Pa): ");  Serial.println(pressure_pa);
        Serial.print(">Altitude (cm): ");  Serial.println(altitude_cm);
        Serial.print(">Temperature (C): "); Serial.println(temperature_c, 2);
    }

    float getPressure()    const { return pressure_pa; }
    float getAltitude()    const { return altitude_cm; }
    float getTemperature() const { return temperature_c; }

private:
    float pressure_pa   = 0.0f;
    float altitude_cm   = 0.0f;
    float temperature_c = 0.0f;
};

// IMUPacket  (raw accelerometer + gyroscope)
// 13 bytes
class IMUPacket : public Packet {
public:
    IMUPacket() { header = PKT_IMU; }

    IMUPacket(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz) {
        header = PKT_IMU;
        fromInput(ax, ay, az, gx, gy, gz);
    }

    void fromInput(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz) {
        ax_mg   = ax; ay_mg   = ay; az_mg   = az;
        gx_mdps = gx; gy_mdps = gy; gz_mdps = gz;
    }

    uint8_t toBuffer(uint8_t* buffer, uint8_t maxSize) const override {
        if (maxSize < 13) return 0; // 1 + 6×2 = 13
        uint8_t* ptr = buffer;
        *ptr++ = header;
        writeInt16(ptr, ax_mg);
        writeInt16(ptr, ay_mg);
        writeInt16(ptr, az_mg);
        writeInt16(ptr, gx_mdps);
        writeInt16(ptr, gy_mdps);
        writeInt16(ptr, gz_mdps);
        return ptr - buffer; // 13
    }

    bool fromBuffer(const uint8_t* buffer, uint8_t size) override {
        if (size < 13) return false;
        const uint8_t* ptr = buffer;
        header = *ptr++;
        readInt16(ptr, ax_mg);
        readInt16(ptr, ay_mg);
        readInt16(ptr, az_mg);
        readInt16(ptr, gx_mdps);
        readInt16(ptr, gy_mdps);
        readInt16(ptr, gz_mdps);
        return true;
    }

    void serialOut() const override {
        Serial.print(">Packet_ID: "); Serial.println(header);
        Serial.print(">Accel X: ");   Serial.println(ax_mg);
        Serial.print(">Accel Y: ");   Serial.println(ay_mg);
        Serial.print(">Accel Z: ");   Serial.println(az_mg);
        Serial.print(">Gyro X: ");    Serial.println(gx_mdps);
        Serial.print(">Gyro Y: ");    Serial.println(gy_mdps);
        Serial.print(">Gyro Z: ");    Serial.println(gz_mdps);
    }

    int16_t getAX() const { return ax_mg; }
    int16_t getAY() const { return ay_mg; }
    int16_t getAZ() const { return az_mg; }
    int16_t getGX() const { return gx_mdps; }
    int16_t getGY() const { return gy_mdps; }
    int16_t getGZ() const { return gz_mdps; }

private:
    int16_t ax_mg   = 0, ay_mg   = 0, az_mg   = 0;
    int16_t gx_mdps = 0, gy_mdps = 0, gz_mdps = 0;
};

// KalmanPacket  (filtered state estimate)
// 17 bytes
class KalmanPacket : public Packet {
public:
    KalmanPacket() { header = PKT_KALMAN; }

    KalmanPacket(float altitude, float v_velocity, float roll, float pitch) {
        header = PKT_KALMAN;
        fromInput(altitude, v_velocity, roll, pitch);
    }

    void fromInput(float _alt, float _vvel, float _roll, float _pitch) {
        altitude_cm          = _alt;
        vertical_velocity_cms = _vvel;
        roll_cd              = _roll;
        pitch_cd             = _pitch;
    }

    uint8_t toBuffer(uint8_t* buffer, uint8_t maxSize) const override {
        if (maxSize < 17) return 0; // 1 + 4×4 = 17
        uint8_t* ptr = buffer;
        *ptr++ = header;
        writeFloat(ptr, altitude_cm);
        writeFloat(ptr, vertical_velocity_cms);
        writeFloat(ptr, roll_cd);
        writeFloat(ptr, pitch_cd);
        return ptr - buffer; // 17
    }

    bool fromBuffer(const uint8_t* buffer, uint8_t size) override {
        if (size < 17) return false;
        const uint8_t* ptr = buffer;
        header = *ptr++;
        readFloat(ptr, altitude_cm);
        readFloat(ptr, vertical_velocity_cms);
        readFloat(ptr, roll_cd);
        readFloat(ptr, pitch_cd);
        return true;
    }

    void serialOut() const override {
        Serial.print(">Packet_ID: ");         Serial.println(header);
        Serial.print(">Altitude (cm): ");     Serial.println(altitude_cm);
        Serial.print(">Vert Velocity (cm/s):"); Serial.println(vertical_velocity_cms);
        Serial.print(">Roll (cd): ");         Serial.println(roll_cd, 2);
        Serial.print(">Pitch (cd): ");        Serial.println(pitch_cd, 2);
    }

    float getAltitude()  const { return altitude_cm; }
    float getVVelocity() const { return vertical_velocity_cms; }
    float getRoll()      const { return roll_cd; }
    float getPitch()     const { return pitch_cd; }

private:
    float altitude_cm           = 0.0f;
    float vertical_velocity_cms = 0.0f;
    float roll_cd               = 0.0f;
    float pitch_cd              = 0.0f;
};

// ClientBroadcastPacket  (discovery / pairing) used by version before this one. 
// (Could be used for mesh). You can use it for your own purposes, or ignore it.
// 8 bytes
class ClientBroadcastPacket : public Packet {
public:
    ClientBroadcastPacket() { header = PKT_CLIENT_BROADCAST; }

    ClientBroadcastPacket(uint8_t id, const uint8_t* address, uint8_t channel) {
        header = PKT_CLIENT_BROADCAST;
        fromInput(id, address, channel);
    }

    void fromInput(uint8_t id, const uint8_t* address, uint8_t channel) {
        ID      = id;
        memcpy(Address, address, 5);
        Channel = channel;
    }

    uint8_t toBuffer(uint8_t* buffer, uint8_t maxSize) const override {
        if (maxSize < 8) return 0; // 1 + 1 + 5 + 1 = 8
        uint8_t* ptr = buffer;
        *ptr++ = header;
        *ptr++ = ID;
        memcpy(ptr, Address, 5); ptr += 5;
        *ptr++ = Channel;
        return ptr - buffer; // 8
    }

    bool fromBuffer(const uint8_t* buffer, uint8_t size) override {
        if (size < 8) return false;
        const uint8_t* ptr = buffer;
        header = *ptr++;
        ID     = *ptr++;
        memcpy(Address, ptr, 5); ptr += 5;
        Channel = *ptr++;
        return true;
    }

    void serialOut() const override {
        Serial.print(">Packet_ID: "); Serial.println(header);
        Serial.print(">ID: ");        Serial.println(ID);
        Serial.print(">Address: ");
        for (uint8_t i = 0; i < 5; i++) {
            Serial.print(Address[i], HEX);
            if (i < 4) Serial.print(":");
        }
        Serial.println();
        Serial.print(">Channel: ");   Serial.println(Channel);
    }

    uint8_t getID()      const { return ID; }
    uint8_t getChannel() const { return Channel; }
    void    getAddress(uint8_t* out) const { memcpy(out, Address, 5); }

private:
    uint8_t ID         = 0;
    uint8_t Address[5] = {};
    uint8_t Channel    = 0;
};

// ConfirmationPacket
// 2 bytes
class ConfirmationPacket : public Packet {
public:
    ConfirmationPacket() { header = PKT_CONFIRMATION; }

    explicit ConfirmationPacket(uint8_t id) {
        header = PKT_CONFIRMATION;
        ID     = id;
    }

    uint8_t toBuffer(uint8_t* buffer, uint8_t maxSize) const override {
        if (maxSize < 2) return 0;
        uint8_t* ptr = buffer;
        *ptr++ = header;
        *ptr++ = ID;
        return ptr - buffer; // 2
    }

    bool fromBuffer(const uint8_t* buffer, uint8_t size) override {
        if (size < 2) return false;
        const uint8_t* ptr = buffer;
        header = *ptr++;
        ID     = *ptr++;
        return true;
    }

    void serialOut() const override {
        Serial.print(">Packet_ID: "); Serial.println(header);
        Serial.print(">ID: ");        Serial.println(ID);
    }

    uint8_t getID() const { return ID; }

private:
    uint8_t ID = 0;
};

// MessagePacket
// 32 Bytes
class MessagePacket : public Packet {
public:
    MessagePacket() { header = PKT_MESSAGE; }

    MessagePacket(const String& text) {
        header = PKT_MESSAGE;
        fromInput(text);
    }

    void fromInput(const String& _text) {
        text = _text;
        if (text.length() > 31) text.remove(31);
    }

    uint8_t toBuffer(uint8_t* buffer, uint8_t maxSize) const override {
        uint8_t len = text.length();
        if (maxSize < len + 1) return 0;
        uint8_t* ptr = buffer;
        *ptr++ = header;                 
        for (uint8_t i = 0; i < len; i++) *ptr++ = text[i];
        return ptr - buffer;
    }

    bool fromBuffer(const uint8_t* buffer, uint8_t size) override {
        if (size < 1) return false;
        header = buffer[0];              
        uint8_t len = min((uint8_t)(size - 1), (uint8_t)31);
        text = "";
        text.reserve(len);
        for (uint8_t i = 0; i < len; i++) text += (char)buffer[i + 1];
        return true;
    }

    void serialOut() const override {
        Serial.print(">Message: ");
        Serial.println(text);
    }

    String getText()       const { return text; }
    uint8_t getSize()      const { return (uint8_t)text.length(); }

private:
    String text;
};

// Packet creation based on Type. defined here so both RFMaster and RFSlave share one copy.
// Caller owns the returned pointer and must delete it.
inline Packet* Packet::create(uint8_t id) {
    switch (id) {
        case PKT_COMMAND:          return new CommandPacket();
        case PKT_GPS:              return new GPSPacket();
        case PKT_IMU:              return new IMUPacket();
        case PKT_TELEMETRY:        return new TelemetryPacket();
        case PKT_BMP:              return new BMPPacket();
        case PKT_KALMAN:           return new KalmanPacket();
        case PKT_CONFIRMATION:     return new ConfirmationPacket();
        case PKT_CLIENT_BROADCAST: return new ClientBroadcastPacket();
        default:                   return nullptr;
    }
}