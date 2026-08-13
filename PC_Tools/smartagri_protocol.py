import struct

SA_SOF = 0xA5
SA_CMD_ROUTE = 0x00E1
SA_ROUTE_VERSION = 2
SA_DEVICE_MASTER = 1
SA_DEVICE_PC = 0xFFFFFFFE
SA_DEVICE_BROADCAST = 0xFFFFFFFF

CMD_LINK_HELLO = 0x00F0
CMD_LINK_ACK = 0x00F1
CMD_LINK_TEST = 0x00F2
CMD_CAPS_GET = 0x3000
CMD_CAPS_REPLY = 0x3001
CMD_CONFIG_GET = 0x3002
CMD_CONFIG_REPLY = 0x3003
CMD_CONFIG_BEGIN = 0x3010
CMD_CONFIG_SENSOR_SET = 0x3011
CMD_CONFIG_SENSOR_DEL = 0x3012
CMD_CONFIG_VALIDATE = 0x3013
CMD_CONFIG_COMMIT = 0x3014
CMD_CONFIG_ROLLBACK = 0x3015
CMD_CONFIG_FACTORY = 0x3016
CMD_NODE_SET = 0x3020
CMD_NODE_INFO = 0x3021
CMD_RADIO_CONFIG_GET = 0x3030
CMD_RADIO_CONFIG_REPLY = 0x3031
CMD_RADIO_CONFIG_SET = 0x3032
CMD_TELEMETRY = 0x3100
CMD_DIAG_TELEMETRY = 0x31F0
CMD_CONTROL_GPIO = 0x3200
CMD_CONTROL_RESULT = 0x3201
CMD_HEARTBEAT = 0x3300
CMD_OTA_INFO_GET = 0x3400
CMD_OTA_INFO_REPLY = 0x3401
CMD_OTA_PREPARE = 0x3402
CMD_OTA_BEGIN = 0x3403
CMD_OTA_DATA = 0x3404
CMD_OTA_END = 0x3405
CMD_OTA_ABORT = 0x3406
CMD_OTA_STATUS = 0x3407
CMD_OTA_RESULT = 0x3408

OTA_COMPONENT_STM32 = 1
OTA_COMPONENT_ESP = 2
OTA_TARGET_STM32F103C8 = 0x0101
OTA_TARGET_ESP32 = 0x0201
OTA_TARGET_ESP32C3 = 0x0202
OTA_STATE_IDLE = 0
OTA_STATE_READY = 1
OTA_STATE_RECEIVING = 2
OTA_STATE_VERIFYING = 3
OTA_STATE_REBOOT_PENDING = 4
OTA_STATE_ERROR = 5

CMD_NAMES = {
    CMD_LINK_HELLO: 'LINK_HELLO', CMD_LINK_ACK: 'LINK_ACK', CMD_LINK_TEST: 'LINK_TEST',
    CMD_CAPS_GET: 'CAPS_GET', CMD_CAPS_REPLY: 'CAPS_REPLY',
    CMD_CONFIG_GET: 'CONFIG_GET', CMD_CONFIG_REPLY: 'CONFIG_REPLY',
    CMD_CONFIG_BEGIN: 'CONFIG_BEGIN', CMD_CONFIG_SENSOR_SET: 'CONFIG_SENSOR_SET',
    CMD_CONFIG_SENSOR_DEL: 'CONFIG_SENSOR_DEL', CMD_CONFIG_VALIDATE: 'CONFIG_VALIDATE',
    CMD_CONFIG_COMMIT: 'CONFIG_COMMIT', CMD_CONFIG_ROLLBACK: 'CONFIG_ROLLBACK',
    CMD_CONFIG_FACTORY: 'CONFIG_FACTORY', CMD_NODE_SET: 'NODE_SET', CMD_NODE_INFO: 'NODE_INFO',
    CMD_RADIO_CONFIG_GET: 'RADIO_CONFIG_GET', CMD_RADIO_CONFIG_REPLY: 'RADIO_CONFIG_REPLY',
    CMD_RADIO_CONFIG_SET: 'RADIO_CONFIG_SET',
    CMD_TELEMETRY: 'TELEMETRY', CMD_DIAG_TELEMETRY: 'DIAG_TELEMETRY',
    CMD_CONTROL_GPIO: 'CONTROL_GPIO', CMD_CONTROL_RESULT: 'CONTROL_RESULT', CMD_HEARTBEAT: 'HEARTBEAT',
    CMD_OTA_INFO_GET: 'OTA_INFO_GET', CMD_OTA_INFO_REPLY: 'OTA_INFO_REPLY', CMD_OTA_PREPARE: 'OTA_PREPARE',
    CMD_OTA_BEGIN: 'OTA_BEGIN', CMD_OTA_DATA: 'OTA_DATA', CMD_OTA_END: 'OTA_END', CMD_OTA_ABORT: 'OTA_ABORT',
    CMD_OTA_STATUS: 'OTA_STATUS', CMD_OTA_RESULT: 'OTA_RESULT',
}

