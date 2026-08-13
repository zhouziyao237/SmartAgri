#!/usr/bin/env python3
import argparse
import copy
import json
import os
import struct
import threading
import time
import webbrowser
import uuid
import zlib
from urllib.parse import urlparse, parse_qs
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError as e:
    raise SystemExit('pyserial is required. Run install_pc_tools.bat or pip install pyserial') from e

from smartagri_protocol import *

BASE = Path(__file__).resolve().parent
DATA_DIR = BASE / 'local_data'
SETTINGS_FILE = DATA_DIR / 'settings.json'
PROFILES_FILE = DATA_DIR / 'profiles.json'
PROFILE_SCHEMA_VERSION = 2

DEFAULT_SETTINGS = {
    'preferred_port': '',
    'baud': 115200,
    'scan_interval_sec': 2.0,
    'master_probe_interval_sec': 3.0,
    'node_offline_timeout_sec': 15.0,
    'node_hide_timeout_sec': 60.0,
    'event_limit': 500,
    'history_limit': 300,
    'auto_open_browser': True,
    'show_offline_nodes': False,
}


def _load_json(path, default):
    try:
        if path.exists():
            with path.open('r', encoding='utf-8') as f:
                x = json.load(f)
            if isinstance(default, dict) and isinstance(x, dict):
                y = dict(default); y.update(x); return y
            return x
    except Exception as e:
        print(f'[WARN] cannot load {path}: {e}')
    return copy.deepcopy(default)


def _save_json(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + '.tmp')
    with tmp.open('w', encoding='utf-8') as f:
        json.dump(obj, f, ensure_ascii=False, indent=2)
    tmp.replace(path)


def _normalize_profile(name, raw):
    """Migrate V2.3.0 slot-only profiles to the extensible V2 profile schema."""
    raw = raw if isinstance(raw, dict) else {}
    target = raw.get('target') if isinstance(raw.get('target'), dict) else None
    if not target:
        legacy_dev = raw.get('target_device_id', raw.get('device_id'))
        target = {'mode': 'node', 'device_id': int(legacy_dev)} if legacy_dev not in (None, '', 0, '0') else {'mode': 'generic', 'device_id': None}
    mode = str(target.get('mode', 'generic'))
    dev = target.get('device_id')
    if mode != 'node' or dev in (None, '', 0, '0'):
        target = {'mode': 'generic', 'device_id': None}
    else:
        target = {'mode': 'node', 'device_id': int(dev)}
    return {
        'schema_version': PROFILE_SCHEMA_VERSION,
        'name': str(raw.get('name') or name),
        'object_type': str(raw.get('object_type') or 'sensor_slot'),
        'target': target,
        'slots': list(raw.get('slots') or []),
        'description': str(raw.get('description') or ''),
        'updated_at': float(raw.get('updated_at') or time.time()),
    }


def _normalize_profiles(raw):
    if not isinstance(raw, dict):
        return {}
    return {str(name): _normalize_profile(name, value) for name, value in raw.items()}


