# Sunrise Clock

## Overview

The Sunrise Clock is a device that is similar to an alarm clock (in concept), that, using a cercadian LED light bulb 
(similar to this one on Amazon:  https://www.amazon.com/gp/product/B08VC71HSG/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) 
gradually turn on the light bulb at the designated alarm time, until it is fully on.  Circadian lights simulate the natural outside light 
and promotes better sleep by emiting specific wavelengths that simulate the sun rising in the morning.

## Parts is parts...

The Sunrise Clock has two parts to it:

### The Sunrise Clock Dimmer (This repository) 

This is a Spark Fun Pro Micro board (or equivilent), which manages the actual dimming. The Sunrise Clock Webserver
communicates to this board through a serial connection to send commands to it:

| Command | Description |
| snnn | Set dim level manually |
| o | Turn light fully on |                             
| f | Turn light fully off |                            
| thh:mm:ss | Set current time |                                
| ahh:mm:ss | Set alarm time and turn alarm on | 
| a | Turn off alarm if it is on, or turn on if off |
| c | Cancel alarm if it has been triggered |   
| q | Query current time and alarm time |           
| w | Set wake up time in secs if default not desired. |
| d | Force alarm going off. |

For debugging purposes, you can type these commands into Arduino's USB serial monitor.

### The Sunrise Clock Webserver
This is an ESP-01S board. This board has an ESP8266 chip on it and 1 meg of memory.