ROUTE_FMT = '<BBBBIIHHHH'
ROUTE_LEN = struct.calcsize(ROUTE_FMT)
SLOT_FMT = '<BBHHBBHHHIIiiiIII'
SLOT_LEN = struct.calcsize(SLOT_FMT)
NODE_CONFIG_HEAD_FMT = '<HBBIHHB3x'
NODE_CONFIG_HEAD_LEN = struct.calcsize(NODE_CONFIG_HEAD_FMT)
DIAG_TELEM_FMT = '<BBHHHi'
DIAG_TELEM_LEN = struct.calcsize(DIAG_TELEM_FMT)
OTA_INFO_FMT = '<BBHIII'
OTA_RESULT_FMT = '<BBhIII'
OTA_BEGIN_FMT = '<BBHIII'
OTA_DATA_HEAD_FMT = '<BBHII'
OTA_END_FMT = '<BBHII'
RADIO_CONFIG_FMT = '<HBBIHHBBBBBB2x6sIBBH'
RADIO_CONFIG_LEN = struct.calcsize(RADIO_CONFIG_FMT)
RADIO_CONFIG_SCHEMA = 1
RADIO_CAPS = {0:'RADIO_CONFIG',1:'AUTO_DISCOVERY',2:'CHANNEL_SCAN',3:'SENSOR_CONFIG',4:'OTA',5:'LEGACY_PROTO2'}


INTERFACES = {
    0: 'NONE', 1: 'GPIO', 2: 'ADC', 3: 'I2C', 4: '1-Wire', 5: 'DHT', 6: 'Modbus', 7: 'Pulse'
}
DRIVERS = {
    0: 'NONE', 1: 'GPIO_INPUT', 2: 'ADC_RAW', 3: 'ADC_LINEAR', 4: '4_20MA', 5: '0_10V',
    10: 'DHT11', 11: 'DHT22', 12: 'DS18B20', 20: 'SHT3X', 21: 'BME280', 22: 'BH1750',
    23: 'SCD4X', 24: 'ADS1115', 30: 'I2C_GENERIC', 40: 'MODBUS_GENERIC', 50: 'PULSE_COUNTER'
}
MEASUREMENTS = {
    0: 'NONE', 1: 'Temperature', 2: 'Humidity', 3: 'Pressure', 10: 'CO2', 11: 'Light lux',
    20: 'Soil moisture', 21: 'Soil temperature', 22: 'Soil EC', 23: 'Soil pH', 30: 'Nitrogen',
    31: 'Phosphorus', 32: 'Potassium', 40: 'Water level', 41: 'Water flow', 42: 'Water pH',
    43: 'Water EC', 44: 'Dissolved oxygen', 45: 'ORP', 50: 'Wind speed', 51: 'Wind direction',
    52: 'Rainfall', 60: 'Digital state', 0x7FFF: 'RAW'
}
PINS = {
    0: 'NONE', 1: 'PA0', 2: 'PA1', 5: 'PA4', 6: 'PA5', 7: 'PA6', 8: 'PA7',
    9: 'PA8', 12: 'PA11', 13: 'PA12', 21: 'PB5', 22: 'PB6', 23: 'PB7', 24: 'PB8', 25: 'PB9',
    29: 'PB13', 30: 'PB14', 31: 'PB15'
}
PIN_CAPS = {
    0: 'GPIO_IN', 1: 'GPIO_OUT', 2: 'ADC', 3: 'I2C', 4: 'ONEWIRE', 5: 'DHT', 6: 'PULSE', 31: 'RESERVED'
}

