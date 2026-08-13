import json
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
API = 'http://127.0.0.1:8080'

class ApiClient:
    def __init__(self, base_url=API):
        self.base_url = base_url.rstrip('/')

    def get(self, path, timeout=4):
        with urllib.request.urlopen(self.base_url + path, timeout=timeout) as r:
            return json.loads(r.read().decode('utf-8'))

    def post(self, path, obj, timeout=7):
        b = json.dumps(obj).encode('utf-8')
        req = urllib.request.Request(self.base_url + path, data=b, headers={'Content-Type': 'application/json'}, method='POST')
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode('utf-8'))


    def post_binary(self, path, data, timeout=20):
        req = urllib.request.Request(self.base_url + path, data=bytes(data), headers={'Content-Type': 'application/octet-stream'}, method='POST')
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode('utf-8'))

    def alive(self):
        try:
            self.get('/api/status', 0.4)
            return True
        except Exception:
            return False

    def ensure_backend(self):
        if self.alive():
            return None
        p = subprocess.Popen([sys.executable, str(BASE / 'smartagri_server.py'), '--no-browser'], cwd=str(BASE))
        deadline = time.time() + 8
        while time.time() < deadline:
            if p.poll() is not None:
                raise RuntimeError('smartagri_server.py exited before startup')
            if self.alive():
                return p
            time.sleep(0.15)
        p.terminate()
        raise RuntimeError('Backend startup timeout')
