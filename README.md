# Robotic ASL Hand

An assistive technology and educational device designed to translate text and speech input into American Sign Language (ASL) finger-spelling. 

Developed as an Electrical and Computer Engineering project at Purdue University, this system bridges communication gaps by accepting input via a USB keyboard or microphone, processing the input through an STM32 microcontroller and Raspberry Pi 4, and outputting the corresponding ASL letters using a 24-DOF servo-controlled robotic hand. 



## Key Features

* **Dual Operating Modes:** * *Translation Mode:* Translates user keyboard or voice input directly into ASL signs.
    * *Learn Mode:* Generates random letters for the user to guess, providing interactive "Correct/Incorrect" feedback on an LCD screen.
* **High-Accuracy Speech-to-Text:** Processes audio within 10 seconds with >90% accuracy using a dedicated Raspberry Pi 4.
* **Fluid & Readable Movements:** Signs at a rate of 30 letters per minute with a letter transition and settle time of 1.6s. 
* **Accessible Output:** Features a 3D-printed, bio-degradable PLA hand that clearly distinguishes similar letters (e.g., "V" vs. "W") with minimal servo jitter.

## System Architecture



Our system is divided into five core subsystems working together to ensure low-latency communication and precise motor control:

| Subsystem | Components & Protocols | Function |
| :--- | :--- | :--- |
| **Microcontroller** | STM32MP157, I2C, SPI, DMA, PWM | The system's brain. Manages buffers, calculates motor movement sequences, and handles peripheral interrupts. |
| **Motor Drivers & Hand** | PCA9685 (x2), 24x Servos (Various weights) | Articulates the 3D-printed hand. Daisy-chained drivers use I2C to generate precise PWM signals for each finger joint and arm axis. |
| **Microphone** | Cardioid USB Mic, Raspberry Pi 4 | Captures audio and processes computationally intensive Speech-to-Text (STT) models locally. |
| **Keyboard & LCD** | USB Keyboard, SPI LCD | Serves as the primary UI. Uses USB HID protocols for input and DMA-based SPI transfers for non-blocking screen updates. |
| **Power** | 5V/30A SMPS, Boost/Buck Converters | Provides safe, filtered, and regulated power to all components using TVS and Schottky diodes to prevent surges. |



## Hardware & Software Stack

* **Embedded Programming:** C, Python
* **Microcontrollers & Microprocessors:** STM32MP157, Raspberry Pi 4
* **Communication Protocols:** I2C, SPI, USB HID
* **Motor Control:** 16-Channel PWM Servo Drivers (PCA9685)
* **Audio Processing:** Custom STT audio queuing and buffering 

## Getting Started

### Prerequisites

* [List any specific software requirements, e.g., STM32CubeIDE, Python 3.x]
* [List any specific libraries or dependencies for the Raspberry Pi STT model]

### Installation & Build

1. Clone the repository:
   `git clone https://github.com/your-username/robotic-asl-hand.git`
2. [Provide instructions on how to flash the STM32 board]
3. [Provide instructions on how to set up the Raspberry Pi environment]
4. [Provide any wiring or mechanical assembly notes]

### Usage

1. Power on the SMPS to supply power to the motor drivers and microcontrollers.

## System Diagrams

* **System Block Diagram:** [Insert link/image to System Block Diagram]
* **System Activity Diagram:** [Insert link/image to System Activity Diagram]
* **Schematics & PCB Layouts:** [Insert link/image to PCB/Schematics]

## Team Members

* **Ameen Abubakr** - Electrical Engineering (Purchasing, Embedded Software, AI/ML, Motor Control)
* **Demir Kaya** - Computer Engineering (Communicator, AI/ML, Software/Electrical Development)
* **Eric Bartczak** - Computer Engineering (Team Lead, Programming, Microcontrollers)
* **Arushi Gupta** - Computer Engineering (Communicator, AI/ML, Signal Processing, Task Management)
* **Xander Van Den Nieuwenhuizen** - Electrical Engineering (Facilitator, Mathematics, Systems Design)

## Acknowledgments

* **Professor:** Fengqing Zhu
* **GTA:** Yuhang Zu
* Purdue University Elmore Family School of Electrical and Computer Engineering