DRIVER_DEFAULTS = {
    1: dict(interface_type=1, measurement_id=60, pin_id=29, sample_interval_ms=1000, report_interval_ms=1000),
    2: dict(interface_type=2, measurement_id=0x7FFF, pin_id=1, sample_interval_ms=1000, report_interval_ms=1000),
    3: dict(interface_type=2, measurement_id=0x7FFF, pin_id=1, sample_interval_ms=1000, report_interval_ms=1000),
    10: dict(interface_type=5, measurement_id=1, pin_id=21, sample_interval_ms=2000, report_interval_ms=2000),
    11: dict(interface_type=5, measurement_id=1, pin_id=21, sample_interval_ms=2000, report_interval_ms=2000),
    12: dict(interface_type=4, measurement_id=1, pin_id=21, sample_interval_ms=2000, report_interval_ms=2000),
    20: dict(interface_type=3, measurement_id=0, pin_id=0, address=0x44, sample_interval_ms=2000, report_interval_ms=2000),
    21: dict(interface_type=3, measurement_id=0, pin_id=0, address=0x76, sample_interval_ms=2000, report_interval_ms=2000),
    22: dict(interface_type=3, measurement_id=11, pin_id=0, address=0x23, sample_interval_ms=2000, report_interval_ms=2000),
    23: dict(interface_type=3, measurement_id=0, pin_id=0, address=0x62, sample_interval_ms=5000, report_interval_ms=5000),
    24: dict(interface_type=3, measurement_id=0x7FFF, pin_id=0, address=0x48, sample_interval_ms=1000, report_interval_ms=1000),
    30: dict(interface_type=3, measurement_id=0x7FFF, pin_id=0, address=0x00, sample_interval_ms=1000, report_interval_ms=1000),
    40: dict(interface_type=6, measurement_id=0x7FFF, pin_id=0, address=1, sample_interval_ms=1000, report_interval_ms=1000),
    50: dict(interface_type=7, measurement_id=41, pin_id=29, sample_interval_ms=1000, report_interval_ms=1000),
}


def crc8(data):
    c = 0xFF
    for x in data:
        c ^= x
        for _ in range(8):
            c = ((c << 1) ^ 0x07) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return c


def crc16(data):
    c = 0xFFFF
    for x in data:
        c ^= x << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def frame(seq, cmd, payload=b''):
    h = bytearray([SA_SOF]) + struct.pack('<H', len(payload)) + bytes([seq & 255])
    h.append(crc8(h))
    x = h + struct.pack('<H', cmd) + payload
    x += struct.pack('<H', crc16(x))
    return bytes(x)


def route(src, dst, group, cmd, payload=b'', flags=0, priority=1):
    h = struct.pack(ROUTE_FMT, SA_ROUTE_VERSION, flags, priority, 0, src, dst, group, cmd, len(payload), 0)
    return h + payload


def unpack_route(data):
    if len(data) < ROUTE_LEN:
        return None
    v, fl, pr, _r, src, dst, g, cmd, n, _r2 = struct.unpack_from(ROUTE_FMT, data)
    if v != SA_ROUTE_VERSION or len(data) != ROUTE_LEN + n:
        return None
    return {'version': v, 'flags': fl, 'priority': pr, 'src': src, 'dst': dst, 'group': g,
            'cmd': cmd, 'cmd_name': CMD_NAMES.get(cmd, f'0x{cmd:04X}'), 'payload': data[ROUTE_LEN:]}


class FrameParser:
    def __init__(self):
        self.b = bytearray()

    def feed(self, d):
        self.b += d
        out = []
        while True:
            while self.b and self.b[0] != SA_SOF:
                self.b.pop(0)
            if len(self.b) < 9:
                break
            if crc8(self.b[:4]) != self.b[4]:
                self.b.pop(0)
                continue
            n = struct.unpack_from('<H', self.b, 1)[0]
            total = 9 + n
            if n > 512:
                self.b.pop(0)
                continue
            if len(self.b) < total:
                break
            raw = bytes(self.b[:total])
            del self.b[:total]
            if crc16(raw[:-2]) != struct.unpack_from('<H', raw, total - 2)[0]:
                continue
            cmd = struct.unpack_from('<H', raw, 5)[0]
            out.append((raw[3], cmd, raw[7:-2]))
        return out


def q16(v):
    return int(round(float(v) * 65536))


def uq16(v):
    return float(v) / 65536.0


def pack_slot(s):
    return struct.pack(
        SLOT_FMT,
        int(s.get('slot_id', 0)), 1 if s.get('enabled', True) else 0,
        int(s.get('driver_id', 0)), int(s.get('measurement_id', 0)),
        int(s.get('interface_type', 0)), int(s.get('bus_id', 0)),
        int(s.get('pin_id', 0)), int(s.get('address', 0)), int(s.get('reg', 0)),
        int(s.get('sample_interval_ms', 1000)), int(s.get('report_interval_ms', 1000)),
        q16(s.get('scale', 1.0)), q16(s.get('offset', 0)), q16(s.get('threshold', 0)),
        int(s.get('flags', 0)), int(s.get('param0', 0)), int(s.get('param1', 0))
    )


