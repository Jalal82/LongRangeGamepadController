# Long-Range Gamepad Controller for RC Vehicles and Drones


## Overview
This project transforms a Bluetooth/BLE gamepad into a long-range controller for RC cars and drones using two ESP32 microcontrollers. By leveraging ESP-NOW technology with long-range antennas, it enables control over distances of up to 1100 m. This system supports various protocols, including SBUS, UART, and PPM, making it versatile for different applications.

## Features
- **Long Range**: Control RC vehicles and drones up to 1100 km away.
- **Protocol Support**: Compatible with SBUS, UART, and PPM protocols.
- **End-to-End Encryption**: Ensures secure communication between devices.
- **Low Latency**: Response time of up to 10 ms for real-time control.
- **Fail-Safe Mechanisms**: Built-in features to ensure reliable operation.
- **Target Server Selection**: Choose the target server or transmitter for data transmission.
- **Clock Frequency**: Set the clock frequency to 240 MHz for improved performance and to achieve the 10 ms response time.
- **BLE Gamepad Configuration**: Change the Bluetooth/BLE MAC address of the gamepad to allow filtering from other devices and enable auto-connect.


## Hardware Requirements
- 2 x ESP32 microcontrollers
- Bluetooth/BLE gamepad
- Long-range antennas
- target compatible with SBUS (Flight controller  RC vehicle)

## Software Requirements
- ESP-IDF framework version 5.3 or Later

## Setup Instructions
1. **Clone the Repository**:
```bash 
git clone https://github.com/yourusername/LongRangeGamepadController.git 
cd LongRangeGamepadController
```
2. **Install ESP-IDF**:
   - Follow the instructions on the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to set up the ESP-IDF framework version 5.3.

3. **Configure the Project**:
   - Navigate to the project directory and run:
```bash 
idf.py menuconfig
```
- Set up your ESP-NOW Encryption key and channel and other configurations as needed.
  
5. **Pair the Gamepad**:
 - Change the Bluetooth/BLE MAC address of the gamepad in the configuration to allow filtering from other devices and enable auto-connect.

6. **Build and Flash the Code**:
   - Build the project and flash it to both ESP32 devices using:
```bash 
   idf.py build 
   idf.py flash
```

7. **Connect to RC Vehicle/Drone Flight controller**:
   - Connect the output of the ESP32 to your RC vehicle or drone using the appropriate protocol (SBUS).

## Usage
- Once set up, use the gamepad to control your RC vehicle or drone. Ensure that all connections are secure and that the devices are powered on.

## Contributing
Contributions are welcome! If you have suggestions for improvements or find bugs, please open an issue or submit a pull request.

## License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.

## Acknowledgments
- [ESP32](https://www.espressif.com/en/products/hardware/esp32/overview) for the microcontroller.
- [ESP-NOW](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/wifi/esp_now.html) for long-range communication.
- [ESP-IDF](https://github.com/espressif/esp-idf) for the development framework.

## Contact
For questions or inquiries, please contact [elkamali.abdeljalil@gmail.com].
