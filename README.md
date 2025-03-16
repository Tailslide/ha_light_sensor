# Home Assistant Mouse Trap Monitor

An ESP32-based mouse trap monitoring system that integrates with Home Assistant via MQTT. The system uses light-dependent resistors (LDRs) to detect both the trap state (triggered/ready) and battery status (ok/low), providing real-time updates to your Home Assistant instance.

## Features

- Monitors mouse trap state (triggered/ready) using an LDR sensor
- Monitors battery status (ok/low) using a second LDR sensor
- Optional external wake circuit support for immediate trap detection
- Integrates with Home Assistant via MQTT
- Power-efficient design with deep sleep between readings
- 24-hour heartbeat publishing to ensure device health monitoring
- MQTT will messages for accurate availability tracking
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

### Optional Wake Circuit Components
- TLV7011 comparator (or similar)
- GL5539 LDR (recommended for best sensitivity)
- 10kΩ resistor for LDR voltage divider
- 10kΩ trim potentiometer for threshold adjustment
- Connecting wires

## Pin Configuration

- GPIO4: LDR sensor for trap state detection (not needed if using wake circuit)
- GPIO1: LDR sensor for battery status detection
- GPIO2: Built-in RGB LED (using ESP32's led_strip driver)
- GPIO5: Wake circuit input (when USE_WAKE_CIRCUIT=1)

### Wake Circuit Connections
If using the optional wake circuit:
1. TLV7011 Pin 1 (OUT) → ESP32 GPIO5
2. TLV7011 Pin 2 (VEE) → GND
3. TLV7011 Pin 3 (IN+) → LDR voltage divider midpoint
4. TLV7011 Pin 4 (IN-) → Trim pot wiper (middle pin)
5. TLV7011 Pin 5 (VDD) → 3.3V

LDR Circuit:
```
3.3V ----[LDR]----+----[10kΩ]---- GND
                  |
                  +---- TLV7011 Pin 3 (IN+)
```

Trim Pot Circuit:
```
3.3V ----[10kΩ Trim Pot]----+---- GND
                            |
                            +---- TLV7011 Pin 4 (IN-)
```
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

### Power Optimization Settings

The project includes a `sdkconfig.defaults` file with optimized settings for power consumption:

```
# Power Management - Enable automatic power saving
CONFIG_PM_ENABLE=y
CONFIG_PM_DFS_INIT_AUTO=y
CONFIG_PM_USE_RTC_TIMER_REF=y

# CPU Frequency - Lower to 80MHz to save power
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_80=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=80

# Flash Power Down in Deep Sleep
CONFIG_ESP_SLEEP_POWER_DOWN_FLASH=y

# WiFi Power Saving
CONFIG_ESP_WIFI_SLP_IRAM_OPT=y
CONFIG_ESP_WIFI_SLP_DEFAULT_MIN_ACTIVE_TIME=100
CONFIG_ESP_WIFI_SLP_DEFAULT_MAX_ACTIVE_TIME=5
CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE=y

# Minimize Logging
CONFIG_LOG_DEFAULT_LEVEL_ERROR=y
CONFIG_LOG_DEFAULT_LEVEL=1
```

These settings are automatically applied during the build process and significantly reduce power consumption.

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
- `MQTT_TOPIC_AVAILABILITY`: Topic for device availability status (default: "home/mousetrap/backdoor/availability")

### Timing Configuration
- `BURST_DURATION_MS`: Duration of each sampling burst (default: 12000ms)
- `SAMPLE_INTERVAL_MS`: Interval between samples during burst (default: 20ms)
- `SLEEP_TIME_SECONDS`: Deep sleep duration between bursts (default: 30 minutes)
- `HEARTBEAT_INTERVAL_HOURS`: How often to force publish state updates (default: 24 hours)
- `CYCLES_PER_HOUR`: Automatically calculated based on sleep time (2 cycles per hour with default sleep time)
- `CYCLES_FOR_PUBLISH`: Automatically calculated based on HEARTBEAT_INTERVAL_HOURS and sleep time
  (e.g., with 30-minute sleep time and 24-hour interval: 2 cycles/hour * 24 hours = 48 cycles)

### Threshold Configuration
- `TRAP_THRESHOLD`: ADC threshold for trap triggered state (default: 50)
- `BATTERY_THRESHOLD`: ADC threshold for low battery state (default: 200)
- Note: These thresholds are calibrated for dark room conditions with the LDR pointed at a black surface. You may need to adjust based on your specific setup and ambient light conditions.

### Wake Circuit Configuration
- `USE_WAKE_CIRCUIT`: Set to 1 to enable external comparator wake circuit, 0 to use standard ADC sampling (default: 0)
- `WAKE_PIN`: GPIO pin connected to comparator output (default: GPIO5)
- Note: To test and calibrate the wake circuit, use diagnostic mode by holding the button during boot

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
      availability_topic: "home/mousetrap/backdoor/availability"
      payload_available: "online"
      payload_not_available: "offline"

    - name: "Back Door Mouse Trap Battery"
      unique_id: "backdoor_mousetrap_battery"
      state_topic: "home/mousetrap/backdoor/battery"
      payload_on: "low"
      payload_off: "ok"
      device_class: problem
      availability_topic: "home/mousetrap/backdoor/availability"
      payload_available: "online"
      payload_not_available: "offline"

    # Garage Near Trap
    - name: "Garage Near Mouse Trap"
      unique_id: "garage_near_mousetrap_state"
      state_topic: "home/mousetrap/garage_near/state"
      payload_on: "triggered"
      payload_off: "ready"
      device_class: occupancy
      availability_topic: "home/mousetrap/garage_near/availability"
      payload_available: "online"
      payload_not_available: "offline"

    - name: "Garage Near Mouse Trap Battery"
      unique_id: "garage_near_mousetrap_battery"
      state_topic: "home/mousetrap/garage_near/battery"
      payload_on: "low"
      payload_off: "ok"
      device_class: problem
      availability_topic: "home/mousetrap/garage_near/availability"
      payload_available: "online"
      payload_not_available: "offline"

# Template for additional traps (copy and modify for each new trap):
    # - name: "New Location Mouse Trap"
    #   unique_id: "new_location_mousetrap_state"
    #   state_topic: "home/mousetrap/new_location/state"
    #   payload_on: "triggered"
    #   payload_off: "ready"
    #   device_class: occupancy
    #   availability_topic: "home/mousetrap/new_location/availability"
    #   payload_available: "online"
    #   payload_not_available: "offline"
    #
    # - name: "New Location Mouse Trap Battery"
    #   unique_id: "new_location_mousetrap_battery"
    #   state_topic: "home/mousetrap/new_location/battery"
    #   payload_on: "low"
    #   payload_off: "ok"
    #   device_class: problem
    #   availability_topic: "home/mousetrap/new_location/availability"
    #   payload_available: "online"
    #   payload_not_available: "offline"
```

The MQTT will message feature ensures that sensors will automatically become `unavailable` if the device disconnects unexpectedly. This provides more accurate and immediate monitoring of device health compared to the previous `expire_after` approach.

Benefits of using MQTT will messages:
- Immediate notification when a device goes offline (no need to wait for a timeout)
- More accurate representation of device state
- Retained messages ensure the availability state persists even after Home Assistant restarts
- Reduces the "flapping" between unavailable and clear states when Home Assistant restarts

You can still create automations to monitor device health using the unavailable state:

```yaml
automation:
  - alias: "Back Door Mouse Trap Disconnected"
    trigger:
      - platform: state
        entity_id: binary_sensor.back_door_mouse_trap
        to: unavailable
    action:
      - service: notify.notify
        data:
          message: Your Back Door mouse trap hasn't reported in over 36 hours.

  - alias: "Garage Near Mouse Trap Disconnected"
    trigger:
      - platform: state
        entity_id: binary_sensor.garage_near_mouse_trap
        to: unavailable
    action:
      - service: notify.notify
        data:
          message: Your Garage Near mouse trap hasn't reported in over 36 hours.
```

These automations will notify you if any trap becomes unavailable, which happens automatically after 36 hours without updates (allowing a buffer over the 24-hour heartbeat).

## Operation

### Standard Operation (USE_WAKE_CIRCUIT=0)
1. The device wakes up every 30 minutes (configurable)
2. Performs burst sampling for 12 seconds to detect LED states
3. If any state has changed (trap triggered or battery low) or the configured heartbeat interval has elapsed:
    - Connects to WiFi
    - Connects to MQTT broker
    - Publishes "online" to the availability topic
    - Publishes the current state(s)
    - Resets the cycle counter
4. Goes back to deep sleep to conserve power

### Wake Circuit Operation (USE_WAKE_CIRCUIT=1)
1. The device configures two wake-up sources:
   - GPIO5 (connected to comparator output) - wakes immediately when trap is triggered
   - Timer - wakes every 30 minutes for battery check and heartbeat
2. When woken by GPIO5:
   - Immediately connects to WiFi/MQTT
   - Reports trap as triggered
   - Goes back to sleep
3. When woken by timer:
   - Samples only the battery sensor
   - Connects to WiFi/MQTT if battery state changed or heartbeat interval reached
   - Goes back to sleep

### Diagnostic Mode with Wake Circuit Support
When the device is started by holding down the button on boot, it enters diagnostic mode:
1. If wake circuit is enabled (USE_WAKE_CIRCUIT=1):
   - Continuously monitors the wake pin (GPIO5) for trap detection
   - Shows the RGB LED based on both wake pin and battery sensor states
   - Displays both wake pin state and LDR readings in the console
2. If wake circuit is not enabled:
   - Uses the standard LDR sensors for both trap and battery detection
   - Shows the RGB LED based on both sensor states

This mode allows for real-time adjustment of the trim pot to set the desired light threshold when using the wake circuit.

If the device disconnects unexpectedly, the MQTT broker will automatically publish the configured will message ("offline") to the availability topic, allowing Home Assistant to immediately mark the device as unavailable.

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
- Power optimization settings in sdkconfig.defaults:
  - CPU frequency reduced to 80MHz
  - Flash powered down during deep sleep
  - WiFi power saving optimizations
  - Compiler optimized for size
  - Logging level minimized

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
- If the device is not waking properly from deep sleep:
  - Ensure the GPIO wake pin is properly configured with appropriate pull-up/pull-down resistors
  - For ESP32-C3, use `esp_deep_sleep_enable_gpio_wakeup()` instead of `gpio_wakeup_enable()` and `esp_sleep_enable_gpio_wakeup()`
  - Check for both `ESP_SLEEP_WAKEUP_GPIO` and `ESP_SLEEP_WAKEUP_EXT0` wake causes in your code
  - Properly configure the GPIO pin before enabling it as a wakeup source
  - Add a small delay before entering deep sleep to ensure all logs are printed
- If the wake circuit triggers but doesn't register as a trap event:
  - The latest firmware includes a fix for a race condition where brief wake pin triggers might not be detected
  - The system now considers a trap triggered if either:
    1. The current wake pin level is HIGH, or
    2. The device was woken by the wake circuit (even if the pin is now LOW)
  - This ensures that even momentary triggers will be properly detected and reported
  - The 2025-03-16 update includes additional debugging to ensure the wake circuit flag is properly set and maintained throughout the wake-up process
  - If you see inconsistent behavior with the wake circuit, update to the latest firmware version

### Power Optimization

- When using the wake circuit (USE_WAKE_CIRCUIT=1):
  - The device will only wake up when the trap is triggered or for the heartbeat interval
  - Sleep time is set to the HEARTBEAT_INTERVAL_HOURS (default 24 hours) instead of the 30-minute polling interval
  - This significantly reduces power consumption since the device doesn't need to wake up every 30 minutes
  - The wake circuit will immediately wake the device if the trap triggers, ensuring no events are missed
- Use the built-in diagnostic mode by holding the boot button during initial power-up (not available during wake from sleep):
  - The RGB LED will indicate sensor states in diagnostic mode:
    - Green: Mouse trap sensor triggered
    - Red: Battery sensor triggered
    - Yellow: Both sensors triggered
    - Blue: No sensors triggered
  - LED only activates in diagnostic mode to conserve power during normal operation
  - Diagnostic mode can only be entered during initial power-up, not during wake from sleep cycles
- For wake circuit troubleshooting:
  - Enter diagnostic mode by holding the button during boot
  - The diagnostic mode will automatically detect if wake circuit is enabled
  - Adjust the trim potentiometer until the LED turns green when the trap is triggered
  - The console will display both wake pin state and LDR readings
  - If the wake circuit is not working, check connections and verify the comparator is receiving power
  - Make sure the LDR is properly positioned to detect the trap's LED
  - In diagnostic mode with wake circuit enabled, the device will:
    - Show LED status based on both wake pin and battery sensor states
    - Not attempt to connect to WiFi or publish MQTT messages
    - Not enter sleep mode
    - Continuously monitor the wake pin state
- If WiFi connection fails during heartbeat:
  - The device will not reset its heartbeat counter and will try again on the next wake cycle
  - It will continue trying on each wake cycle until it successfully connects
  - No "offline" status is published until a successful connection is made and then lost
  - Home Assistant will continue to show the last known state until a successful update
  - WiFi issues won't affect the device's ability to monitor the trap - all states are preserved
- If you encounter build issues with macros in common.h, ensure there are no trailing backslashes at the end of lines
- If you see duplicate MQTT messages in the logs (e.g., "MQTT Connected" appearing twice), update to the latest version which fixes an issue with duplicate event handler registration in mqtt_manager.c

## License

This project is open source and available under the MIT License.
