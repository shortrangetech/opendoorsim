# OpenDoorSim Official Build Guide
This manual provides complete instructions for building your very own **OpenDoorSim LAB** or Pocket, assuming you have all parts on hand.  

## Contents:  

| [BUILD The Board (Part 1)](#build-the-board-part-1)                   | Build and connect the main PCB with components.                       |
| --------------------------------------------------------------------- | --------------------------------------------------------------------- |
| [ASSEMBLE The OpenDoorSim (Part 2)](#assemble-the-opendoorsim-part-2) | Assemble the board and accessories into the final OpenDoorSim device. |
| [FLASH The Firmware (Part 3)](#flash-the-firmware-part-3)             | Flash and test the microcontroller firmware.                          |


> [!TIP]
> 
>🚀 **SUPPORT THE PROJECT!**
>You can support the project by purchasing an official build kit [here](https://shortrange.tech) or by buying your own components through affiliate links at no extra cost to you. Or, just tell your friends about the project! Thank you for your support. ❤️ 


> [!NOTE]
> 
>Links below are affiliate links.
>If you purchase within 24hrs of clicking I may receive a small commission, which helps support the OpenDoorSim project at no extra cost to you. Thank you!


## BUILD The Board (Part 1)
This section contains instructions for building the main OpenDoorSim PCB, including core component connections and case fit preparation. 

### Bill of Materials
To complete Part 1 of the build, you will need the following tools and components. Depending on whether you're building the Lab or Pocket, you will need different components! 

📈 **Table Key:**  

| ✅                 | Essential  |
| ----------------- | ---------- |
| 🟡                | Optional   |
| ❌                 | Not Needed |


| **Tool**s                   | LAB | POCKET | Link |
| :-------------------------- | :-- | :----- | :--- |
| Soldering Iron              | ✅   | ✅      |      |
| Solder                      | ✅   | ✅      |      |
| Wire Cutters                | ✅   | ✅      |      |
| Small Screwdriver           | ✅   | ✅      |      |
| Painter's Tape *(Optional)* | 🟡  | 🟡     |      |
| Wire Strippers *(Optional)* | 🟡  | 🟡     |      |
| Pliers *(Optional)*         | 🟡  | 🟡     |      |

| Quantity | Components                             | LAB | POCKET | Link |
| -------- | -------------------------------------- | --- | ------ | ---- |
| 1x       | **OpenDoorSim PCB**                    | ✅   | ✅      |      |
| 1x       | **ESP-32 Microcontroller (30-Pin)**    | ✅   | ✅      |      |
| 6x       | **Screw Terminal (2-Pin)**             | ✅   | ✅      |      |
| 1x       | **KY-040 Encoder and Knob**            | ✅   | ✅      |      |
| 1x       | **128x64 OLED, 2.42"**                 | ✅   | ❌      |      |
| 9x       | **Solid Core Wire, 100mm Long, 22awg** | ✅   | ❌      |      |
| 1x       | **128x64 OLED, 0.96"**                 | ❌   | ✅      |      |
| 9x       | **Solid Core Wire, 60mm Long, 22awg**  | ❌   | ✅      |      |


> [!NOTE]
> ⚡️ **TIP**
>  I typically color scheme the 9x wire bundle as follows: 2x Red (Power), 2x Black (Ground), 2x Blue (Clock), 2x Yellow (Data), 1x Green (Switch)
> 

### Instructions

#### **Step 1: Microcontroller**

1. Align and insert the **ESP32** into the holes on the top of the PCB.
2. Optionally, secure the ESP32 flat against the board with tape.
3. Solder the **four corner pins** first to lock in alignment.
4. Ensure the ESP32 is completely flush with the PCB.
5. Solder the remaining pins.
6. Remove tape, excess solder.
#### **Step 2: Screw Terminal Blocks**

1. Pair up the 2 pin terminal blocks by sliding them together, making 3 sets.
2. Insert them into the designated holes on the PCB, facing out.
3. Optionally, secure with tape.
4. Solder the outside pins to lock in alignment.
5. Ensure the screw terminals are completely flush with the PCB.
6. Solder the remaining pins.
7. Remove tape, excess solder.

#### **Step 3: Wire Preparation**

1. Strip a small amount of insulation (~3-5mm) from both ends of the colored wires.

#### **Step 4: Wiring the PCB**

1. Thread the wires through the top of the OpenDoorSim PCB. There should be 4 OLED through holes, and 5 encoder through holes for a total of 9 wires. I suggest you follow the color-code standard below:
	- **Red:** Voltage (VCC/5V/3V).
	- **Black:** Ground (GND).
	- **Blue:** Clock (CLK).
	- **Yellow:** Data (SDA).
	- **Green:** Switch (SW).
2. Optionally, secure with tape.
3. Solder each wire to the board with a healthy bead of solder.
4. If desired, carefully add solder to the wire-side connection, avoiding the wire's own insulation. This "double bead" creates a stronger connection, helping avoid future wire snaps.
5. Remove tape, excess solder.
    
#### **Step 5: Preparing Accessories**

1. If your OLED screen or encoder has pre-installed pin headers, they must be removed for the wires to connect to them, and for the boards to fit in their precision casings.
2. To remove the pin headers, heat each pin with your iron and use pliers to gently wiggle and pull them out. This may take some patience!

#### **Step 6: Final Connections**

1. Thread the colored wires from the PCB into the corresponding screen and encoder components. 
	- ❗️Ensure wires are entering from the **underside** of the accessory boards. Otherwise, they will not fit in the case!
	- ❗️Note that the order of holes on the PCB is not necessarily the same order as your accessory's holes. Ensure that the correct connections are being made.
2. Optionally, use tape to secure the wires. 
3. Solder the wires to their component boards with a healthy bead of solder.
4. If desired, carefully add solder to the wire-side connection, avoiding the wire's own insulation. This "double bead" creates a stronger connection, helping avoid future wire snaps.
5. Remove tape, excess solder.
    
#### **Step 7: Clean Up Leads**

1. Use wire cutters to snip all excess component and wire leads on the bottom of the PCB and accessories.
2. Ensure all snips are flush to prevent shorts when the board is mounted in its casing.

### Final Thoughts

Congratulations on completing your OpenDoorSim board build! Give yourself a pat on the back, grab a snack, and get ready for part 2!


## ASSEMBLE The OpenDoorSim (Part 2)
This section contains instructions for putting everything together - preparing your incredible case, inserting magnets and threaded inserts, placing the board inside the case, and closing up the build!

> [!NOTE]
> ⚡️***TIP***
>*If you want to make sure your board build works before putting it in the case, you may prefer to flash and test the board (Part 3) before assembling (Part 2). Otherwise, you may have to disassemble to troubleshoot properly. If that doesn't bother you, carry on!*

### Bill of Materials
To complete Part 2 of the build, you will need the following tools and components. Depending on whether you're building the Lab or Pocket, you will need different components! 

📈 **Table Key:**  
✅ Essential  
🟡 Optional  
❌ Not Needed  

| **Tool**s                                                                                    | LAB | POCKET | Link |
| :------------------------------------------------------------------------------------------- | :-- | :----- | :--- |
| Soldering Iron and/or Threaded Insert Press                                                  | ✅   | ✅      |      |
| Small Screwdriver or General Tool for Poking and Scraping                                    | ✅   | ✅      |      |
| Allen Key, 2.5mm                                                                             | ✅   | ✅      |      |
| Allen Key, 1.5mm                                                                             | ❌   | ✅      |      |
| Thin String or Thread *(Optional. Necessary if you don't know the polarity of your magnets)* | ✅   | ❌      |      |
| Super Glue, Commercial Strength *(Optional, Highly Recommended)*                             | 🟡  | 🟡     |      |
| Super Glue Accelerator *(Optional, if you're impatient)*                                     | 🟡  | 🟡     |      |

| Quantity | Components                                | LAB | POCKET | Link |
| -------- | ----------------------------------------- | --- | ------ | ---- |
| 1x       | **ODS Lab Case (Body, Lid, 2x End Caps)** | ✅   | ❌      |      |
| 1x       | **ODS Pocket Case (Body, Slider, Lid)**   | ❌   | ✅      |      |
| 12x      | **Threaded Insert, M3x4**                 | ✅   | ❌      |      |
| 9x       | **Screw, Hex Socket Head M3x6**           | ✅   | ❌      |      |
| 3x       | **Screw, Hex Socket Head M3x4**           | ✅   | ❌      |      |
| 4x       | **Neodymium Magnet, 20x10x5**             | ✅   | ❌      |      |
| 6x       | **Neodymium Magnet, 20x10x3**             | ✅   | ❌      |      |
| 1x       | **MagSafe Compatible Ring**               | ✅   | ❌      |      |
| 5x       | **Threaded Insert, M3x4**                 | ❌   | ✅      |      |
| 5x       | **Screw, Hex Socket Head M3x6**           | ❌   | ✅      |      |
| 4x       | **Threaded Insert, M2x3**                 | ❌   | ✅      |      |
| 4x       | **Screw, Hex Socket Head M2x4**           | ❌   | ✅      |      |
| 1x       | **Key Ring, 20mm**                        | ❌   | ✅      |      |
| 1x       | **Carabiner, 33mm**                       | ❌   | ✅      |      |
### Instructions

---
## Flash The Firmware (Part 3)