class State:
    def __init__(self):
        self.lock = threading.RLock()
        self.cv = threading.Condition(self.lock)
        self.ser = None
        self.port = None
        self.seq = 0
        self.nodes = {}
        self.events = []
        self.stop = False
        self.settings = _load_json(SETTINGS_FILE, DEFAULT_SETTINGS)
        self.profiles = _normalize_profiles(_load_json(PROFILES_FILE, {}))
        self.result_serial = 0
        self.ota_serial = 0
        self.ota_jobs = {}
        self.serial_scan = {'state': 'starting', 'ports': [], 'current_port': None, 'errors': {}, 'last_scan': 0.0}

    def event(self, x, level='info'):
        item = {'time': time.strftime('%H:%M:%S'), 'ts': time.time(), 'level': level, 'event': x}
        with self.lock:
            self.events.append(item)
            limit = max(50, int(self.settings.get('event_limit', 500)))
            self.events = self.events[-limit:]
            self.cv.notify_all()
        print(f"[{item['time']}] {x}")

    def connected(self):
        with self.lock:
            return self.ser is not None and getattr(self.ser, 'is_open', False)

    def attach(self, ser_obj, port):
        with self.lock:
            self.ser = ser_obj
            self.port = port
            self.cv.notify_all()

    def detach(self, ser_obj=None):
        with self.lock:
            if ser_obj is not None and self.ser is not ser_obj:
                return
            old = self.ser
            self.ser = None
            self.port = None
            self.cv.notify_all()
        if old:
            try: old.close()
            except Exception: pass

    def send_route(self, dst, cmd, payload=b'', group=1):
        with self.lock:
            if not self.ser or not self.ser.is_open:
                raise RuntimeError('MASTER STM32 PC link not connected')
            r = route(SA_DEVICE_PC, int(dst), int(group), int(cmd), payload)
            f = frame(self.seq, SA_CMD_ROUTE, r)
            self.seq = (self.seq + 1) & 0xFF
            self.ser.write(f)
            self.ser.flush()

    def node(self, device_id):
        key = str(int(device_id))
        n = self.nodes.setdefault(key, {
            'device_id': int(device_id), 'last_seen': 0.0, 'first_seen': time.time(),
            'telemetry': {}, 'telemetry_history': [], 'results': [], 'caps': None, 'config': None, 'radio_config': None,
            'ota_info': {}, 'ota_results': [],
            '_caps_rev': 0, '_config_rev': 0, '_radio_rev': 0, '_ota_info_rev': 0,
        })
        return n

    def wait_result(self, device_id, command, after_serial, timeout=2.5):
        deadline = time.time() + timeout
        with self.cv:
            while time.time() < deadline:
                n = self.nodes.get(str(int(device_id)))
                if n:
                    for r in reversed(n.get('results', [])):
                        if r.get('_serial', 0) > after_serial and r.get('command') == int(command):
                            return copy.deepcopy(r)
                self.cv.wait(timeout=max(0.02, min(0.2, deadline - time.time())))
        return None

    def command_wait(self, device_id, command, payload=b'', group=1, timeout=2.5):
        with self.lock:
            mark = self.result_serial
        self.send_route(device_id, command, payload, group)
        return self.wait_result(device_id, command, mark, timeout)

    def wait_ota_result(self, device_id, transfer_id, after_serial, component=None, timeout=3.0):
        deadline = time.time() + timeout
        with self.cv:
            while time.time() < deadline:
                n = self.nodes.get(str(int(device_id)))
                if n:
                    for r in reversed(n.get('ota_results', [])):
                        if r.get('_serial', 0) <= after_serial:
                            continue
                        if int(r.get('transfer_id', -1)) != int(transfer_id):
                            continue
                        if component is not None and int(r.get('component', -1)) != int(component):
                            continue
                        return copy.deepcopy(r)
                self.cv.wait(timeout=max(0.02, min(0.2, deadline - time.time())))
        return None

    def ota_command_wait(self, device_id, command, payload, transfer_id, component, group=1, timeout=3.0):
        with self.lock:
            mark = self.ota_serial
        self.send_route(device_id, command, payload, group)
        return self.wait_ota_result(device_id, transfer_id, mark, component, timeout)

    def ota_job_update(self, job_id, **fields):
        with self.lock:
            job = self.ota_jobs.get(job_id)
            if not job:
                return
            job.update(fields)
            job['updated_at'] = time.time()
            self.cv.notify_all()

    def snapshot(self):
        with self.lock:
            now = time.time()
            offline = float(self.settings.get('node_offline_timeout_sec', 15.0))
            hide = float(self.settings.get('node_hide_timeout_sec', 60.0))
            nodes = copy.deepcopy(self.nodes)
            for n in nodes.values():
                age = now - float(n.get('last_seen') or 0)
                n['age_sec'] = round(age, 1) if n.get('last_seen') else None
                n['online'] = bool(n.get('last_seen') and age < offline)
                n['hidden_by_age'] = bool(n.get('last_seen') and age >= hide)
                for k in list(n):
                    if k.startswith('_'): n.pop(k, None)
            return {
                'connected': self.ser is not None and getattr(self.ser, 'is_open', False),
                'transport': 'USB-Serial / MASTER STM32 USART2',
                'master': self.port,
                'baud': int(self.settings.get('baud', 115200)),
                'nodes': nodes,
                'events': copy.deepcopy(self.events[-100:]),
                'settings': copy.deepcopy(self.settings),
                'ota_jobs': copy.deepcopy(self.ota_jobs),
                'serial_scan': copy.deepcopy(self.serial_scan),
                'server_time': time.time(),
            }

S = State()


def handle_route(r):
    src, cmd, p = r['src'], r['cmd'], r['payload']
    with S.lock:
        n = S.node(src)
        n['last_seen'] = time.time()
        if cmd in (CMD_HEARTBEAT, CMD_NODE_INFO):
            x = decode_node_info(p)
            if x: n.update(x)
        elif cmd == CMD_TELEMETRY:
            x = decode_telemetry(p)
            if x:
                n['telemetry'][str(x['slot_id'])] = x
                hist = {'time': time.time(), **x}
                n['telemetry_history'].append(hist)
                n['telemetry_history'] = n['telemetry_history'][-max(20, int(S.settings.get('history_limit', 300))):]
        elif cmd == CMD_CONTROL_RESULT:
            x = decode_result(p)
            if x:
                S.result_serial += 1
                x['_serial'] = S.result_serial
                x['time'] = time.time()
                n['results'] = (n.get('results', []) + [x])[-80:]
        elif cmd == CMD_OTA_INFO_REPLY:
            x = decode_ota_info(p)
            if x:
                x['component_name'] = ota_component_name(x['component'])
                x['target_name'] = ota_target_name(x['target'])
                x['updated_at'] = time.time()
                n['ota_info'][str(x['component'])] = x
                n['_ota_info_rev'] = n.get('_ota_info_rev', 0) + 1
        elif cmd == CMD_OTA_RESULT:
            x = decode_ota_result(p)
            if x:
                S.ota_serial += 1
                x['_serial'] = S.ota_serial
                x['time'] = time.time()
                x['component_name'] = ota_component_name(x['component'])
                n['ota_results'] = (n.get('ota_results', []) + [x])[-120:]
        elif cmd == CMD_CAPS_REPLY:
            if len(p) >= 16:
                board, fw, maxslots, pinc, lo, hi = struct.unpack_from('<IHBBII', p, 0)
                pins = []; off = 16
                for _ in range(pinc):
                    if off + 6 > len(p): break
                    pid, caps = struct.unpack_from('<HI', p, off); off += 6
                    pins.append({'pin_id': pid, 'pin_name': PINS.get(pid, str(pid)), 'caps': caps,
                                 'cap_names': caps_to_names(caps)})
                n['caps'] = {'board_id': board, 'fw_version': fw, 'max_slots': maxslots,
                             'driver_mask_lo': lo, 'driver_mask_hi': hi, 'pins': pins,
                             'updated_at': time.time()}
                n['_caps_rev'] = n.get('_caps_rev', 0) + 1
        elif cmd == CMD_CONFIG_REPLY:
            x = decode_node_config(p)
            if x:
                x['updated_at'] = time.time()
                n['config'] = x
                n['_config_rev'] = n.get('_config_rev', 0) + 1
        elif cmd == CMD_RADIO_CONFIG_REPLY:
            x = decode_radio_config(p)
            if x:
                x['updated_at'] = time.time()
                n['radio_config'] = x
                n['_radio_rev'] = n.get('_radio_rev', 0) + 1
        S.cv.notify_all()
    S.event({'src': src, 'cmd': f'0x{cmd:04X}', 'name': CMD_NAMES.get(cmd, 'UNKNOWN'), 'len': len(p)})


