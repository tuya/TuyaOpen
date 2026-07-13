# HTTP Service Portal

This example starts a SoftAP on the device and serves a minimal web portal (HTML/JS/CSS) through `http_host` and `http_site`.

## Default SoftAP

- SSID: `TuyaOpen-Setup`
- Password: `12345678`
- AP IP: `192.168.50.1`
- HTTP port: `80`

## Build

```bash
cd examples/protocols/http_service_portal
tos.py build
```

After flashing, connect to the device hotspot and open `http://192.168.50.1/` in a browser.
