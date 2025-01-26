# Home Assistant Mouse Trap Monitor

An ESP32-based mouse trap monitoring system that integrates with Home Assistant via MQTT. The system uses light-dependent resistors (LDRs) to detect both the trap state (triggered/ready) and battery status (ok/low), providing real-time updates to your Home Assistant instance.

## Features

- Monitors mouse trap state (triggered/ready) using an LDR sensor
- Monitors battery status (ok/low) using a second LDR sensor
- Integrates with Home Assistant via MQTT
- Power-efficient design with deep sleep between readings
- 24-hour heartbeat publishing to ensure device health monitoring
- Configurable sampling parameters and thresholds
- Automatic state persistence across deep sleep cycles
- Robust WiFi and MQTT connection handling
- Modular code structure for better maintainability
- Configurable debug output

## Project Structure

The project follows a modular architecture for better organization and maintainability:

```
├── main/                  # Core application code
│   ├── include/          # Header files
│   │   ├── common.h     # Common definitions and utilities
│   │   ├── wifi_manager.h # WiFi connection management
│   │   ├── mqtt_manager.h # MQTT client operations
│   │   ├── sensor_manager.h # ADC and sensor handling
│   │   ├── led_controller.h # LED control functions
│   │   └── diagnostic.h  # Diagnostic mode operations
│   ├── src/             # Source files
│   │   ├── main.c      # Main application entry
│   │   ├── wifi_manager.c # WiFi implementation
│   │   ├── mqtt_manager.c # MQTT implementation
│   │   ├── sensor_manager.c # Sensor implementation
│   │   ├── led_controller.c # LED implementation
│   │   └── diagnostic.c # Diagnostic implementation
│   └── CMakeLists.txt   # Component build configuration
└── traps/               # Trap-specific configurations
    ├── backdoor/       # Back door trap config
    │   ├── config.h.template # Configuration template
    │   └── secrets.h.template # Credentials template
    ├── garage_near/    # Garage near trap config
    │   ├── config.h.template # Configuration template
    │   └── secrets.h.template # Credentials template
    └── template/       # Template for new traps
        ├── config.h.template # Configuration template
        └── secrets.h.template # Credentials template
```

## Hardware Requirements

- ESP32 development board ( M5Stamp C3)
- 2x Light Dependent Resistors (LDRs)
- Voltage divider resistors for LDRs
- Mouse trap with status LED and optional battery status LED (I use the OWL electronic mouse trap)

## Pin Configuration