def master_identity_from_route(r):
    if not r or r['src'] != SA_DEVICE_MASTER or r['cmd'] not in (CMD_HEARTBEAT, CMD_NODE_INFO):
        return False
    x = decode_node_info(r['payload'])
    return bool(x and x.get('role') == 1 and x.get('device_id') == SA_DEVICE_MASTER)


def make_probe(seq=0):
    return frame(seq, SA_CMD_ROUTE, route(SA_DEVICE_PC, SA_DEVICE_MASTER, 1, CMD_HEARTBEAT, b''))


def probe_port(port, baud, timeout=1.3):
    ser_obj = None
    try:
        ser_obj = serial.Serial(port, baudrate=baud, bytesize=8, parity=serial.PARITY_NONE,
                                stopbits=1, timeout=0.05, write_timeout=0.5)
        time.sleep(0.08)
        parser = FrameParser(); deadline = time.time() + timeout; next_probe = 0.0; seq = 0
        while time.time() < deadline:
            now = time.time()
            if now >= next_probe:
                ser_obj.write(make_probe(seq)); ser_obj.flush(); seq = (seq + 1) & 0xFF; next_probe = now + 0.30
            d = ser_obj.read(ser_obj.in_waiting or 1)
            if not d: continue
            for _seq, cmd, pay in parser.feed(d):
                if cmd == SA_CMD_ROUTE and master_identity_from_route(unpack_route(pay)):
                    return ser_obj, None
        ser_obj.close()
        return None, 'opened, but no MASTER STM32 A5 identity reply'
    except Exception as e:
        if ser_obj:
            try: ser_obj.close()
            except Exception: pass
        return None, f'{type(e).__name__}: {e}'


def serial_worker(cli_port=None, cli_baud=None):
    while not S.stop:
        if S.connected(): time.sleep(0.2); continue
        with S.lock:
            pref = cli_port if cli_port is not None else str(S.settings.get('preferred_port', '') or '')
            baud = int(cli_baud if cli_baud is not None else S.settings.get('baud', 115200))
            scan_interval = float(S.settings.get('scan_interval_sec', 2.0))
        ports = [pref] if pref else [p.device for p in list_ports.comports()]
        with S.lock:
            S.serial_scan = {'state': 'scanning', 'ports': list(ports), 'current_port': None, 'errors': {}, 'last_scan': time.time()}
        if not ports:
            with S.lock: S.serial_scan['state'] = 'no_ports'
            S.event('No serial ports found; waiting...', 'warning'); time.sleep(scan_interval); continue
        S.event(f'Scanning SmartAgri MASTER on {ports}')
        found = found_port = None
        for port in ports:
            if S.stop: return
            with S.lock:
                S.serial_scan['state'] = 'probing'; S.serial_scan['current_port'] = port
            obj, err = probe_port(port, baud)
            if obj:
                found, found_port = obj, port; break
            with S.lock: S.serial_scan['errors'][port] = err or 'probe failed'
        if not found:
            with S.lock: S.serial_scan['state'] = 'waiting'; S.serial_scan['current_port'] = None
            time.sleep(scan_interval); continue
        S.attach(found, found_port)
        with S.lock:
            S.serial_scan['state'] = 'connected'; S.serial_scan['current_port'] = found_port; S.serial_scan['errors'] = {}
        S.event(f'MASTER STM32 connected: {found_port} @ {baud}')
        parser = FrameParser(); last_probe = 0.0
        try:
            while not S.stop and found.is_open:
                now = time.time(); probe_interval = float(S.settings.get('master_probe_interval_sec', 3.0))
                if now - last_probe >= probe_interval:
                    with S.lock: seq = S.seq; S.seq = (S.seq + 1) & 0xFF
                    found.write(make_probe(seq)); found.flush(); last_probe = now
                d = found.read(found.in_waiting or 1)
                if not d: continue
                for _seq, cmd, pay in parser.feed(d):
                    if cmd != SA_CMD_ROUTE: continue
                    r = unpack_route(pay)
                    if r: handle_route(r)
        except Exception as e:
            S.event(f'Serial link lost on {found_port}: {e}', 'error')
        finally:
            S.detach(found)
            with S.lock:
                S.serial_scan['state'] = 'disconnected'; S.serial_scan['current_port'] = None
            S.event(f'MASTER STM32 disconnected: {found_port}', 'warning'); time.sleep(scan_interval)


def wait_node_field(dev, field, old_rev, timeout=2.5):
    deadline = time.time() + timeout
    with S.cv:
        while time.time() < deadline:
            n = S.nodes.get(str(dev))
            if n and n.get(field) is not None and n.get('_' + field.replace('caps','caps').replace('config','config') + '_rev', 0) > old_rev:
                return copy.deepcopy(n.get(field))
            S.cv.wait(timeout=max(0.02, min(0.2, deadline - time.time())))
    return None


def configure_transaction(dev, slots, group=1, timeout=2.5):
    out = []
    steps = [(CMD_CONFIG_BEGIN, b'')]
    steps += [(CMD_CONFIG_SENSOR_SET, pack_slot(sl)) for sl in slots]
    steps += [(CMD_CONFIG_VALIDATE, b''), (CMD_CONFIG_COMMIT, b'')]
    for cmd, payload in steps:
        r = S.command_wait(dev, cmd, payload, group, timeout)
        out.append({'command': cmd, 'name': CMD_NAMES.get(cmd), 'response': r})
        if not r:
            return {'ok': False, 'error': f'timeout waiting {CMD_NAMES.get(cmd)}', 'steps': out}
        if int(r.get('result', -999)) != 0:
            try: S.command_wait(dev, CMD_CONFIG_ROLLBACK, b'', group, timeout=1.0)
            except Exception: pass
            return {'ok': False, 'error': f"{CMD_NAMES.get(cmd)} result={r.get('result')} detail={r.get('detail')}", 'steps': out}
    return {'ok': True, 'steps': out}


