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

---

## 📸 Galería del Proyecto (Hardware & Build)

A continuación se muestra el proceso de ensamble, cableado y pruebas del **Cyberdeck Mini S3**.

| | | |
|:---:|:---:|:---:|
| ![1](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/1.jpg?raw=true) | ![2](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/2.jpg?raw=true) | ![3](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/3.jpg?raw=true) |
| ![4](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/4.jpg?raw=true) | ![5](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/5.jpg?raw=true) | ![6](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/6.jpg?raw=true) |
| ![7](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/7.jpg?raw=true) | ![8](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/8.jpg?raw=true) | ![9](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/9.jpg?raw=true) |
| ![10](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/10.jpg?raw=true) | ![11](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/11.jpg?raw=true) | ![12](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/12.jpg?raw=true) |
| ![13](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/13.jpg?raw=true) | ![14](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/14.jpg?raw=true) | ![15](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/15.jpg?raw=true) |
| ![16](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/16.jpg?raw=true) | ![17](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/17.jpg?raw=true) | ![18](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/18.jpg?raw=true) |
| ![19](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/19.jpg?raw=true) | ![20](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/20.jpg?raw=true) | ![21](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/21.jpg?raw=true) |
| ![22](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/22.jpg?raw=true) | ![23](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/23.jpg?raw=true) | ![24](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/24.jpg?raw=true) |
| ![25](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/25.jpg?raw=true) | ![26](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/26.jpg?raw=true) | ![27](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/27.jpg?raw=true) |
| ![28](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/28.jpg?raw=true) | ![29](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/29.jpg?raw=true) | ![30](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/30.jpg?raw=true) |
| ![31](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/31.jpg?raw=true) | ![32](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/32.jpg?raw=true) | ![33](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/33.jpg?raw=true) |
| ![34](https://github.com/pepeangell5/CYBERDECK-MINI-ESP32/blob/main/img/34.jpg?raw=true) | | |

---




## ⚠️ Advertencia Legal (Disclaimer)

Este proyecto ha sido desarrollado con fines exclusivamente educativos y de auditoría de seguridad ética. El uso de este equipo para interrumpir comunicaciones sin autorización previa es ilegal en muchas jurisdicciones. El desarrollador no se hace responsable del mal uso de esta herramienta.

---

**Desarrollado por PepeAngell** 📍 Los Mochis, Sinaloa, México | 2026  
*Software Engineer | Electronics Enthusiast*