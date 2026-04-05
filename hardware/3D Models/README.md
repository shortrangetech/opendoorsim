# 3D Print Instructions
The following print instructions and settings are what I use for the official cases at [shortrange.tech](https://shortrange.tech). You may try to use materials other than ASA, however, please note that you will definitely need to change the settings if you are using a different material.

## My Personal Printer Setup

> [!NOTE]
> 
>Links below are affiliate links.
>If you purchase within 24hrs of clicking I may receive a small commission, which helps support the OpenDoorSim project at no extra cost to you. Thank you!

| Item           | Details                                  | Link                                                                                                                                                                                                                                                                                                                                                        |
| -------------- | ---------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Printer        | Prusa Core One+                          | Core One +: [Prusa3D](https://www.prusa3d.com/product/prusa-core-one-12/?p2p=%40shortrange_3431163)<br>Core One + Kit: [Prusa3D](https://www.prusa3d.com/product/prusa-core-one-kit-12/?p2p=%40shortrange_3431163)<br>or<br>**Use referral code @shortrange_3431163** for a free filament roll and extra prusameter reward points on any Prusa Printer.<br> |
| Print Plate    | Prusa Textured Powder Coated Print Sheet | [Prusa3D](https://www.prusa3d.com/product/textured-powder-coated-print-sheet/)                                                                                                                                                                                                                                                                              |
| Print Material | PolyMaker Fiberon ASA Carbon Fiber       | 0.5kg Roll: [Amazon](https://amzn.to/4c1Yvve)<br>3kg Roll: [Amazon](https://amzn.to/4m8RcXg)                                                                                                                                                                                                                                                                |

## Slicer Settings
The slicer settings I use can be found below. I use PrusaSlicer. 

### Print Settings

| Print Setting | Value  |
| ------------- | ------ |
| Layer Height  | 0.2mm  |
| Perimeters    | 3      |
| Seam Position | Random |
| Infill        | 15%    |
| Fill Pattern  | Gyroid |

### Temperature Settings

| Temperature Setting  | Value |
| -------------------- | ----- |
| Nozzle, First Layer  | 265 C |
| Nozzle, Other Layers | 265 C |
| Bed                  | 110 C |
| Chamber, Nominal     | 55 C  |
| Chamber, Minimal     | 40 C  |

### Cooling Settings

| Cooling Setting           | Value    |
| ------------------------- | -------- |
| Bridges Fan Speed         | 30%      |
| Disable fan for the first | 5 layers |


### Modifier Settings
I place small modifier cubes around the following locations, for the following cases, with the following settings to help print stability and durability:

| Location                                                  | Setting                | LAB | Pocket |
| --------------------------------------------------------- | ---------------------- | --- | ------ |
| Threaded Insert Holes, Surrounding Area                   | Cube, 100% Infill      | ✅   | ✅      |
| Screw Head Holes, Surrounding Area                        | Cube, 100% Infill      | ✅   | ✅      |
| USB-C Hole                                                | Cube, Support Enforcer | ✅   | ✅      |
| Thin PCB Slot Below USB-C Hole, Internal                  | Cube, Support Enforcer | ✅   | ❌      |
| Through Strap "C" Slots, Surrounding and Supporting Areas | Cube, 100% Infill      | ✅   | ❌      |
| Slider Side Arch                                          | Cube, Support Enforcer | ❌   | ✅      |
| Encoder Hole                                              | Cube, Support Enforcer | ❌   | ✅      |
| Horizontal Slider Pegs                                    | Cube, Support Enforcer | ❌   | ✅      |