def request_node_config(dev, group=1, timeout=2.5):
    dev = int(dev)
    with S.lock:
        old = S.node(dev).get('_config_rev', 0)
    S.send_route(dev, CMD_CONFIG_GET, b'', group)
    deadline = time.time() + timeout
    with S.cv:
        while time.time() < deadline:
            n = S.nodes.get(str(dev))
            if n and n.get('_config_rev', 0) > old and n.get('config') is not None:
                return copy.deepcopy(n.get('config'))
            S.cv.wait(0.08)
    return None


def request_radio_config(dev, group=1, timeout=2.8):
    dev = int(dev)
    with S.lock:
        old = S.node(dev).get('_radio_rev', 0)
    S.send_route(dev, CMD_RADIO_CONFIG_GET, b'', group)
    deadline = time.time() + timeout
    with S.cv:
        while time.time() < deadline:
            n = S.nodes.get(str(dev))
            if n and n.get('_radio_rev', 0) > old and n.get('radio_config') is not None:
                return copy.deepcopy(n.get('radio_config'))
            S.cv.wait(0.08)
    return None


def export_node_config(dev, group=None):
    dev = int(dev); group = node_group_id(dev, 1) if group is None else int(group)
    node_cfg = request_node_config(dev, group, 2.8)
    radio_cfg = request_radio_config(dev, group, 2.8)
    warnings = []
    if node_cfg is None: warnings.append('STM32 CONFIG_GET unsupported or timed out')
    if radio_cfg is None: warnings.append('ESP RADIO_CONFIG_GET unsupported; V2.4.2-V2.4.6 nodes remain route-compatible but radio settings may require AT until upgraded')
    return {
        'schema': 'smartagri-node-bundle', 'schema_version': 1, 'exported_at': time.time(),
        'device_id': dev, 'group_id': group, 'radio': radio_cfg, 'stm32': node_cfg,
        'compatibility': {'protocol': 2, 'warnings': warnings}
    }


def import_node_config(bundle, target_device=None, group=None):
    if not isinstance(bundle, dict):
        raise ValueError('configuration bundle must be a JSON object')
    dev = int(target_device or bundle.get('device_id') or 0)
    if not dev: raise ValueError('target device_id required')
    group = int(group if group is not None else bundle.get('group_id', node_group_id(dev, 1)))
    result = {'ok': True, 'device_id': dev, 'group_id': group, 'stm32': None, 'radio': None}
    stm = bundle.get('stm32')
    if isinstance(stm, dict) and isinstance(stm.get('slots'), list):
        result['stm32'] = configure_transaction(dev, stm.get('slots', []), group)
        if not result['stm32'].get('ok'):
            result['ok'] = False
            return result
    radio = bundle.get('radio')
    if isinstance(radio, dict):
        payload = pack_radio_config(radio)
        rr = S.command_wait(dev, CMD_RADIO_CONFIG_SET, payload, group, timeout=3.2)
        result['radio'] = rr
        if not rr or int(rr.get('result', -999)) != 0:
            result['ok'] = False
    return result


def request_ota_info(dev, component, group=1, timeout=2.5):
    dev = int(dev); component = int(component)
    with S.lock:
        n = S.node(dev); old = n.get('_ota_info_rev', 0)
    S.send_route(dev, CMD_OTA_INFO_GET, pack_ota_info_get(component), group)
    deadline = time.time() + timeout
    with S.cv:
        while time.time() < deadline:
            n = S.nodes.get(str(dev))
            if n and n.get('_ota_info_rev', 0) > old:
                info = (n.get('ota_info') or {}).get(str(component))
                if info:
                    return copy.deepcopy(info)
            S.cv.wait(0.08)
    return None


def node_group_id(dev, fallback=1):
    """Use the node's last advertised logical group instead of hard-coding group 1.
    Group is route metadata today, but preserving it makes OTA/config traffic
    correct when the installation later uses multiple logical groups.
    """
    with S.lock:
        n = S.nodes.get(str(int(dev)))
        if n and n.get('group_id') is not None:
            try:
                return int(n.get('group_id'))
            except Exception:
                pass
    return int(fallback)


def wait_stm32_bootloader_ready(dev, transfer_id, component, group, timeout=6.0):
    """After OTA_PREPARE the App resets. Poll STATUS until the relocated
    bootloader answers with the same transfer/session id. This removes the old
    fixed 0.9 s race and gives a precise failure point before OTA_BEGIN.
    """
    deadline = time.time() + float(timeout)
    last = None
    payload = pack_ota_status(component, transfer_id)
    while time.time() < deadline:
        last = S.ota_command_wait(dev, CMD_OTA_STATUS, payload, transfer_id, component, group, timeout=0.55)
        if last and int(last.get('result', -999)) == 0 and int(last.get('transfer_id', 0)) == int(transfer_id):
            return last
        time.sleep(0.10)
    return last


def wait_component_reappear(dev, component, group, timeout=10.0):
    deadline = time.time() + float(timeout)
    while time.time() < deadline:
        try:
            info = request_ota_info(dev, component, group, timeout=min(1.2, max(0.25, deadline-time.time())))
            if info:
                return info
        except Exception:
            pass
        time.sleep(0.25)
    return None

