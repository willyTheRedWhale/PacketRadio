## 20.04.2026
### Version 1.0.1
- Added a new configuration to class Connection. This lets you to choose the SPI connection used to pass to the NRF24 using radio.begin(SPI here)

### Usage
```cpp
Connection conn;

conn.setSPI(&SPI1); // if you want to use SPI1 or a custome SPI pinout. The paramter needs to be a pointer
```
- If you do not specify which SPI should be used, the default SPI on the board will be used.
- The SPI will be started (SPI->begin()) by RFMaster/RFSlave in init();