def unpack_slot(p, off=0):
    if len(p) < off + SLOT_LEN:
        return None
    x = struct.unpack_from(SLOT_FMT, p, off)
    return {
        'slot_id': x[0], 'enabled': bool(x[1]), 'driver_id': x[2], 'driver_name': DRIVERS.get(x[2], str(x[2])),
        'measurement_id': x[3], 'measurement_name': MEASUREMENTS.get(x[3], str(x[3])),
        'interface_type': x[4], 'interface_name': INTERFACES.get(x[4], str(x[4])), 'bus_id': x[5],
        'pin_id': x[6], 'pin_name': PINS.get(x[6], str(x[6])), 'address': x[7], 'reg': x[8],
        'sample_interval_ms': x[9], 'report_interval_ms': x[10], 'scale': uq16(x[11]),
        'offset': uq16(x[12]), 'threshold': uq16(x[13]), 'flags': x[14], 'param0': x[15], 'param1': x[16]
    }


def decode_node_config(p):
    if len(p) < NODE_CONFIG_HEAD_LEN:
        return None
    schema, role, _r, dev, net, grp, count = struct.unpack_from(NODE_CONFIG_HEAD_FMT, p, 0)
    slots = []
    off = NODE_CONFIG_HEAD_LEN
    for _ in range(8):
        sl = unpack_slot(p, off)
        if not sl:
            break
        slots.append(sl)
        off += SLOT_LEN
    return {'schema_version': schema, 'role': role, 'device_id': dev, 'network_id': net,
            'group_id': grp, 'sensor_count': count, 'slots': slots}


def decode_telemetry(p):
    if len(p) != 40:
        return None
    slot, count, drv, ts = struct.unpack_from('<BBHI', p, 0)
    vals = []
    off = 8
    for _ in range(min(count, 4)):
        mid, v, q, _ = struct.unpack_from('<HiBB', p, off)
        off += 8
        vals.append({'measurement_id': mid, 'measurement_name': MEASUREMENTS.get(mid, str(mid)),
                     'value': uq16(v), 'quality': q})
    return {'slot_id': slot, 'driver_id': drv, 'driver_name': DRIVERS.get(drv, str(drv)),
            'timestamp_ms': ts, 'values': vals}


def decode_node_info(p):
    if len(p) < 16:
        return None
    role = p[0]
    dev = struct.unpack_from('<I', p, 4)[0]
    net, grp = struct.unpack_from('<HH', p, 8)
    gen = struct.unpack_from('<I', p, 12)[0]
    return {'role': role, 'role_name': {1: 'MASTER', 2: 'SLAVE'}.get(role, 'UNCONFIGURED'),
            'device_id': dev, 'network_id': net, 'group_id': grp, 'generation': gen}


def decode_result(p):
    if len(p) != 8:
        return None
    cmd, res, detail = struct.unpack('<HhI', p)
    return {'command': cmd, 'command_name': CMD_NAMES.get(cmd, f'0x{cmd:04X}'), 'result': res, 'detail': detail}


def pack_diag_telemetry(slot_id, measurement_id, value, quality=100, driver_id=0xFFFE):
    return struct.pack(DIAG_TELEM_FMT, int(slot_id) & 0xFF, int(quality) & 0xFF,
                       int(driver_id) & 0xFFFF, int(measurement_id) & 0xFFFF, 0, q16(value))


def caps_to_names(caps):
    return [name for bit, name in PIN_CAPS.items() if caps & (1 << bit)]


def schema_dict():
    return {
        'interfaces': [{'id': k, 'name': v} for k, v in INTERFACES.items()],
        'drivers': [{'id': k, 'name': v, 'defaults': DRIVER_DEFAULTS.get(k, {})} for k, v in DRIVERS.items()],
        'measurements': [{'id': k, 'name': v} for k, v in MEASUREMENTS.items()],
        'pins': [{'id': k, 'name': v} for k, v in PINS.items()],
        'pin_capability_bits': PIN_CAPS,
    }


def radio_caps_to_names(caps):
    return [name for bit, name in RADIO_CAPS.items() if int(caps) & (1 << bit)]


def decode_radio_config(p):
    if len(p) != RADIO_CONFIG_LEN:
        return None
    schema, role, serial_mode, dev, net, grp, ch, power, uplink, auto, scan, gw_valid, gw, fw, pmin, pmax, caps = struct.unpack(RADIO_CONFIG_FMT, p)
    return {
        'schema_version': schema,
        'role': role, 'role_name': {0:'UNCONFIGURED',1:'MASTER',2:'SLAVE'}.get(role, str(role)),
        'serial_mode': serial_mode, 'device_id': dev, 'network_id': net, 'group_id': grp,
        'channel': ch, 'power_mode': power, 'uplink': uplink,
        'auto_discovery': bool(auto), 'channel_scan': bool(scan), 'gateway_valid': bool(gw_valid),
        'gateway_mac': ':'.join(f'{x:02X}' for x in gw) if gw_valid else '',
        'fw_version': fw, 'fw_version_text': f'{(fw>>16)&0xff}.{(fw>>8)&0xff}.{fw&0xff}',
        'proto_min': pmin, 'proto_max': pmax, 'capabilities': caps, 'capability_names': radio_caps_to_names(caps),
    }