def _ota_retry(dev, cmd, payload, transfer_id, component, group, timeout=3.0, retries=4, accept_next=None):
    last = None
    for _ in range(retries):
        last = S.ota_command_wait(dev, cmd, payload, transfer_id, component, group, timeout)
        if last:
            if int(last.get('result', -999)) == 0:
                return last
            if accept_next is not None and int(last.get('next_offset', -1)) == int(accept_next):
                return last
            # Negative replies are deterministic except an offset reply that tells us the remote already advanced.
            if int(last.get('result', -999)) not in (-11, -12, -20):
                return last
        time.sleep(0.08)
    return last

def validate_ota_image(image, component):
    image = bytes(image)
    if len(image) < 16:
        raise ValueError('firmware image is too small')
    if int(component) == OTA_COMPONENT_STM32:
        sp, rv = struct.unpack_from('<II', image, 0)
        reset = rv & ~1
        if not (0x20000000 <= sp <= 0x20005000 and (rv & 1) and 0x08002000 <= reset < 0x0800F800):
            raise ValueError('STM32 image vector is not a V2.4 OTA App. Build/upload relocated smart_agri_comm.bin (linked at 0x08002000), not an old V2.3 bin or bootloader bin.')
    elif int(component) == OTA_COMPONENT_ESP:
        if image[0] != 0xE9:
            raise ValueError('ESP application image magic is invalid. Upload the ESP-IDF application .bin, not bootloader.bin/partition-table.bin.')
    else:
        raise ValueError(f'unknown OTA component {component}')
    return True

def run_ota_job(job_id, image, dev, component, group=None):
    group = node_group_id(dev, 1) if group is None else int(group)
    transfer_id = int(uuid.uuid4().int & 0xFFFFFFFF) or 1
    crc = zlib.crc32(image) & 0xFFFFFFFF
    try:
        validate_ota_image(image, component)
        S.ota_job_update(job_id, state='probing', progress=1, transfer_id=transfer_id, crc32=f'{crc:08X}', message='读取目标固件信息')
        info = request_ota_info(dev, component, group, timeout=3.5)
        if not info:
            raise RuntimeError('目标没有返回 OTA_INFO。STM32 必须先通过 ST-LINK 安装 V2.4 bootloader；ESP 必须先烧 V2.4 OTA 固件。')
        max_size = int(info.get('max_image_size') or 0)
        if max_size and len(image) > max_size:
            raise RuntimeError(f'固件 {len(image)} bytes 超过目标允许的 {max_size} bytes')
        target = int(info['target'])
        S.ota_job_update(job_id, target=target, target_name=info.get('target_name'), fw_before=info.get('fw_version'), max_image_size=max_size)

        if component == OTA_COMPONENT_STM32:
            S.ota_job_update(job_id, state='prepare', progress=2, message='请求 STM32 进入 OTA Bootloader')
            r = _ota_retry(dev, CMD_OTA_PREPARE, pack_ota_prepare(component, transfer_id), transfer_id, component, group, timeout=3.0, retries=3)
            if not r or int(r.get('result', -1)) != 0:
                raise RuntimeError(f'OTA_PREPARE failed: {r}')
            S.ota_job_update(job_id, state='bootloader', progress=3, message='等待 STM32 OTA Bootloader 恢复 Node 身份与会话')
            ready = wait_stm32_bootloader_ready(dev, transfer_id, component, group, timeout=6.0)
            if not ready:
                raise RuntimeError('STM32 Bootloader 未返回 OTA_STATUS。请确认目标已烧 V2.4.x OTA Bootloader，并且 Node identity/session 能在重启后恢复。')

        S.ota_job_update(job_id, state='begin', progress=4, message='擦除/准备目标分区')
        begin = pack_ota_begin(component, target, transfer_id, len(image), crc)
        r = _ota_retry(dev, CMD_OTA_BEGIN, begin, transfer_id, component, group, timeout=4.0, retries=8)
        if not r or int(r.get('result', -1)) != 0:
            raise RuntimeError(f'OTA_BEGIN failed: {r}')

        # 360 B firmware data + 12 B OTA head + 20 B route head = 392 B route.
        # ESP-NOW transport fragments this into two 200-byte radio chunks instead of three.
        chunk_size = 360
        offset = int(r.get('next_offset', 0))
        total = len(image)
        while offset < total:
            chunk = image[offset:offset + chunk_size]
            want = offset + len(chunk)
            payload = pack_ota_data(component, transfer_id, offset, chunk)
            r = _ota_retry(dev, CMD_OTA_DATA, payload, transfer_id, component, group, timeout=3.0, retries=6, accept_next=want)
            if not r:
                raise RuntimeError(f'OTA_DATA timeout at offset {offset}')
            remote_next = int(r.get('next_offset', -1))
            if remote_next != want:
                if remote_next > offset and remote_next <= total:
                    offset = remote_next
                    continue
                raise RuntimeError(f'OTA_DATA offset mismatch local={want} remote={remote_next} result={r.get("result")}')
            offset = want
            pct = 5 + int(91 * offset / max(1, total))
            S.ota_job_update(job_id, state='transfer', progress=min(96, pct), bytes_sent=offset, total_bytes=total, message=f'传输 {offset}/{total} bytes')

        S.ota_job_update(job_id, state='verify', progress=97, message='校验并切换新固件')
        r = _ota_retry(dev, CMD_OTA_END, pack_ota_end(component, transfer_id, crc), transfer_id, component, group, timeout=6.0, retries=4)
        if not r or int(r.get('result', -1)) != 0:
            raise RuntimeError(f'OTA_END failed: {r}')
        S.ota_job_update(job_id, state='reboot', progress=99, message='目标重启中；等待组件重新响应')
        time.sleep(0.8 if component == OTA_COMPONENT_ESP else 0.5)
        after = wait_component_reappear(dev, component, group, timeout=10.0)
        if after:
            S.ota_job_update(job_id, state='done', progress=100, ok=True, fw_after=after.get('fw_version'), message='OTA 完成，目标组件已重新上线')
        else:
            # OTA_END success means the target verified/committed the image before reset.
            # Keep the flash result successful but make the reconnect state explicit.
            S.ota_job_update(job_id, state='done', progress=100, ok=True, reconnect_pending=True, message='OTA 写入与校验完成；目标暂未重新响应，请刷新节点状态')
    except Exception as e:
        S.ota_job_update(job_id, state='failed', progress=S.ota_jobs.get(job_id, {}).get('progress', 0), ok=False, error=str(e), message=str(e))
        S.event({'ota_job': job_id, 'device_id': dev, 'component': ota_component_name(component), 'error': str(e)}, 'error')

