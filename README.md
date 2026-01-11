# ESP32 M5Dial Remote Controller

Remote controller firmware targeting the **M5Stack Dial** (round 240×240 display + touch + encoder). This project controls a fatigue-test unit over ESP-NOW.

## Features

- Round-screen UI with touch + encoder navigation
- Control pages for fatigue testing (including **Bounds** with Start/Stop/Back)
- Secure discovery/pairing support at the protocol layer (approved peers stored in NVS)

## Build

This project uses the repo build tools under `scripts/`.

### Pairing secret

Some configurations require a shared pairing secret (HMAC-based pairing). Provide it one of these ways:

- `--secret <hex>` (recommended for CI/local testing)
- `ESPNOW_PAIRING_SECRET_HEX` environment variable
- `secrets.local.yml` (see `secrets.template.yml`)

Example:

```bash
cd examples/esp32_m5dial_remote_controller
./scripts/build_app.sh m5dial_remote_controller Release --secret 00000000deadbeefcafebabedeadbeef
```

## Flash

```bash
cd examples/esp32_m5dial_remote_controller
./scripts/flash_app.sh flash_monitor m5dial_remote_controller Release --port /dev/ttyACM0
```

## Notes

- Pairing UI/workflow may be implemented separately from the protocol support. If pairing is enabled and no peer is approved/configured, control messages may not be sent until a peer is selected/paired.

