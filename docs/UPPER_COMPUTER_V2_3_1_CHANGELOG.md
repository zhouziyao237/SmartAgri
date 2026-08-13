# SmartAgri Upper Computer V2.3.1

This release changes PC upper-computer software only. ESP firmware and STM32 V2.3.0 firmware/protocol remain unchanged.

## Fixed: local templates can target different Nodes

V2.3.0 templates stored only Slot payloads. Device/Node ID lived in the sensor editor and was not part of a template, so multiple local templates could not independently remember different target Nodes.

V2.3.1 profile schema:

```json
{
  "schema_version": 2,
  "name": "Node1001-PB13",
  "object_type": "sensor_slot",
  "target": {"mode": "node", "device_id": 1001},
  "slots": [],
  "description": "",
  "updated_at": 0
}
```

A template can be:
- bound to a Node (`target.mode=node`), or
- generic (`target.mode=generic`) and applied to a Node selected at run time.

Legacy V2.3.0 templates are loaded automatically as generic templates and can then be bound to individual Nodes.

New APIs:
- `POST /api/profiles/target` — change one template's Node binding.
- `POST /api/profiles/apply` — apply a template directly to a selected Node.

## Qt architecture refactor

The previous Qt program put every page and action in one `MainWindow`. V2.3.1 uses registered page objects:

```text
qt_app/
  api.py
  base.py          BasePage + register_page()
  pages.py         DashboardPage / SensorConfigPage / ControlPage / ...
  main_window.py   page host only
smartagri_qt.py    launcher only
```

To add a future local page, create a `BasePage` subclass and decorate it with `@register_page(order)`; `MainWindow` discovers and mounts it.

## Web architecture refactor

The inline procedural JavaScript was split into page objects:
- `ApiClient`
- `BasePage`
- `DashboardPage`
- `SensorPage`
- `ControlPage`
- `ProfilesPage`
- `SettingsPage`
- `EventsPage`
- `SmartAgriApp`

Navigation is built from registered page objects. Styles and application logic are now separate `styles.css` and `app.js` files.

## Extensible local object model

Templates now contain `object_type`. V2.3.1 implements `sensor_slot`. The same storage model is reserved for future types such as `actuator`, `rule`, `gateway`, and `scene`, so those features do not need a second unrelated local-template system.
