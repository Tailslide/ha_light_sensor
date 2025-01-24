# Home Assistant Mouse Trap Monitor

An ESP32-based mouse trap monitoring system that integrates with Home Assistant via MQTT. The system uses light-dependent resistors (LDRs) to detect both the trap state (triggered/ready) and battery status (ok/low), providing real-time updates to your Home Assistant instance.

## Features

- Monitors mouse trap state (triggered/ready) using an LDR sensor
- Monitors battery status (ok/low) using a second LDR sensor
- Integrates with Home Assistant via MQTT
- Power-efficient design with deep sleep between readings
- Configurable sampling parameters and thresholds
- Automatic state persistence across deep sleep cycles
- Robust WiFi and MQTT connection handling
- Modular code structure for better maintainability

## Project Structure

The project follows a modular architecture for better organization and maintainability:

```
main/
├── include/                 # Header files
│   ├── common.h            # Common definitions and utilities
│   ├── wifi_manager.h      # WiFi connection management
│   ├── mqtt_manager.h      # MQTT client operations
│   ├── sensor_manager.h    # ADC and sensor handling
│   ├── led_controller.h    # LED control functions
│   └── diagnostic.h        # Diagnostic mode operations
├── src/                    # Source files
│   ├── main.c             # Main application entry
│   ├── wifi_manager.c     # WiFi implementation
│   ├── mqtt_manager.c     # MQTT implementation
│   ├── sensor_manager.c   # Sensor implementation
│   ├── led_controller.c   # LED implementation
│   └── diagnostic.c       # Diagnostic implementation
├── CMakeLists.txt         # Component build configuration
├── config.h.template      # Configuration template
└── secrets.h.template     # Credentials template
```

## Hardware Requirements

- ESP32 development board
- 2x Light Dependent Resistors (LDRs)
- Voltage divider resistors for LDRs
- Mouse trap with status LED
- Battery with status LED

## Pin Configuration

- GPIO4: LDR sensor for trap state detection
- GPIO1: LDR sensor for battery status detection
- GPIO2: Built-in RGB LED (using ESP32's led_strip driver)

## Software Requirements

- ESP-IDF v5.x
- Home Assistant with MQTT broker

## Setup Instructions

1. Clone this repository
2. Copy `main/secrets.h.template` to `main/secrets.h` and configure your WiFi and MQTT credentials:
   ```c
   #define WIFI_SSID "your_wifi_ssid"
   #define WIFI_PASS "your_wifi_password"
   #define MQTT_BROKER "your.mqtt.broker.ip"
   #define MQTT_USERNAME "your_mqtt_username"
   #define MQTT_PASSWORD "your_mqtt_password"
   ```

3. (Optional) Copy `main/config.h.template` to `main/config.h` and adjust the configuration parameters if needed:
   - MQTT topics
   - ADC channels
   - Timing parameters
   - Threshold values

4. Build and flash the project:
   ```bash
   powershell -Command "& {. 'C:\Users\gregp\esp\v5.4\esp-idf\export.ps1'; idf.py build}"
   ```

## Configuration Parameters

### MQTT Configuration
- `MQTT_PORT`: MQTT broker port (default: 1883)
- `MQTT_TOPIC_CAUGHT`: Topic for trap state updates (default: "home/mousetrap/backdoor/state")
- `MQTT_TOPIC_BATTERY`: Topic for battery status updates (default: "home/mousetrap/backdoor/battery")

### Timing Configuration
- `BURST_DURATION_MS`: Duration of each sampling burst (default: 12000ms)
- `SAMPLE_INTERVAL_MS`: Interval between samples during burst (default: 20ms)
- `SLEEP_TIME_SECONDS`: Deep sleep duration between bursts (default: 30 minutes)

### Threshold Configuration
- `TRAP_THRESHOLD`: ADC threshold for trap triggered state (default: 300)
- `BATTERY_THRESHOLD`: ADC threshold for low battery state (default: 300)

## Home Assistant Configuration

Add the following to your Home Assistant configuration:

```yaml
binary_sensor:
  - platform: mqtt
    name: "Back Door Mouse Trap"
    state_topic: "home/mousetrap/backdoor/state"
    payload_on: "triggered"
    payload_off: "ready"
    device_class: occupancy

  - platform: mqtt
    name: "Back Door Mouse Trap Battery"
    state_topic: "home/mousetrap/backdoor/battery"
    payload_on: "low"
    payload_off: "ok"
    device_class: battery
```

## Operation

1. The device wakes up every 30 minutes (configurable)
2. Performs burst sampling for 12 seconds to detect LED states
3. If any state has changed (trap triggered or battery low):
   - Connects to WiFi
   - Connects to MQTT broker
   - Publishes the new state(s)
4. Goes back to deep sleep to conserve power

## Power Consumption

The device is designed to be power efficient:
- Uses deep sleep between readings
- Only connects to WiFi/MQTT when states change
- Configurable sleep duration
- Minimal wake time with burst sampling

## Troubleshooting

- If the sensor readings are inconsistent, adjust the `TRAP_THRESHOLD` and `BATTERY_THRESHOLD` values in `config.h`
- For more frequent updates, reduce `SLEEP_TIME_SECONDS`
- For more accurate readings, increase `BURST_DURATION_MS` or decrease `SAMPLE_INTERVAL_MS`
- Check the serial output for debugging information and sensor values
- Use the built-in diagnostic mode by holding the boot button during startup
- If you encounter build issues with macros in common.h, ensure there are no trailing backslashes at the end of lines

## License

This project is open source and available under the MIT License.
