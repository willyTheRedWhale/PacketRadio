// Stack-based: no dynamic allocation
bool parsePacket(const uint8_t* buffer, uint8_t size, Packet& outPkt) {
    switch (buffer[0]) {
        case PKT_COMMAND:   return outPkt.fromBuffer(buffer, size);
        case PKT_TELEMETRY: return outPkt.fromBuffer(buffer, size);
        case PKT_GPS:       return outPkt.fromBuffer(buffer, size);
        // Add others...
    }
    return false;
}

Packet* createPacket(const uint8_t* buffer, uint8_t size) {
    switch (buffer[0]) {
        case PKT_COMMAND:   return new CommandPacket(buffer, size);
        case PKT_TELEMETRY: return new TelemetryPacket(buffer, size);
        // ...
    }
    return nullptr;
}

uint8_t buf[32];
size_t len = receiveFromTransport(buf);

TelemetryPacket tp;
if (parsePacket(buf, len, tp)) {
    tp.serialOut();  // debug
}

// To send
uint8_t outBuf[32];
uint8_t outLen = tp.toBuffer(outBuf, sizeof(outBuf));
sendToTransport(outBuf, outLen);
