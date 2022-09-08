ModularBoard
============

The ModularBoard concept is that every setup will start off with a singular 
Base Board attached to any number of Breakout Boards.  These Breakout Boards can 
be of different varieties, but share a common pin header daisychain.  

The intention is that, if the major version (the 1 in v1.0.0) is the same, then 
the pin headers will be the same.  

The boards are as follows:

Base Board
----------

Contains all the necessary components for an OpenDeck board; basically the 
OpenDeck 3, but with all the parts in the Breakout boards removed.  

Status: Schematic complete, no PCB

Classic Breakout Board
----------------------

This plus a BaseBoard is basically an OpenDeck 3, except that the 74HCT125 is 
replaced with a TXB0104PW (so that we can daisy-chain out the LEDs).  

Status: Schematic complete, no PCB

Standard Breakout Board
-----------------------

Like the Classic Breakout Board, except with only one Analog MUX, and the MUX 
can daisy-chain.  This means only 15 usable channels per board.  

Status: Not started

3x3 Breakout Board
------------------
Has headers for 3x3 and 2x2 LED keyboards, which have their own circuit boards.  

Status: Not started
