# Packet Radio  Documentation

Guide to setting up typed, bi directional wireless communication between two devices using nRF24L01 modules.
Made with help of a lot of caffeine
---

## Table of Contents

1. [What is PacketRadio?](#what-is-packetradio)
2. [How it works](#how-it-works)
3. [Installation](#installation)
4. [Hardware setup](#hardware-setup)
5. [Getting started](#getting-started)
6. [Architecture](#architecture)
7. [API Reference](#api-reference)
   - [Connection](#connection)
   - [Packet](#packet)
   - [RFMaster](#rfmaster)
   - [RFSlave](#rfslave)
   - [Built in packet types](#built-in-packet-types)
8. [How to add a new packet type](#how-to-add-a-new-packet-type)
9. [Full examples](#full-examples)

---

## What is PacketRadio?

This is is a small C++ library for Arduino and ESP32 that lets two devices talk to each other wirelessly over nRF24L01 radio modules. 
The key idea is that you can send **different types of data** GPS coordinates, sensor readings, commands, text messages over the same radio link, 
without writing separate handling code for each one.

One device is the **Master**. It sends a packet to the Slave. 
The Slave automatically sends a packet back inside the radio's built in acknowledgement (ACK) so you get a reply without a second transmission. 

Both devices can exchange different types of data every cycle.

---

## How it works

To understand PacketRadio, it helps to think of it in layers (OSI model) each layer handles one job and hands off to the next.

```
┌─────────────────────────────────────┐
│  Layer 4 — Your application         │  Your flight controller, sensor logger, etc.
├─────────────────────────────────────┤
│  Layer 3 — RFMaster / RFSlave       │  Sends and receives typed packets
├─────────────────────────────────────┤
│  Layer 2 — Connection + Packet      │  Defines what data looks like and link config
├─────────────────────────────────────┤
│  Layer 1 — RF24 library             │  Talks directly to the nRF24 hardware
└─────────────────────────────────────┘
```

This is inspired by the [OSI model](https://en.wikipedia.org/wiki/OSI_model) the same idea used in real computer networks. 
Each layer only needs to know about the layer directly below it.

### The ACK payload trick

Normally, the nRF24 sends a tiny empty acknowledgement when it receives a packet just to confirm delivery. 
PacketRadio uses **ACK payloads**, a feature of the nRF24 that lets the Slave attach real data to that acknowledgement. 
This means:

- Master sends a packet, Slave receives it
- Slave's reply is automatically sent back inside the ACK
- Master reads the ACK payload

This gives you **two way communication in a single transmission cycle**, without the Slave ever needing to initiate a send.

### Important timing detail

Because the ACK is sent *at the moment the packet arrives*, the Slave must preload its reply **before** the next packet comes in. 
In practice this means: after receiving a packet and processing it, immediately queue the next ACK payload. 
PacketRadio handles reminding you of this through the `queueAckPacket()` method on `RFSlave`.

---

## Installation

### Dependencies

You need the RF24 library installed first:

- In the Arduino IDE: go to **Sketch > Include Library > Manage Libraries**, search for `RF24`, and install it.
- Or manually from [github.com/nRF24/RF24](https://github.com/nRF24/RF24)

### Installing PacketRadio

1. Download or clone this repository
2. Copy the `.hpp` files into your project folder (next to your `.ino` or `.cpp` file):
   - `Packet.hpp`
   - `Connection.hpp`
   - `RFMaster.hpp` *(for the master device)*
   - `RFSlave.hpp` *(for the slave device)*
3. Include the relevant files at the top of your sketch

```cpp
// master device:
#include "RFMaster.hpp"

// slave device:
#include "RFSlave.hpp"
```

You only need to include `RFMaster.hpp` or `RFSlave.hpp` they pull in `Packet.hpp` and `Connection.hpp` automatically.

---

## Hardware setup

### What you need

- 2× nRF24L01 modules (the PA+LNA version with external antenna is recommended for range)
- 2× 100µF capacitors **one per module**! placed across the VCC and GND pins. The nRF24 draws current in bursts and without a capacitor it might not work as intended.
- 2× microcontrollers (ESP32, Arduino, etc.)

### Wiring (ESP32)

| nRF24L01 Pin | ESP32 Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| CE | GPIO 4 (or any digital pin) |
| CSN | GPIO 5 (or any digital pin) |
| SCK | GPIO 18 |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| IRQ | Not connected (optional) |

> **The nRF24L01 runs on 3.3V. Do not connect VCC to 5V you will turn it into a well done steak.**


## Getting started

Here is the minimum code to get two devices talking. The Master sends a `CommandPacket` every second, and the Slave echoes back a `ConfirmationPacket` in the ACK.

### Master (sender)

```cpp
#include <SPI.h>
#include <RF24.h>
#include "RFMaster.hpp"

// CE pin = 4, CSN pin = 5
RF24 radio(4, 5);
RFMaster master(radio);

void setup() {
    Serial.begin(115200);
    radio.begin();

    // Both master and slave must use the same address and channel
    Connection conn;
    uint8_t addr[5] = { 'R', 'F', 'T', 'E', 'S' };
    conn.setAddress(addr);
    conn.setChannel(76);

    master.init(1, conn); // ID = 1
}

void loop() {
    // Send a command packet master takes ownership and deletes it after sending
    master.sendPacket(new CommandPacket(0, 128, 128, 128, 100), true);
    master.update();

    // Check if the slave sent something back in the ACK
    if (master.hasNewPacket()) {
        Packet* reply = master.takePacket(); // you own this now
        reply->serialOut(); // Output
        delete reply;
    }

    Serial.print("Connected: ");
    Serial.println(master.isConnected() ? "yes" : "no"); //Connection Health

    delay(1000);
}
```

### Slave (receiver)

```cpp
#include <SPI.h>
#include <RF24.h>
#include "RFSlave.hpp"

RF24 radio(4, 5);
RFSlave slave(radio);

void setup() {
    Serial.begin(115200);
    radio.begin();

    // Must match master exactly
    Connection conn;
    uint8_t addr[5] = { 'R', 'F', 'T', 'E', 'S' };
    conn.setAddress(addr);
    conn.setChannel(76);

    slave.init(2, conn); // ID = 2

    // Preload the first ACK payload before the master sends anything
    slave.queueAckPacket(new ConfirmationPacket(2));
}

void loop() {
    slave.update();

    if (slave.hasNewPacket()) {
        Packet* incoming = slave.takePacket(); // you own this now
        incoming->serialOut();
        delete incoming;

        // Queue the next ACK payload immediately after receiving
        slave.queueAckPacket(new ConfirmationPacket(2));
    }
}
```

---

## Architecture

### Connection

`Connection` holds the radio configuration that **both master and slave must share**. 
Both devices must be configured with the same address and channel, otherwise they will not hear each other.
This could be also used to talk to multiple devices. -> Allowing each Micro Controller act as master or Slave, based on whom he wants to talk to.

It also tracks link health it knows how long ago the last packet was received, and whether the connection is still alive.

### Packet

`Packet` is the base class for all data types you can send. Every packet must be able to do three things:

- **Serialize** itself into a byte buffer (`toBuffer`) turning structured data into raw bytes for the radio
- **Deserialize** itself from a byte buffer (`fromBuffer`) turning raw bytes back into structured data
- **Print** itself for debugging (`serialOut`)

The first byte of every serialized packet is always the **packet ID**.
A number that tells the receiver what type of packet it is. When a packet arrives, `Packet::create()` reads this first byte and constructs the right object automatically.
The maximum number of Packet Types are 256

### RFMaster

The master controls the conversation. It calls `radio.write()` to send a packet, then listens for the ACK payload the slave sent back. 
It never listens passively it only receives data as part of the ACK response to its own transmissions.

### RFSlave

The slave listens continuously. 
When a packet arrives, the radio hardware automatically sends back whatever ACK payload was preloaded.
The slave then processes the incoming packet and loads the next ACK payload, ready for the master's next transmission.
You can update the Ack packet every iteration, while waiting for the master to send you a message (asking for data), this way you could reach a fully real time bi directional connection.

---

## API Reference

### Connection

Holds the radio configuration and tracks link health. Create one, configure it, and pass it to both `master.init()` and `slave.init()`.

```cpp
Connection conn;
```

#### Configuration

```cpp
// Set the 5 byte address both devices must use the same address!
uint8_t addr[5] = { 'R', 'F', 'T', 'E', 'S' };
conn.setAddress(addr);

// Set the RF channel (0–125) both devices must use the same channel
conn.setChannel(76);

// Optional: set node IDs
conn.setMyID(1);
conn.setRemoteID(2);

//Set the SPI to use, needs to be a pointer. default is SPI0, the default spi on the board, it stays that until changed.
conn.setSPI(&SPI1 or &SPI);
```

#### Reading configuration back

```cpp
uint8_t addr[5];
conn.getAddress(addr);    // fills addr with the stored address
uint8_t ch = conn.getChannel();
uint8_t id = conn.getMyID();
uint8_t rid = conn.getRemoteID();
```

#### Link health

```cpp
// Returns true if a packet was received within the last 5 seconds. The time (5s) could be configured in Connection.hpp 
bool alive = conn.isConnected();

// Milliseconds since the last received packet
uint32_t ms = conn.msSinceLastReceived();

// Milliseconds since the last sent packet
uint32_t ms = conn.msSinceLastSent();
```

---

### Packet

Abstract base class. You never create a `Packet` directly you use one of the built in subclasses, or write your own.

#### Factory method

```cpp
// Creates the correct subclass for a given packet ID byte.
// Returns nullptr for unknown IDs.
// You own the returned pointer and must delete it.
Packet* p = Packet::create(buffer[0]);
```

#### Interface (implemented by every subclass)

```cpp
// Serialize to buffer. Returns bytes written, or 0 on failure.
uint8_t size = packet->toBuffer(buffer, sizeof(buffer));

// Deserialize from buffer. Returns true on success.
bool ok = packet->fromBuffer(buffer, size);

// Print all fields to Serial
packet->serialOut();

// Get the packet type ID (first byte)
// defined by the base class. Do not override
uint8_t id = packet->getHeader(); 
```

---

### RFMaster

```cpp
RF24 radio(CE_PIN, CSN_PIN);
RFMaster master(radio);
```

#### `init(myID, connection)`

Configure the radio. Call once in `setup()`. Both master and slave must use the same `Connection`.

```cpp
master.init(1, conn);
```

#### `sendPacket(packet, takeOwnership)`

Queue a packet to be sent on the next `update()` call.

- `packet` pointer to a `Packet` subclass
- `takeOwnership` if `true`, the master will `delete` the packet after sending. If `false`, you manage the lifetime yourself.

```cpp
// Heap allocated let master delete it (recommended)
master.sendPacket(new CommandPacket(0, 128, 128, 128, 100), true);

// Stack allocatedyou manage it, do NOT pass true
CommandPacket cmd(0, 128, 128, 128, 100);
master.sendPacket(&cmd, false);
// cmd is automatically cleaned up when it goes out of scope
```

> **Never pass `true` for a stack allocated object.** Calling `delete` on a stack address will crash the device. This caused my esp32 to crash :(

#### `update()`

Drives the radio. Sends the queued packet (if any) and reads any ACK payload the slave sent back. Call once per loop iteration or at your rate.

```cpp
master.update();
```

#### `hasNewPacket()`

Returns `true` if a new packet arrived from the slave since the last `takePacket()` call.

```cpp
if (master.hasNewPacket()) { ... }
```

#### `takePacket()`

Returns the incoming packet and transfers ownership to you. You **must** call `delete` on it when you are done. Returns `nullptr` if no packet is waiting.

```cpp
Packet* p = master.takePacket();
if (p) {
    p->serialOut();
    delete p; // always delete it
}
```

#### `isConnected()`

Returns `true` if a packet was received from the slave within the last 5 seconds. (Configureable)

```cpp
if (!master.isConnected()) {
    Serial.println("Lost link!");
}
```

#### `msSinceLastReceived()`

Milliseconds since the last packet was received from the slave.

```cpp
uint32_t age = master.msSinceLastReceived();
```

---

### RFSlave

```cpp
RF24 radio(CE_PIN, CSN_PIN);
RFSlave slave(radio);
```

#### `init(myID, connection)`

Configure the radio. Call once in `setup()`. Must use the same `Connection` as the master.

```cpp
slave.init(2, conn);
```

#### `queueAckPacket(packet)`

Preloads the ACK payload to be sent when the master's next packet arrives. The packet is serialized immediately. You can delete it after this call if you want.

**Call this:**
1. Once in `setup()` before the master starts sending, so the first ACK is ready
2. Immediately after processing an incoming packet in `loop()` , so the next ACK is ready

```cpp
// In setup()
slave.queueAckPacket(new ConfirmationPacket(2)); // owns the pointer briefly, serializes it

// After receiving in loop()
slave.queueAckPacket(new TelemetryPacket(lat, lon, alt, baroAlt));
```

#### `update()`

Drives the radio. Reads any incoming packet from the master. Call once per loop iteration.

```cpp
slave.update();
```

#### `hasNewPacket()` / `takePacket()`

Same as master, returns whether a packet is waiting, and claims it.

```cpp
if (slave.hasNewPacket()) {
    Packet* p = slave.takePacket();
    p->serialOut();
    delete p;
}
```

#### `isConnected()` / `msSinceLastReceived()`

Same as master.

---

### Built-in packet types

All packet types below can be used on either end of the link.

---

#### `CommandPacket`

Sends flight control inputs. Typically sent from master to slave.

| Field | Type | Description |
|---|---|---|
| autopilot | uint8_t | 0 = manual, 1 = autopilot enabled |
| pitch | uint8_t | Target pitch (0–255) |
| roll | uint8_t | Target roll (0–255) |
| yaw | uint8_t | Target yaw (0–255) |
| throttle | uint8_t | Throttle (0–255) |

**Size:** 6 bytes

```cpp
// Create
CommandPacket* cmd = new CommandPacket(0, 128, 128, 128, 100);

// Read fields
cmd->getAutopilot();
cmd->getPitch();
cmd->getRoll();
cmd->getYaw();
cmd->getThrottle();
```

---

#### `TelemetryPacket`

Combined GPS + barometer data. Typically sent from slave to master via ACK.

| Field | Type | Description |
|---|---|---|
| lat | float | Latitude |
| lon | float | Longitude |
| gps_alt_cm | float | GPS altitude in centimetres |
| baro_alt_cm | float | Barometric altitude in centimetres |

**Size:** 17 bytes

```cpp
TelemetryPacket* pkt = new TelemetryPacket(51.5074, -0.1278, 5000.0, 4980.0);

pkt->getLat();
pkt->getLon();
pkt->getGpsAlt();
pkt->getBaroAlt();
```

---

#### `GPSPacket`

Raw GPS data.

| Field | Type | Description |
|---|---|---|
| lat | float | Latitude |
| lon | float | Longitude |
| gps_alt_cm | float | Altitude in centimetres |
| satellites | uint8_t | Number of satellites |

**Size:** 14 bytes

```cpp
GPSPacket* pkt = new GPSPacket(51.5074, -0.1278, 5000.0, 8);

pkt->getLat();
pkt->getLon();
pkt->getAlt();
pkt->getSatellites();
```

---

#### `IMUPacket`

Raw accelerometer and gyroscope data.

| Field | Type | Description |
|---|---|---|
| ax_mg | int16_t | Accelerometer X (milli g) |
| ay_mg | int16_t | Accelerometer Y (milli g) |
| az_mg | int16_t | Accelerometer Z (milli g) |
| gx_mdps | int16_t | Gyroscope X (milli degrees/sec) |
| gy_mdps | int16_t | Gyroscope Y (milli degrees/sec) |
| gz_mdps | int16_t | Gyroscope Z (milli degrees/sec) |

**Size:** 13 bytes (1 header + 6 × 2)

```cpp
IMUPacket* pkt = new IMUPacket(0, 0, 1000, 0, 0, 0);

pkt->getAX(); pkt->getAY(); pkt->getAZ();
pkt->getGX(); pkt->getGY(); pkt->getGZ();
```

---

#### `BMPPacket`

Barometric pressure sensor data.

| Field | Type | Description |
|---|---|---|
| pressure_pa | float | Pressure in Pascals |
| altitude_cm | float | Altitude in centimetres |
| temperature_c | float | Temperature in °C |

**Size:** 13 bytes (1 header + 3 × 4)

```cpp
BMPPacket* pkt = new BMPPacket(101325.0, 100.0, 22.5);

pkt->getPressure();
pkt->getAltitude();
pkt->getTemperature();
```

---

#### `KalmanPacket`

Kalman-filtered state estimate.

| Field | Type | Description |
|---|---|---|
| altitude_cm | float | Filtered altitude in centimetres |
| vertical_velocity_cms | float | Vertical velocity in cm/s |
| roll_cd | float | Roll in centidegrees |
| pitch_cd | float | Pitch in centidegrees |

**Size:** 17 bytes (1 header + 4 × 4)

```cpp
KalmanPacket* pkt = new KalmanPacket(5000.0, 10.0, 0.0, 0.0);

pkt->getAltitude();
pkt->getVVelocity();
pkt->getRoll();
pkt->getPitch();
```

---

#### `ConfirmationPacket`

A simple acknowledgement carrying a node ID. 
Useful as a default ACK payload when you have nothing else to send.
Keeps the connection alive.

| Field | Type | Description |
|---|---|---|
| ID | uint8_t | Node ID of the sender |

**Size:** 2 bytes

```cpp
ConfirmationPacket* pkt = new ConfirmationPacket(2);
pkt->getID();
```

---

#### `MessagePacket`

A short text message. Maximum 31 characters.

| Field | Type | Description |
|---|---|---|
| text | String | Message text (max 31 chars) |

**Size:** 1–32 bytes (1 header + up to 31 chars)

```cpp
MessagePacket* pkt = new MessagePacket("Hello from slave!");
pkt->getText();
pkt->getSize();
```

---

#### `ClientBroadcastPacket`

Used for device discovery.
A node announces its own ID, address, and channel so others can connect to it.

| Field | Type | Description |
|---|---|---|
| ID | uint8_t | Node ID |
| Address | uint8_t[5] | Node radio address |
| Channel | uint8_t | Node radio channel |

**Size:** 8 bytes

```cpp
uint8_t addr[5] = { 'N', 'O', 'D', 'E', '1' };
ClientBroadcastPacket* pkt = new ClientBroadcastPacket(3, addr, 76);

pkt->getID();
pkt->getChannel();
uint8_t outAddr[5];
pkt->getAddress(outAddr);
```

---

## How to add a new packet type

Adding a new data type takes four steps and no changes to `RFMaster`, `RFSlave`, or `Connection`.

### Step 1: Add an ID to the enum

In `Packet.hpp`, add your new type to `PacketID`:

```cpp
typedef enum : uint8_t {
    PKT_KEEP_ALIVE       = 0,
    PKT_COMMAND          = 1,
    // ... existing types ...
    PKT_BATTERY          = 10, // ← add yours here
} PacketID;
```

### Step 2: Write the class

Inherit from `Packet` and implement the three required methods. 
Use the serialization helpers `writeFloat`, `readFloat`, `writeInt16`, etc.

```cpp
class BatteryPacket : public Packet {
public:
    BatteryPacket() { header = PKT_BATTERY; }

    BatteryPacket(float voltage, uint8_t percent) {
        header = PKT_BATTERY;
        fromInput(voltage, percent);
    }

    void fromInput(float _voltage, uint8_t _percent) {
        voltage = _voltage;
        percent = _percent;
    }

    // Serialize write header first, then fields
    // Size: 1 (header) + 4 (float) + 1 (uint8) = 6 bytes
    uint8_t toBuffer(uint8_t* buffer, uint8_t maxSize) const override {
        if (maxSize < 6) return 0;
        uint8_t* ptr = buffer;
        *ptr++ = header;
        writeFloat(ptr, voltage);
        *ptr++ = percent;
        return ptr - buffer;
    }

    // Deserialize read header first, then fields
    bool fromBuffer(const uint8_t* buffer, uint8_t size) override {
        if (size < 6) return false;
        const uint8_t* ptr = buffer;
        header  = *ptr++;
        readFloat(ptr, voltage);
        percent = *ptr++;
        return true;
    }

    void serialOut() const override {
        Serial.print(">Voltage: "); Serial.println(voltage);
        Serial.print(">Percent: "); Serial.println(percent);
    }

    float   getVoltage() const { return voltage; }
    uint8_t getPercent() const { return percent; }

private:
    float   voltage = 0.0f;
    uint8_t percent = 0;
};
```

### Step 3 Register it in the factory

In `Packet.hpp`, add a `case` to `Packet::create()`:

```cpp
inline Packet* Packet::create(uint8_t id) {
    switch (id) {
        // ... existing cases ...
        case PKT_BATTERY: return new BatteryPacket(); // add this
        default:          return nullptr;
    }
}
```

### Step 4 Use it

```cpp
// Send from master
master.sendPacket(new BatteryPacket(3.7, 85), true);

// Receive anywhere
Packet* p = master.takePacket();
if (p && p->getHeader() == PKT_BATTERY) {
    BatteryPacket* bat = static_cast<BatteryPacket*>(p);
    Serial.println(bat->getVoltage());
}
delete p;
```

### Rules to follow when writing a packet

- The first byte of `toBuffer` must always be `*ptr++ = header`
- The first byte of `fromBuffer` must always be `header = *ptr++`
- The size check at the start of `toBuffer` must equal exactly: 1 (header) + sum of all field sizes
- Never write more bytes than the size check allows the nRF24 maximum payload is **32 bytes**

---

## Full examples

### Example 1: Master sends commands, Slave replies with telemetry

**Master:**
```cpp
#include <SPI.h>
#include <RF24.h>
#include "RFMaster.hpp"

RF24 radio(4, 5);
RFMaster master(radio);

void setup() {
    Serial.begin(115200);
    radio.begin();

    Connection conn;
    uint8_t addr[5] = { 'D', 'R', 'O', 'N', 'E' };
    conn.setAddress(addr);
    conn.setChannel(100);
    master.init(1, conn);
}

void loop() {
    // Send control inputs
    master.sendPacket(new CommandPacket(0, 128, 128, 128, 150), true);
    master.update();

    // Read telemetry from slave
    if (master.hasNewPacket()) {
        Packet* p = master.takePacket();
        if (p->getHeader() == PKT_TELEMETRY) {
            TelemetryPacket* telem = static_cast<TelemetryPacket*>(p);
            Serial.print("Lat: ");    Serial.println(telem->getLat(), 6);
            Serial.print("Lon: ");    Serial.println(telem->getLon(), 6);
            Serial.print("Alt: ");    Serial.println(telem->getBaroAlt());
        }
        delete p;
    }

    delay(100);
}
```

**Slave:**
```cpp
#include <SPI.h>
#include <RF24.h>
#include "RFSlave.hpp"

RF24 radio(4, 5);
RFSlave slave(radio);

float lat = 51.5074, lon = -0.1278, gpsAlt = 5000, baroAlt = 4980;

void setup() {
    Serial.begin(115200);
    radio.begin();

    Connection conn;
    uint8_t addr[5] = { 'D', 'R', 'O', 'N', 'E' };
    conn.setAddress(addr);
    conn.setChannel(100);
    slave.init(2, conn);

    // Preload first ACK
    slave.queueAckPacket(new TelemetryPacket(lat, lon, gpsAlt, baroAlt));
}

void loop() {
    slave.update();

    if (slave.hasNewPacket()) {
        Packet* p = slave.takePacket();
        if (p->getHeader() == PKT_COMMAND) {
            CommandPacket* cmd = static_cast<CommandPacket*>(p);
            Serial.print("Throttle: "); Serial.println(cmd->getThrottle());
            // ... apply to motors ...
        }
        delete p;

        // Update sensor readings and queue next ACK
        // (in a real project, read from actual sensors here)
        slave.queueAckPacket(new TelemetryPacket(lat, lon, gpsAlt, baroAlt));
    }
}
```

---

### Example 2: Sending a text message

```cpp
// Master sends a message
master.sendPacket(new MessagePacket("Hello Slave!"), true);
master.update();

// Slave receives it
if (slave.hasNewPacket()) {
    Packet* p = slave.takePacket();
    if (p->getHeader() == PKT_MESSAGE) {
        MessagePacket* msg = static_cast<MessagePacket*>(p);
        Serial.println(msg->getText());
    }
    delete p;
}
```

---