# STEP-1 PCB CONTENT

**Standalone FPGA board (LED blink)**

This is the **minimum complete system**. Remove anything here and the board stops being a real FPGA board.

---

## 1. FPGA Core Section (MANDATORY)

### Components

* iCE40UP5K FPGA (SG48 package)
* Decoupling capacitors:

  * **0.1 µF × one per VCC pin**
  * **1 µF × 1–2 near core**
* Optional bulk cap:

  * 4.7–10 µF near FPGA

### Why

* FPGA logic switches at MHz internally
* Local charge storage is mandatory
* Missing caps = random failure

---

## 2. Power Regulation Section (MANDATORY)

### Components

**Input side**

* USB-C / Micro-USB / DC jack
* Optional polyfuse
* Optional reverse-polarity diode

**3.3 V rail**

* 3.3 V LDO
* Input capacitor (1–10 µF)
* Output capacitor (1–10 µF)

**1.2 V rail**

* 1.2 V LDO
* Input capacitor
* Output capacitor

### Why

* FPGA core **cannot run on 3.3 V**
* I/O **cannot run on 1.2 V**
* LDO stability depends on caps

---

## 3. Clock Generation Section (MANDATORY)

### Components

* 12 MHz CMOS oscillator
* 0.1 µF decoupling capacitor

### Why

* FPGA has no internal free-running clock
* LED blink needs deterministic timing
* CMOS oscillator avoids crystal tuning errors

---

## 4. Configuration (Boot) Section (MANDATORY)

### Components

* SPI NOR Flash (SOIC-8)
* Pull-up resistors:

  * CS (≈10 kΩ)
  * Optional HOLD / WP pins
* 0.1 µF decoupling capacitor

### Why

* iCE40UP5K has **no internal flash**
* Without this, FPGA is blank every boot

---

## 5. LED Output Section (PROJECT GOAL)

### Components

* LED
* Series resistor (220–470 Ω)

### Why

* Confirms:

  * Power
  * Clock
  * Configuration
  * Pin mapping
  * HDL correctness

---

## 6. Programming / Access Section (MANDATORY)

### Components

* SPI programming header (2×3 or 1×6)
* GND reference pin
* 3.3 V reference pin

### Why

* Required to program flash initially
* Required for recovery if FPGA doesn’t boot

---

## 7. Reset & Status (STRONGLY RECOMMENDED)

### Components

* Pull-up resistor on CRESET
* Test point or LED on DONE pin
* Optional reset push button

### Why

* CRESET floating = undefined boot
* DONE tells you immediately if config worked

---

## 8. PCB Infrastructure (MANDATORY BUT OFTEN IGNORED)

### Components / Features

* Ground plane (solid)
* Power plane or wide pours
* Test points for:

  * 3.3 V
  * 1.2 V
  * GND
  * CLK
  * DONE

### Why

* You **cannot debug what you cannot probe**

---









# PART A — FPGA (iCE40-specific + general)

## 1. Mandatory (Read These First)

### 1. **Lattice iCE40UP5K Datasheet** (PDF)

* Official pin definitions, power requirements, configuration modes
* You should be able to answer:

  * What happens at power-up?
  * Which pins are sampled before configuration?
  * What voltage each pin tolerates?

Search:

> “iCE40UP5K Datasheet PDF lattice”

---

### 2. **Lattice iCE40 Hardware Checklist** (PDF)

* Board-level rules from the manufacturer
* Power sequencing, decoupling, pull-ups

Search:

> “Lattice iCE40 hardware checklist pdf”

This document alone explains **why 70% of beginner boards fail**.

---

### 3. **iCE40 Programming and Configuration User Guide** (PDF)

* SPI boot timing
* DONE / CRESET behavior
* Flash requirements

Search:

> “iCE40 programming configuration user guide pdf”

---

## 2. FPGA Toolchain & Open-Source Flow (Essential)

### 4. **Yosys Documentation**

* How HDL becomes gates
* What synthesis actually does

Website:

* yosys.readthedocs.io

Read sections:

* “Design flow”
* “Processes and always blocks”

---

### 5. **nextpnr-ice40 Documentation**

* Placement, routing, timing
* Why pin placement matters

Website:

* github.com/YosysHQ/nextpnr

---

### 6. **Project IceStorm**

* Reverse-engineered iCE40 internals
* Explains bitstreams at a physical level

Website:

* github.com/YosysHQ/icestorm

This is **how people really learned iCE40**.

---

## 3. Reference Designs (Study, Don’t Copy Blindly)

### 7. **iCEBreaker FPGA Board**

* Open hardware
* Clean schematics + layout

Search:

> “iCEBreaker FPGA schematic pdf”

Ask yourself:

* Why this capacitor value?
* Why this pull-up location?

---

### 8. **TinyFPGA BX**

* Minimalist but robust
* Good power and SPI layout

Search:

> “TinyFPGA BX schematic pdf”

---

# PART B — PCB Design (This Matters More Than HDL)

## 1. Absolute Must-Read PCB PDFs

### 9. **Texas Instruments – PCB Design Guidelines** (PDF)

* Power integrity
* Decoupling physics (not rules of thumb)

Search:

> “TI PCB design guidelines decoupling pdf”

---

### 10. **Analog Devices – Grounding and Layout** (PDF)

* Why ground planes matter
* Return current paths

Search:

> “Analog Devices grounding layout pdf”

---

### 11. **Howard Johnson – High-Speed Digital Design Notes**

* Even for “slow” FPGAs
* Explains signal integrity from physics

Search:

> “Howard Johnson high speed digital design pdf notes”

---

## 2. FPGA-Specific PCB Design

### 12. **Xilinx PCB Design Guide** (Vendor-agnostic principles)

Even though it’s Xilinx, **physics is the same**.

Search:

> “Xilinx PCB design guide pdf”

Focus on:

* Decoupling placement
* Power planes
* Clock routing

---

### 13. **Lattice FPGA PCB Design Guide** (PDF)

* iCE40-specific recommendations

Search:

> “Lattice FPGA PCB design guide pdf”

---

## 3. Practical Layout Learning (Real-World)

### 14. **Phil’s Lab (YouTube + GitHub)**

* Real schematics
* Real layout mistakes explained

Search:

> “Phil’s Lab PCB design”

This is one of the few channels that doesn’t lie to beginners.

---

### 15. **Robert Feranec – FEDEVEL**

* Power integrity
* FPGA and MCU boards

Search:

> “FEDEVEL FPGA PCB design”

---

## 4. KiCad-Specific (Tool Mastery)

### 16. **KiCad Documentation**

* Footprint verification
* Net classes
* Design rules

Website:

* docs.kicad.org

Focus on:

* “Footprint assignment”
* “PCB constraints”

---

# PART C — How to Study (Important)

Do **not** read everything linearly.

Use this loop:

1. Read **datasheet section**
2. Look at **reference schematic**
3. Ask: *why this exact connection?*
4. Confirm in **PCB guideline PDF**
5. Apply to your schematic

If you cannot explain a connection → you don’t understand it yet.

---

# Minimal Reading Path (If You Want a Short List)

If you read only **6 things**, read these:

1. iCE40UP5K Datasheet
2. iCE40 Hardware Checklist
3. iCE40 Configuration Guide
4. TI PCB Design Guidelines
5. Lattice FPGA PCB Design Guide
6. iCEBreaker Schematic

That’s enough to build Step-1 successfully.