def _mac_bytes(text):
    if not text:
        return bytes(6)
    parts = str(text).replace('-', ':').split(':')
    if len(parts) != 6:
        raise ValueError('gateway_mac must be XX:XX:XX:XX:XX:XX')
    out = bytes(int(x, 16) for x in parts)
    if len(out) != 6:
        raise ValueError('invalid gateway_mac')
    return out


def pack_radio_config(c):
    role = c.get('role', c.get('role_name', 2))
    if isinstance(role, str):
        role = {'UNCONFIGURED':0,'MASTER':1,'SLAVE':2}.get(role.upper(), -1)
    gw_valid = bool(c.get('gateway_valid', bool(c.get('gateway_mac'))))
    gw = _mac_bytes(c.get('gateway_mac', '')) if gw_valid else bytes(6)
    return struct.pack(
        RADIO_CONFIG_FMT,
        RADIO_CONFIG_SCHEMA, int(role), 1, int(c.get('device_id', 1001)),
        int(c.get('network_id', 1)), int(c.get('group_id', 1)), int(c.get('channel', 6)),
        int(c.get('power_mode', 1)), int(c.get('uplink', 0)),
        1 if c.get('auto_discovery', True) else 0, 1 if c.get('channel_scan', True) else 0,
        1 if gw_valid else 0, gw, 0, 2, 2, 0
    )


def pack_ota_info_get(component):
    return struct.pack('<B3x', int(component) & 0xFF)

def decode_ota_info(p):
    if len(p) != struct.calcsize(OTA_INFO_FMT):
        return None
    comp, state, target, fw, max_size, flags = struct.unpack(OTA_INFO_FMT, p)
    return {'component': comp, 'state': state, 'target': target, 'fw_version': fw, 'max_image_size': max_size, 'flags': flags}

def pack_ota_prepare(component, transfer_id):
    return struct.pack('<B3xI', int(component) & 0xFF, int(transfer_id) & 0xFFFFFFFF)

def pack_ota_begin(component, target, transfer_id, image_size, crc32):
    return struct.pack(OTA_BEGIN_FMT, int(component)&0xFF, 0, int(target)&0xFFFF, int(transfer_id)&0xFFFFFFFF, int(image_size)&0xFFFFFFFF, int(crc32)&0xFFFFFFFF)

def pack_ota_data(component, transfer_id, offset, data):
    data = bytes(data)
    if len(data) > 440:
        raise ValueError('OTA chunk too large')
    return struct.pack(OTA_DATA_HEAD_FMT, int(component)&0xFF, 0, len(data), int(transfer_id)&0xFFFFFFFF, int(offset)&0xFFFFFFFF) + data

def pack_ota_end(component, transfer_id, crc32):
    return struct.pack(OTA_END_FMT, int(component)&0xFF, 0, 0, int(transfer_id)&0xFFFFFFFF, int(crc32)&0xFFFFFFFF)

def pack_ota_abort(component, transfer_id):
    return struct.pack('<B3xI', int(component)&0xFF, int(transfer_id)&0xFFFFFFFF)

def pack_ota_status(component, transfer_id=0):
    return struct.pack('<B3xI', int(component)&0xFF, int(transfer_id)&0xFFFFFFFF)

def decode_ota_result(p):
    if len(p) != struct.calcsize(OTA_RESULT_FMT):
        return None
    comp, state, result, transfer_id, next_offset, detail = struct.unpack(OTA_RESULT_FMT, p)
    return {'component': comp, 'state': state, 'result': result, 'transfer_id': transfer_id, 'next_offset': next_offset, 'detail': detail}

def ota_component_name(c):
    return {OTA_COMPONENT_STM32:'STM32', OTA_COMPONENT_ESP:'ESP32'}.get(int(c), str(c))

def ota_target_name(t):
    return {OTA_TARGET_STM32F103C8:'STM32F103C8', OTA_TARGET_ESP32:'ESP32', OTA_TARGET_ESP32C3:'ESP32-C3'}.get(int(t), f'0x{int(t):04X}')
