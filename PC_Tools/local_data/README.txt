V2.4.0 package intentionally does not ship profiles.json/settings.json.
Existing local templates/settings are not overwritten when extracting over an existing installation.
Templates keep object_type + target(mode/device_id) + payload, so future actuator/rule/gateway/scene object templates can use the same store.