- GPIO4: LDR sensor for trap state detection
- GPIO1: LDR sensor for battery status detection
- GPIO2: Built-in RGB LED (using ESP32's led_strip driver)

## Software Requirements

- ESP-IDF v5.x
- Home Assistant with MQTT broker

## Setup Instructions

The project now supports multiple mouse traps with individual configurations. Each trap has its own configuration directory under `traps/`.

### Available Traps
- `backdoor` - Back door mouse trap (original)
- `garage_near` - Garage near entrance trap
- Additional traps can be added by copying the `template` directory

### Setting Up a New Trap

1. Clone this repository

2. Choose a trap to build:
   - Use existing traps in `traps/` directory
   - Or create a new trap by copying `traps/template`:
     ```
     copy traps\template traps\your_new_trap
     ```

3. Configure the trap:
   a. Copy `traps/your_trap/secrets.h.template` to `secrets.h` in the same directory:
      ```c
      #define WIFI_SSID "your_wifi_ssid"
      #define WIFI_PASS "your_wifi_password"
      #define MQTT_BROKER "your.mqtt.broker.ip"
      #define MQTT_USERNAME "your_mqtt_username"
      #define MQTT_PASSWORD "your_mqtt_password"
      ```
   b. Edit `traps/your_trap/config.h`:
      - Set TRAP_ID and TRAP_FRIENDLY_NAME
      - Adjust MQTT topics
      - Modify thresholds if needed
      - Configure other parameters as needed

4. Build and flash for a specific trap:
   ```bash
   # For backdoor trap (default if no trap specified)
   powershell -Command "& {. 'C:\Users\username\esp\v5.4\esp-idf\export.ps1'; idf.py build flash}"

   # For garage near trap (clean first when switching targets)
   powershell -Command "& {. 'C:\Users\username\esp\v5.4\esp-idf\export.ps1'; idf.py clean; idf.py -DTRAP_ID=garage_near build flash}"
   ```

   Note: Always run `idf.py clean` before building for a different trap target to ensure the correct configuration is used.

### Adding More Traps

1. Copy the template directory:
   ```
   copy traps\template traps\new_trap_name
   ```

2. Edit the new trap's config.h:
   - Update TRAP_ID (use underscores, e.g., garage_far)
   - Update TRAP_FRIENDLY_NAME
   - Update MQTT topics
   - Adjust thresholds based on location

3. Configure secrets.h as above

4. Build using:
   ```bash
   # Clean first when switching from another target
   powershell -Command "& {. 'C:\Users\username\esp\v5.4\esp-idf\export.ps1'; idf.py clean; idf.py -DTRAP_ID=new_trap_name build flash}"
   ```

## Configuration Parameters

### Debug Configuration
- `DEBUG_LOGS`: Set to 1 to enable debug messages, 0 to disable (default: 0)
  - When disabled, only essential system messages and diagnostic mode prompts are shown
  - When enabled, provides detailed operational status messages for debugging

### MQTT Configuration
- `MQTT_PORT`: MQTT broker port (default: 1883)
- `MQTT_TOPIC_CAUGHT`: Topic for trap state updates (default: "home/mousetrap/backdoor/state")
- `MQTT_TOPIC_BATTERY`: Topic for battery status updates (default: "home/mousetrap/backdoor/battery")

### Timing Configuration
- `BURST_DURATION_MS`: Duration of each sampling burst (default: 12000ms)
- `SAMPLE_INTERVAL_MS`: Interval between samples during burst (default: 20ms)
- `SLEEP_TIME_SECONDS`: Deep sleep duration between bursts (default: 30 minutes)
- `FORCE_PUBLISH_INTERVAL_SEC`: Maximum time between state publications (default: 24 hours)

### Threshold Configuration
- `TRAP_THRESHOLD`: ADC threshold for trap triggered state (default: 50)
- `BATTERY_THRESHOLD`: ADC threshold for low battery state (default: 200)
- Note: These thresholds are calibrated for dark room conditions with the LDR pointed at a black surface. You may need to adjust based on your specific setup and ambient light conditions.

## Home Assistant Configuration

Add configurations for each trap to your Home Assistant configuration. Here's the complete setup for both existing traps:

```yaml
mqtt:
  binary_sensor:
    # Back Door Trap
    - name: "Back Door Mouse Trap"
      unique_id: "backdoor_mousetrap_state"
      state_topic: "home/mousetrap/backdoor/state"
      payload_on: "triggered"
      payload_off: "ready"
      device_class: occupancy

    - name: "Back Door Mouse Trap Battery"
      unique_id: "backdoor_mousetrap_battery"
      state_topic: "home/mousetrap/backdoor/battery"
      payload_on: "low"
      payload_off: "ok"
      device_class: problem
      expire_after: 129600  # 36 hours in seconds

    # Garage Near Trap
    - name: "Garage Near Mouse Trap"
      unique_id: "garage_near_mousetrap_state"
      state_topic: "home/mousetrap/garage_near/state"
      payload_on: "triggered"
      payload_off: "ready"
      device_class: occupancy

    - name: "Garage Near Mouse Trap Battery"
      unique_id: "garage_near_mousetrap_battery"
      state_topic: "home/mousetrap/garage_near/battery"
      payload_on: "low"
      payload_off: "ok"
      device_class: problem
      expire_after: 129600  # 36 hours in seconds

# Template for additional traps (copy and modify for each new trap):
    # - name: "New Location Mouse Trap"
    #   unique_id: "new_location_mousetrap_state"
    #   state_topic: "home/mousetrap/new_location/state"
    #   payload_on: "triggered"
    #   payload_off: "ready"
    #   device_class: occupancy
    #
    # - name: "New Location Mouse Trap Battery"
    #   unique_id: "new_location_mousetrap_battery"
    #   state_topic: "home/mousetrap/new_location/battery"
    #   payload_on: "low"
    #   payload_off: "ok"
    #   device_class: problem
    #   expire_after: 129600  # 36 hours in seconds

Each trap's sensors will show their last update time in Home Assistant, which can be used to monitor when the device last published its state (either due to state changes or the 24-hour heartbeat).

You can create an automation to monitor device health using the last update time:
```yaml
automation:
  - alias: "Mouse Trap Device Offline Alert"
    trigger:
      - platform: template
        value_template: >
          {% set last_changed = states.binary_sensor.back_door_mouse_trap.last_changed %}
          {% set hours_since = ((now() - last_changed) | as_timedelta).total_seconds() / 3600 %}
          {{ hours_since > 25 }}
    action:
      - service: notify.notify
        data:
          message: >-
            Mouse trap device hasn't reported in over 25 hours. Last report was
            {{ states.binary_sensor.back_door_mouse_trap.last_changed | as_local }}
```

The above automation will notify you if the device hasn't reported its state for more than 25 hours (allowing a small buffer over the 24-hour heartbeat).
```

## Operation

1. The device wakes up every 30 minutes (configurable)
2. Performs burst sampling for 12 seconds to detect LED states
3. If any state has changed (trap triggered or battery low) or 24 hours have elapsed since last publish:
   - Connects to WiFi
   - Connects to MQTT broker
   - Publishes the current state(s)
   - Updates last publish timestamp
4. Goes back to deep sleep to conserve power

## Power Consumption

The device is designed to be power efficient:
- Uses deep sleep between readings
- Uses light sleep during burst sampling instead of active waiting
- Only connects to WiFi/MQTT when states change or every 24 hours
- WiFi power save mode (modem sleep) enabled during connections, reducing power consumption by ~60%
- Configurable sleep duration
- Minimal wake time with optimized sampling using sleep modes
- UART/Serial interface completely disabled after diagnostic mode check
- Serial output only enabled during initial boot and diagnostic mode
- GPIO pins for UART (TX/RX) reset to save power when not in use

The dual sleep mode strategy maximizes power efficiency:
1. Light sleep during burst sampling (20ms intervals)
   - Reduces power consumption during the 12-second sampling period
   - Maintains millisecond-level timing accuracy
   - CPU and most peripherals powered down between samples
2. Deep sleep between sampling cycles (30 minutes)
   - Maximum power savings during long idle periods
   - Only RTC and essential peripherals remain powered

## Troubleshooting

- If the sensor readings are inconsistent, adjust the `TRAP_THRESHOLD` and `BATTERY_THRESHOLD` values in `config.h`
- For more frequent updates, reduce `SLEEP_TIME_SECONDS`
- For more accurate readings, increase `BURST_DURATION_MS` or decrease `SAMPLE_INTERVAL_MS`
- For detailed operation logs, set `DEBUG_LOGS` to 1 in `config.h`
  - This will output detailed status messages for WiFi, MQTT, and sensor operations
  - Note: Critical system messages and diagnostic mode will always be shown regardless of this setting
- Use the built-in diagnostic mode by holding the boot button during initial power-up (not available during wake from sleep):
  - The RGB LED will indicate sensor states in diagnostic mode:
    - Green: Mouse trap sensor triggered
    - Red: Battery sensor triggered
    - Yellow: Both sensors triggered
    - Blue: No sensors triggered
  - LED only activates in diagnostic mode to conserve power during normal operation
  - Diagnostic mode can only be entered during initial power-up, not during wake from sleep cycles
- If you encounter build issues with macros in common.h, ensure there are no trailing backslashes at the end of lines

## License

This project is open source and available under the MIT License.
