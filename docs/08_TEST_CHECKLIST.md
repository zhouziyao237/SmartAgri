# V2.2.0 test checklist

1. ESP32 MASTER and C3 nodes are already ESP-NOW linked.
2. Set all production ESPs `TESTMODE=0`.
3. MASTER ESP uses `UPLINK=NONE`.
4. Verify SLAVE STM32 <-> C3 A5 rx counter increases.
5. Connect MASTER STM32 <-> Classic ESP32 on PA9/PA10 <-> GPIO16/17.
6. Reset MASTER ESP and verify MASTER STM32 becomes role=MASTER/device=1.
7. Connect MASTER STM32 PA2/PA3/GND to a USB-TTL at 115200.
8. Close other serial terminals using that COM port.
9. Double-click `PC_Tools/run_server.bat`.
10. Confirm backend prints `MASTER STM32 connected: COMx @ 115200`.
11. Open `http://127.0.0.1:8080`.
12. Confirm Node 1 and Node 1001 appear.
13. Click `发现全部节点`; all online nodes should respond within a few seconds.
14. Read Capability for Node 1001.
15. Configure one sensor slot with Begin -> Set -> Validate -> Commit.
16. Verify telemetry appears in Web.
17. Verify one GPIO control command if safe hardware is attached.