def create_ota_job(image, dev, component, filename, group=None):
    if not image:
        raise ValueError('firmware file is empty')
    if len(image) > 4 * 1024 * 1024:
        raise ValueError('firmware file too large')
    job_id = uuid.uuid4().hex[:12]
    now = time.time()
    job = {'job_id': job_id, 'device_id': int(dev), 'component': int(component), 'component_name': ota_component_name(component),
           'filename': filename, 'size': len(image), 'state': 'queued', 'progress': 0, 'ok': None, 'message': '等待开始',
           'created_at': now, 'updated_at': now, 'bytes_sent': 0, 'total_bytes': len(image)}
    with S.lock:
        # One active job per target component; this prevents two HTTP/Qt clients from racing the same flash writer.
        for j in S.ota_jobs.values():
            if j.get('device_id') == int(dev) and j.get('component') == int(component) and j.get('state') not in ('done', 'failed', 'cancelled'):
                raise RuntimeError('该 Node/组件已有 OTA 任务运行中')
        S.ota_jobs[job_id] = job
    threading.Thread(target=run_ota_job, args=(job_id, bytes(image), int(dev), int(component), group), daemon=True).start()
    return copy.deepcopy(job)


class API(SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        return

    def end_headers(self):
        # The Web UI is developed in-place and all releases use the same /app.js
        # and /styles.css URLs. Disable browser caching so a new package can never
        # render with stale CSS/JS from an older SmartAgri release.
        self.send_header('Cache-Control', 'no-store, no-cache, must-revalidate, max-age=0')
        self.send_header('Pragma', 'no-cache')
        self.send_header('Expires', '0')
        super().end_headers()

    def _json(self, obj, code=200):
        b = json.dumps(obj, ensure_ascii=False).encode('utf-8')
        self.send_response(code); self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Cache-Control', 'no-store'); self.send_header('Content-Length', len(b)); self.end_headers(); self.wfile.write(b)

    def _body(self):
        n = int(self.headers.get('Content-Length', '0'))
        return json.loads(self.rfile.read(n) or b'{}')

    def do_GET(self):
        path = self.path.split('?', 1)[0]
        if path == '/api/status': return self._json(S.snapshot())
        if path == '/api/ports':
            ports = []
            for p in list_ports.comports():
                ports.append({'device': p.device, 'name': p.name, 'description': p.description,
                              'hwid': p.hwid, 'manufacturer': p.manufacturer})
            with S.lock:
                pref = str(S.settings.get('preferred_port', '') or '')
                baud = int(S.settings.get('baud', 115200))
            return self._json({'ok': True, 'ports': ports, 'preferred_port': pref, 'baud': baud})
        if path == '/api/schema': return self._json(schema_dict())
        if path == '/api/settings':
            with S.lock: return self._json(copy.deepcopy(S.settings))
        if path == '/api/profiles':
            with S.lock: return self._json(copy.deepcopy(S.profiles))
        if path.startswith('/api/caps/'):
            try:
                d = int(path.split('/')[-1])
                with S.lock: old = S.node(d).get('_caps_rev', 0)
                S.send_route(d, CMD_CAPS_GET)
                deadline = time.time() + 2.5
                with S.cv:
                    while time.time() < deadline:
                        n = S.nodes.get(str(d))
                        if n and n.get('_caps_rev', 0) > old:
                            return self._json({'ok': True, 'caps': copy.deepcopy(n.get('caps'))})
                        S.cv.wait(0.1)
                return self._json({'ok': False, 'error': 'CAPS_REPLY timeout'}, 408)
            except Exception as e: return self._json({'ok': False, 'error': str(e)}, 400)
        if path.startswith('/api/config/'):
            try:
                d = int(path.split('/')[-1])
                with S.lock: old = S.node(d).get('_config_rev', 0)
                S.send_route(d, CMD_CONFIG_GET)
                deadline = time.time() + 2.5
                with S.cv:
                    while time.time() < deadline:
                        n = S.nodes.get(str(d))
                        if n and n.get('_config_rev', 0) > old:
                            return self._json({'ok': True, 'config': copy.deepcopy(n.get('config'))})
                        S.cv.wait(0.1)
                return self._json({'ok': False, 'error': 'CONFIG_REPLY timeout'}, 408)
            except Exception as e: return self._json({'ok': False, 'error': str(e)}, 400)
        if path.startswith('/api/radio/'):
            try:
                d = int(path.split('/')[-1]); group = node_group_id(d, 1)
                x = request_radio_config(d, group)
                if x is None:
                    return self._json({'ok': False, 'error': 'RADIO_CONFIG_REPLY timeout; legacy Protocol-2 firmware may not implement remote radio config'}, 408)
                return self._json({'ok': True, 'radio': x})
            except Exception as e: return self._json({'ok': False, 'error': str(e)}, 400)
        if path.startswith('/api/export/'):
            try:
                d = int(path.split('/')[-1])
                return self._json({'ok': True, 'bundle': export_node_config(d)})
            except Exception as e: return self._json({'ok': False, 'error': str(e)}, 400)
        if path.startswith('/api/ota/info/'):
            try:
                parts = path.strip('/').split('/')
                d = int(parts[-2]); comp_text = parts[-1].lower()
                comp = OTA_COMPONENT_STM32 if comp_text in ('stm32','mcu','1') else OTA_COMPONENT_ESP if comp_text in ('esp','esp32','2') else int(comp_text)
                info = request_ota_info(d, comp, node_group_id(d, 1), 3.5)
                if not info: return self._json({'ok': False, 'error': 'OTA_INFO timeout'}, 408)
                return self._json({'ok': True, 'info': info})
            except Exception as e: return self._json({'ok': False, 'error': str(e)}, 400)
        if path == '/api/ota/jobs':
            with S.lock: return self._json({'ok': True, 'jobs': copy.deepcopy(S.ota_jobs)})
        if path.startswith('/api/ota/job/'):
            jid = path.rsplit('/',1)[-1]
            with S.lock: job = copy.deepcopy(S.ota_jobs.get(jid))
            return self._json({'ok': bool(job), 'job': job, 'error': None if job else 'job not found'}, 200 if job else 404)
        return super().do_GET()

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path == '/api/ota/start':
            try:
                q = parse_qs(parsed.query)
                dev = int((q.get('device_id') or ['0'])[0])
                comp_text = str((q.get('component') or [''])[0]).lower()
                component = OTA_COMPONENT_STM32 if comp_text in ('stm32','mcu','1') else OTA_COMPONENT_ESP if comp_text in ('esp','esp32','2') else int(comp_text)
                group_raw = (q.get('group_id') or [''])[0]
                group = int(group_raw) if str(group_raw).strip() else None
                filename = str((q.get('filename') or ['firmware.bin'])[0])
                n = int(self.headers.get('Content-Length', '0'))
                if n <= 0: raise ValueError('empty firmware body')
                image = self.rfile.read(n)
                job = create_ota_job(image, dev, component, filename, group)
                return self._json({'ok': True, 'job': job}, 202)
            except Exception as e:
                return self._json({'ok': False, 'error': str(e)}, 400)
        try: data = self._body()
        except Exception as e: return self._json({'ok': False, 'error': str(e)}, 400)
        try:
            if parsed.path == '/api/serial/reconnect':
                pref = str(data.get('preferred_port', '') or '').strip()
                baud = int(data.get('baud', 115200) or 115200)
                with S.lock:
                    S.settings['preferred_port'] = pref
                    S.settings['baud'] = baud
                    _save_json(SETTINGS_FILE, S.settings)
                S.detach()
                with S.lock:
                    S.serial_scan = {'state': 'reconnect_requested', 'ports': [], 'current_port': None, 'errors': {}, 'last_scan': time.time()}
                return self._json({'ok': True, 'preferred_port': pref, 'baud': baud})
            if parsed.path == '/api/discover':
                S.send_route(SA_DEVICE_BROADCAST, CMD_HEARTBEAT, b'', int(data.get('group_id', 1)))
                return self._json({'ok': True})
            if parsed.path == '/api/radio/configure':
                dev = int(data['device_id']); group = int(data.get('group_id', node_group_id(dev, 1)))
                payload = pack_radio_config(data.get('radio') or data)
                r = S.command_wait(dev, CMD_RADIO_CONFIG_SET, payload, group, timeout=3.2)
                return self._json({'ok': bool(r and int(r.get('result', -999)) == 0), 'result': r,
                                   'note': 'Successful radio changes are saved by ESP and take effect after its scheduled reboot.'},
                                  200 if r and int(r.get('result', -999)) == 0 else 400)
            if parsed.path == '/api/config/import':
                bundle = data.get('bundle', data)
                x = import_node_config(bundle, data.get('target_device_id'), data.get('group_id'))
                return self._json(x, 200 if x.get('ok') else 400)
            if self.path == '/api/configure':
                return self._json(configure_transaction(int(data['device_id']), data.get('slots', []), int(data.get('group_id', 1))))
            if self.path == '/api/delete-slot':
                dev, slot = int(data['device_id']), int(data['slot_id'])
                group = int(data.get('group_id', 1))
                r0 = S.command_wait(dev, CMD_CONFIG_BEGIN, b'', group)
                if not r0 or r0.get('result') != 0: return self._json({'ok': False, 'error': 'CONFIG_BEGIN failed', 'result': r0}, 400)
                r1 = S.command_wait(dev, CMD_CONFIG_SENSOR_DEL, bytes([slot & 0xFF]), group)
                if not r1 or r1.get('result') != 0: return self._json({'ok': False, 'error': 'CONFIG_SENSOR_DEL failed', 'result': r1}, 400)
                r2 = S.command_wait(dev, CMD_CONFIG_COMMIT, b'', group)
                return self._json({'ok': bool(r2 and r2.get('result') == 0), 'result': r2})
            if self.path == '/api/gpio':
                dev, pin = int(data['device_id']), int(data['pin_id'])
                val = 1 if data.get('value') else 0; duration = int(data.get('duration_ms', 0)); mode = 1 if duration else 0
                payload = struct.pack('<HBBI', pin, val, mode, duration)
                r = S.command_wait(dev, CMD_CONTROL_GPIO, payload, int(data.get('group_id', 1)))
                return self._json({'ok': bool(r and r.get('result') == 0), 'result': r})
            if self.path == '/api/test-telemetry':
                dev = int(data['device_id']); payload = pack_diag_telemetry(
                    int(data.get('slot_id', 0)), int(data.get('measurement_id', 0x7FFF)), float(data.get('value', 0)),
                    int(data.get('quality', 100)), int(data.get('driver_id', 0xFFFE)))
                r = S.command_wait(dev, CMD_DIAG_TELEMETRY, payload, int(data.get('group_id', 1)))
                return self._json({'ok': bool(r and r.get('result') == 0), 'result': r,
                                   'note': 'Node generated a real 0x3100 TELEMETRY uplink through the full route.'})
            if self.path == '/api/factory':
                r = S.command_wait(int(data['device_id']), CMD_CONFIG_FACTORY, b'', int(data.get('group_id', 1)))
                return self._json({'ok': bool(r and r.get('result') == 0), 'result': r})
            if self.path == '/api/settings':
                allowed = set(DEFAULT_SETTINGS)
                with S.lock:
                    for k, v in data.items():
                        if k in allowed: S.settings[k] = v
                    _save_json(SETTINGS_FILE, S.settings)
                    out = copy.deepcopy(S.settings)
                return self._json({'ok': True, 'settings': out, 'note': 'Port/baud changes take effect on next reconnect.'})
            if self.path == '/api/profiles/save':
                name = str(data.get('name', '')).strip()
                if not name: return self._json({'ok': False, 'error': 'profile name required'}, 400)
                generic = bool(data.get('generic', False))
                dev = data.get('target_device_id', data.get('device_id'))
                target = {'mode': 'generic', 'device_id': None}
                if not generic and dev not in (None, '', 0, '0'):
                    target = {'mode': 'node', 'device_id': int(dev)}
                profile = _normalize_profile(name, {
                    'name': name,
                    'object_type': data.get('object_type', 'sensor_slot'),
                    'target': target,
                    'slots': data.get('slots', []),
                    'description': data.get('description', ''),
                    'updated_at': time.time(),
                })
                with S.lock:
                    S.profiles[name] = profile; _save_json(PROFILES_FILE, S.profiles)
                return self._json({'ok': True, 'profile': profile})
            if self.path == '/api/profiles/target':
                name = str(data.get('name', '')).strip()
                with S.lock:
                    p = S.profiles.get(name)
                    if not p: return self._json({'ok': False, 'error': 'profile not found'}, 404)
                    generic = bool(data.get('generic', False))
                    dev = data.get('target_device_id')
                    p['target'] = {'mode': 'generic', 'device_id': None} if generic or dev in (None, '', 0, '0') else {'mode': 'node', 'device_id': int(dev)}
                    p['updated_at'] = time.time()
                    _save_json(PROFILES_FILE, S.profiles)
                    out = copy.deepcopy(p)
                return self._json({'ok': True, 'profile': out})
            if self.path == '/api/profiles/apply':
                name = str(data.get('name', '')).strip()
                with S.lock:
                    p = copy.deepcopy(S.profiles.get(name))
                if not p: return self._json({'ok': False, 'error': 'profile not found'}, 404)
                if p.get('object_type') != 'sensor_slot':
                    return self._json({'ok': False, 'error': f"unsupported object_type: {p.get('object_type')}"}, 400)
                dev = data.get('device_id')
                if dev in (None, '', 0, '0'):
                    dev = (p.get('target') or {}).get('device_id')
                if dev in (None, '', 0, '0'):
                    return self._json({'ok': False, 'error': 'target node required for generic profile'}, 400)
                result = configure_transaction(int(dev), p.get('slots', []), int(data.get('group_id', 1)))
                result['profile'] = name
                result['device_id'] = int(dev)
                return self._json(result, 200 if result.get('ok') else 400)
            if self.path == '/api/profiles/delete':
                name = str(data.get('name', '')).strip()
                with S.lock:
                    S.profiles.pop(name, None); _save_json(PROFILES_FILE, S.profiles)
                return self._json({'ok': True})
            if self.path == '/api/forget-offline':
                now = time.time(); timeout = float(S.settings.get('node_offline_timeout_sec', 15.0))
                with S.lock:
                    for k in list(S.nodes):
                        if int(k) == SA_DEVICE_MASTER: continue
                        if now - float(S.nodes[k].get('last_seen') or 0) >= timeout: del S.nodes[k]
                return self._json({'ok': True})
            return self._json({'ok': False, 'error': 'unknown endpoint'}, 404)
        except Exception as e:
            return self._json({'ok': False, 'error': str(e)}, 400)


def main():
    ap = argparse.ArgumentParser(description='SmartAgri V2.4.8 upper-computer backend with auto-discovery, remote radio config, OTA and serial diagnostics')
    ap.add_argument('--port', default='', help='optional COM override; empty = automatic scan')
    ap.add_argument('--baud', type=int, default=0, help='optional baud override; 0 = local settings')
    ap.add_argument('--web-host', default='127.0.0.1')
    ap.add_argument('--web-port', type=int, default=8080)
    ap.add_argument('--no-browser', action='store_true')
    a = ap.parse_args()
    DATA_DIR.mkdir(exist_ok=True)
    threading.Thread(target=serial_worker, args=(a.port or None, a.baud or None), daemon=True).start()
    os.chdir(BASE / 'static')
    http = ThreadingHTTPServer((a.web_host, a.web_port), API)
    print(f'Web UI: http://127.0.0.1:{a.web_port}')
    print('MASTER link: auto-detect STM32 USART2 over USB-TTL; Qt and Web share this backend')
    auto_browser = bool(S.settings.get('auto_open_browser', True)) and not a.no_browser
    if auto_browser: threading.Timer(0.8, lambda: webbrowser.open(f'http://127.0.0.1:{a.web_port}')).start()
    try: http.serve_forever()
    finally: S.stop = True; S.detach()


if __name__ == '__main__':
    main()
