# 📟 CYBERDECK-MINI-ESP32 (PEPE-OS V2.1)

![License](https://img.shields.io/badge/License-Open%20Source-green)
![Hardware](https://img.shields.io/badge/Hardware-ESP32--S3-orange)
![Origin](https://img.shields.io/badge/Origin-Sinaloa%2C%20Mexico-red)
![Developer](https://img.shields.io/badge/Dev-PepeAngell-blue)

**CYBERDECK-MINI** es una herramienta portátil de auditoría de radiofrecuencia y redes basada en el potente **ESP32-S3**. Este dispositivo integra múltiples capacidades de ataque y monitoreo en la banda de 2.4GHz, GPS tracking y un entorno gráfico personalizado (PEPE-OS) optimizado para velocidad y sigilo.

---

## 🛠️ Especificaciones Técnicas (Hardware)

El sistema utiliza un **ESP32-S3 N8R2**, configurado para gestionar un bus SPI compartido de alta velocidad entre la pantalla, el almacenamiento SD y los módulos de radio.

### 🔌 Pinout & Conexiones

| Periférico | Pin Función | Pin ESP32-S3 | Nota Táctica |
| :--- | :--- | :--- | :--- |
| **Pantalla ST7789** | SDI (MOSI) / SCK | GPIO 11 / 12 | Bus SPI compartido. |
| **Pantalla ST7789** | CS / DC / RESET | GPIO 10 / 21 / 47 | Control de señal. |
| **Dual nRF24L01** | CE / CSN (#1 Scan) | GPIO 4 / 5 | Módulo de análisis. |
| **Dual nRF24L01** | CE / CSN (#2 Jam) | GPIO 6 / 7 | Módulo de saturación. |
| **MicroSD Slot** | CS / MOSI / MISO | GPIO 38 / 11 / 13 | Almacenamiento FAT32. |
| **Encoder Rotativo** | CLK / DT / SW | GPIO 48 / 46 / 3 | Navegación de precisión. |
| **Botones Físicos** | UP/DN/ENT/BACK | GPIO 1, 2, 42, 41 | Navegación rápida (GND). |
| **GPS NEO-6M** | TX / RX | GPIO 18 / 17 | Serial 1 Hardware. |
| **Buzzer** | Signal (+) | GPIO 14 | Feedback auditivo. |
| **Battery Monitor**| Divider (VCC) | GPIO 9 | Divisor 2.2k + 1k Ohms. |

---

## 📱 Suite de Aplicaciones (Firmware Modules)

El firmware está dividido en módulos `.cpp` especializados para maximizar el rendimiento del ESP32-S3:

* 📡 **WiFi Tool / Scanner:** Monitoreo y gestión de redes inalámbricas.
* ⚠️ **Beacon Spammer:** Generación masiva de tramas Beacon (SSID flooding).
* 😈 **Evil Portal:** Portales cautivos para auditoría de ingeniería social.
* 🛡️ **Packet Monitor:** Analizador de tráfico en tiempo real.
* 📍 **GPS Tracker:** Registro y visualización de coordenadas satelitales.
* ⚡ **NRF Jammer (Dual):** Ataque coordinado de 2.4GHz (Inundación de canales).
* 📊 **NRF Spectrogram:** Visualizador gráfico de interferencias RF.
* 🏥 **BLE Jammer:** Ataque agresivo contra protocolos Bluetooth con animación EKG roja.
* 📂 **File Explorer:** Navegador de archivos en la tarjeta SD.
* 🕹️ **Games Menu:** Sección de juegos arcade retro integrados.

---

## 🚀 Instalación y Compilación

1.  **Requisitos:** Tener instalado **VS Code** con la extensión **PlatformIO**.
2.  **Clonar Repo:** ```bash
    git clone [https://github.com/pepeangell5/CYBERDECK-MINI-ESP32.git](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32.git)
    ```
3.  **Librerías principales:**
    * `LovyanGFX`
    * `RF24`
    * `TinyGPSPlus`
4.  **Upload:** Conectar el S3 y ejecutar el comando de carga en PlatformIO.

---

## ⚠️ Advertencia Legal (Disclaimer)

Este proyecto ha sido desarrollado con fines exclusivamente educativos y de auditoría de seguridad ética. El uso de este equipo para interrumpir comunicaciones sin autorización previa es ilegal en muchas jurisdicciones. El desarrollador no se hace responsable del mal uso de esta herramienta.

---

**Desarrollado por PepeAngell** 📍 Los Mochis, Sinaloa, México | 2026  
*Software Engineer | Electronics Enthusiast*