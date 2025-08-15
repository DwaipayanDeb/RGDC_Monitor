# RGDC Rainwater TDS Monitor

**An open-source educational and research project by the Department of Physics, Ramanuj Gupta Degree College, Silchar, India.**

## Overview
The RGDC Rainwater TDS Monitor is a custom-built system for **real-time monitoring of Total Dissolved Solids (TDS)** in rainwater. Whenever it rains, the device collects a sample, measures the TDS, and uploads the results instantly to the cloud. All data is publicly available for research, educational use, and community awareness.

**Live data:** [rgdcmonitor.blogspot.com](https://rgdcmonitor.blogspot.com)  
**Source code:** [github.com/DwaipayanDeb/RGDC_Monitor](https://github.com/DwaipayanDeb/RGDC_Monitor)

---

## What is TDS?
TDS (Total Dissolved Solids) refers to the amount of inorganic salts, minerals, and small organic matter dissolved in water. It is measured via the water's electrical conductivity and expressed in **mg/L** or **ppm**.  
- Low TDS → purer water  
- High TDS → possible contamination or environmental impact

---

## Why This Project Matters
- Tracks environmental pollution from industrial, agricultural, and urban sources  
- Builds long-term environmental records for researchers and students  
- Supports climate and hydrology studies  
- Promotes community awareness on water safety

---

## System Architecture
The project consists of:
1. **Collection Unit** – Collects rainwater samples  
2. **Sampling Unit** – Uses a **Seeed Studio Grove Analog TDS Sensor** with an **ESP32 microcontroller** and **AD620 amplifier** to measure TDS  
3. **Distribution Unit** – Uploads data to Firebase and makes it accessible via a public website

---

## Features
- Real-time data uploads via WiFi
- Calibration against a standard TDS meter for accuracy
- Open-source hardware and software (GNU GPL-3.0 license)
- Free public data archive

---

## License
This project is licensed under the **GNU GPL-3.0**.  

---

## Contact
**Project Lead:** Dr. Dwaipayan Deb  
**Institution:** Department of Physics, Ramanuj Gupta Degree College, Silchar, Assam, India
