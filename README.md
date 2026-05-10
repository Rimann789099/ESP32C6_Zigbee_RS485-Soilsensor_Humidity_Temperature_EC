# Zigbee Soil Monitor (ESP32-C6)

Firmware for ESP32-C6 integrating RS485 soil sensors into Zigbee networks.

## 🛠 Key Implementations
*   **Custom RS485 Driver:** High-reliability UART polling (4800 baud) for industrial sensors.
*   **Zigbee Stack:** EndPoint/Cluster management for Temp, Humidity, and EC.
*   **Visual Feedback:** Signal handler mapping network states to RGB LEDs (Green: Joined, Blue: Pairing, Red: Lost).
*   **Sync Logic:** Decoupled tasks for sensor polling and Zigbee attribute updates.

## 📂 Core Logic
*   **`app_main`**: Initializes NVS, GPIO interrupts, and launches main tasks.
*   **`esp_zb_task`**: Defines Zigbee device behavior and cluster attributes via `esp_zb_stack_main_loop`.
*   **`update_attribute`**: Periodic (3s) data scaling and injection into the Zigbee stack.
*   **`uart_rs485_soilsens`**: Manages low-level RS485 communication:
    1.  Flushes buffers and sends 8-byte requests.
    2.  Wait for TX synchronization.
    3.  Parses responses into shared memory via `DataPointers`.

## 🚦 Status Indicators
| Network State | Signal Code | LED Color |
| :--- | :--- | :--- |
| **Connected** | `BDB_SIGNAL_STEERING` (OK) | Green |
| **Searching** | `BDB_SIGNAL_STEERING` (Fail) | Blue |
| **Offline** | `ZDO_SIGNAL_LEAVE` / `UNAVAILABLE` | Red |

**Monitoring:** Run `idf.py monitor` for real-time sensor logs.
