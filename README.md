# BLDC Fan Remote — Combined Online + Home Wi-Fi

This repository contains the phone/PWA remote for the ESP32 BLDC fan.

## Important architecture note

The GitHub Pages app is served over HTTPS. Browsers block an HTTPS page from directly calling an ordinary `http://` ESP32 server because of mixed-content security.

Therefore the reliable design is:

- Internet available: GitHub Pages remote → HiveMQ WebSocket/MQTT → ESP32
- ISP/internet down but home Wi-Fi working: open the ESP32's local Home Remote at `http://fan-esp32.local/`
- No IP address is required; mDNS provides the `.local` hostname.

The ESP32 local page uses the same fan controls and MQTT command format.

## 1. GitHub Pages

Upload these files to the repository root:

- `index.html`
- `manifest.json`
- `sw.js`
- `icon-512.png`

In `index.html`, replace:

- `REPLACE_WITH_MQTT_USERNAME`
- `REPLACE_WITH_MQTT_PASSWORD`

with your HiveMQ credentials.

Do not publish a real MQTT password in a public repository if the repository is public. Prefer a dedicated low-privilege MQTT credential and rotate any password that has already been exposed.

## 2. ESP32

Upload `esp32/bldc_fan.ino` using Arduino IDE.

Set your Wi-Fi and MQTT credentials in the configuration section.

The firmware provides:

- MQTT commands on `bldc-fan/7Kx92LmP/command`
- state on `bldc-fan/7Kx92LmP/state`
- local web remote at `http://fan-esp32.local/`
- mDNS hostname `fan-esp32.local`
- ON/OFF/S1-S6/FULL
- startup state protection: no automatic ON command after reboot

## 3. Local mode

Connect the phone to the same home Wi-Fi as the ESP32.

When the ISP is down, open:

`http://fan-esp32.local/`

No IP address is required.

## 4. Fan startup behavior

The firmware deliberately does not send an ON command at boot. After reboot it waits for a command. This prevents an ESP32 restart from intentionally turning the fan on.

## 5. Hardware

Keep the IR transmitter circuit that is already working with your fan. This firmware assumes the working IR LED/transistor circuit is connected to the configured IR transmit GPIO.
