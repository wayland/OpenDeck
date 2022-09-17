# ModularBoard

The ModularBoard concept is that every setup will start off with a singular 
Base Board attached to any number of Breakout Boards.  These Breakout Boards can 
be of different varieties, but share a common pin header daisychain.

The intention is that, if the major version (the 1 in v1.0.0) is the same, then 
the pin headers will be the same.

The boards are as follows:
-	**Base Board:** Contains all the necessary components for an OpenDeck board; 
	basically the OpenDeck 3, but with all the parts in the Breakout boards 
	removed.
-	**Classic Breakout Board:** This plus a BaseBoard is basically an OpenDeck 3 
	with a few minor changes
-	**Standard Breakout Board:** Like the Classic Breakout Board, except:
	-	Only one Analog MUX, but the board can chain better
	-	Only 1 set of buttons
-	**3x3 Breakout Board:** Designed for lots of buttons with LEDs; supporting 
	boards are:
	-	3x3 keypad (x4)
	-	2x2 keypad (x3)

## Base Board

**Description:** Contains all the necessary components for an OpenDeck board; 
basically the OpenDeck 3, but with all the parts in the Breakout boards removed.

### Planning
-	**Status:** Schematic complete, no PCB
-	**Intention:** Tim will do PCB if no-one else does

### Features
-	**Power:** Micro USB (Arduino Nano 33 BLE) or USB C (Black Pill)
-	**MCU:** nRF52840 (Arduino Nano 33 BLE) or STM32F411CE (Black Pill)
-	**DIN MIDI:** Yes
-	**DMX:** Yes
-	**LED indicators:** Yes
-	**ModularDeck Output Header:** 1

## Classic Breakout Board

**Description:** This plus a BaseBoard is basically an OpenDeck 3, except that the 
74HCT125 is replaced with a TXB0104PW (so that we can daisy-chain out the LEDs).
Doesn't daisy-chain the MUXs very well (see Standard Breakout Board for that).

### Planning
-	**Status:** Schematic complete, no PCB
-	**Intention:** None

### Features
-	**ModularDeck Input Header:** 1
-	**ModularDeck Output Header:** 1 (Straight Through)
-	**Digital inputs:** 128
-	**Digital outputs:** 64
-	**Analog inputs:** 64

## Standard Breakout Board

**Description:** Like the Classic Breakout Board, except:
-	Only one Analog MUX, but the board can chain better
-	Only 1 set of buttons

### Planning
-	**Status:** Schematic complete, no PCB
-	**Intention:** None

### Features
-	**ModularDeck Input Header:** 1
-	**ModularDeck Output Header:** 1 (Analog Shift 1)
-	**Digital inputs:** 64
-	**Digital outputs:** 64
-	**Analog inputs:** 16

## 3x3 Breakout Board

**Description:** Has headers for 3x3 and 2x2 LED keypads (ie. each key has its 
own LED).  The keypads have their own circuit boards (which do the diodes).

**Intended use:** For places that do speaking events with one or more speakers 
giving a presentation on a stage.
-	**The space:** A stage with say 8 different areas (4 upstage, 4 downstage)
-	**The control surface:** Has 8 different areas representing the stage areas
-	**Each area:**
	-	*Stage:* Has up to 9 devices (lights, microphones, cameras, HDMI 
		computers) associated with it
	-	*Control Surface:* Has 9 LED keys (3x3) representing the different 
		devices.  Some stage areas might warrant fewer devices, and only need a 
		2x2 keypad.  There will also be extra buttons, LEDs, and some pots.  

If more stage areas are needed, simply chain more boards together (should work 
up to at least 2, hopefully 4).

### Planning
-	**Status:** Schematic complete, no PCB
-	**Intention:** Tim will do PCB if no-one else does

### Features
-	**ModularDeck Input Header:** 1
-	**ModularDeck Output Header:** 1 (Analog Shift 1)
-	**3x3 keypad headers:** 4
-	**2x2 keypad headers:** 3
-	**Digital inputs:** 16 (excluding keypads)
-	**Digital outputs:** 16 (excluding keypads)
-	**Analog inputs:** 16
