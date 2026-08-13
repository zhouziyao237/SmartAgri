# V2.3.1 PC software checks

Performed in the packaging environment:
- Python syntax compilation passed for all PC_Tools Python files.
- Web `app.js` passed `node --check`.
- V2.3.0 legacy profile migration was exercised with a slot-only profile; it becomes `target.mode=generic`.
- A profile carrying legacy `target_device_id=1002` was normalized to `target.mode=node, device_id=1002`.

Not performed here:
- Windows PySide6 GUI runtime launch.
- Real COM/STM32/ESP hardware transaction test.

Firmware directories are copied unchanged from V2.3.0; this update is PC upper-computer only.
