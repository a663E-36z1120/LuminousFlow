# 🌊 **Project Luminous Flow**
***Real-time fluid simulation on an LED matrix display*** responding to ***live accelerometer data***, rendered at 60+ FPS by a ***simple GPU built from scratch*** with Arduino and logic IC chips — continuum mechanics in a purely digital circuit!

<p float="left">
  <img src="Assets/Demo.gif" height="250" />
  <img src="Assets/Circuit.jpg" height="250" />
</p>

## Physics Engine
The physics engine of our fluid simulation implements [Smoothed-particle hydrodynamics (SPH)](https://en.wikipedia.org/wiki/Smoothed-particle_hydrodynamics) proposed by Gingold and Monaghan in 1977 [[1]](#1). This can be found in [SPHEngine.cpp](SPHEngine.cpp), which in every cycle takes as input a 2D instantaneous global acceleration in polar coordinates to update the positions of particles in the simulation.

💡 If you want to use our physics engine for your own project, we've included an isolated version of it in [Prototyping/SPHEnginePybind.cpp](Prototyping/SPHEnginePybind.cpp) that you can interact with through a Jupyter notebook such as [Prototyping/physics_testbench.ipynb](Prototyping/physics_testbench.ipynb), thanks to the [pybind11](https://github.com/pybind/pybind11) project. A Python implementation that we used in the early stages of this project is also available in [Prototyping/physics_testbench_py.ipynb](Prototyping/physics_testbench_py.ipynb).


⚠️ Please note that this physics engine is designed to be compiled and ran on a PC instead of a microcontroller.


<p float="left">
  <img src="Assets/engine.gif" height="240" />
  <img src="Assets/hashgrid.gif" height="240" />
</p>


___

<a id="1">[[1]](https://academic.oup.com/mnras/article/181/3/375/988212)</a> 
Gingold, R. A., & Monaghan, J. J. (1977). *Smoothed particle hydrodynamics: theory and application to non-spherical stars.* Monthly notices of the royal astronomical society, 181(3), 375-389.

Created by 
[@a663E-36z1120](https://www.github.com/a663E-36z1120)
&
[@AlexZengXi](https://www.github.com/AlexZengXi); shoutout to [Prof. Ziqing Hong](https://zqhong.physics.utoronto.ca/) from the Department of Physics at University of Toronto for teaching the amazing [PHY405H1: Electronics Lab](https://artsci.calendar.utoronto.ca/course/phy405h1) course and supporting this project!

___


# **Detailed Technical Documentations for the Nerds**
<img src="Assets/Circuit.jpg" height="250" />
*Figure 1: Project Luminous Flow in Its Entirety *

## System Architecture
We are designing an interactive LED matrix that simulates fluid motion in response to the system's tilt. Figure 2 shows the functional block diagram of the system. 

<img src="Assets/Project Functional Block Diagram.png" height="250" />
*Figure 2: Functional Block Diagram*

As illustrated in Figure 1, the system architecture is divided into three primary sections. The far-right segment comprises four full-sized breadboards, collectively housing our custom-designed GPU. This region includes the main Arduino UNO, responsible for generating all driving signals, and the array of logic IC chips that help distribute the signals.

To the left, a pair of breadboards make up the LED display section. This section houses the 16×9 LED matrix, the accelerometer, all the nMOSFETs, and the current-limiting resistors.\\

Finally, the single breadboard in between acts as an interface board. It organizes the signal connections from the logic gate array to the LED display, simplifying testing and debugging. This board also holds the secondary Arduino, which relays accelerometer data to the physics engine for real-time simulation. 

The Build of Materials can be found below in Table 1.

<img src="Assets/Project Build of Materials.png" height="250" />
*Table 1: Build of Materials*


## Fluid Physics Engine
<img src="Assets/Fluid Physics Engine Pseudocode.png" height="250" />
*Algorithm 1: Core Abstract Operations of the SPH Physics Engine*

## Accelerometer

## Communication Protocol 

## Graphics Processing Unit
<img src="Assets/Arduino Bit-Bang Pin Mapping for Addressing the Multiplexed LED Matrix.png" height="250" />
*Table 2: Arduino Bit-Bang Pin Mapping for Addressing the Multiplexed LED Matrix. (See Table 3: Pixel Addressing) Pin 11 is used for the PWM Signal Broadcast. *


<img src="Assets/GPU Top Down Photo.jpg" height="250" />
*Figure 3: Photo of the GPU we constructed. On the rightmost 3 breadboard columns: **top left:** 4x SN7442N decimal decoder array. **top center & top right:** 6x SN7404 hex inverter (NOT gate) array. **bottom left, center, & right:** 9x SN7408 AND gate array. Wire colors: **Orange & Blue:** internal digital signal wires, alternates every 4 bit and after passing through NOT gates. **Yellow:** PWM signal broadcast wires. **Green:** Output signal wires. **Red:** 5V Vcc. **Black:** Ground.*

## LED Matrix and nMOSFET Array
<img src="Hardware/LED Matrix 2973 Adafruit Schematics & Layout & Simulatoin/led_matrix_matrixsch.png" height="250" />
*Figure 4: 2973 Adafruit 16x9 Blue LED Matrix Schematic*

<img src="Assets/nMOSFETs Controls for Each LED Matrix Pin.png" height="250" />
*Figure 5: Schematics of the Pull-Up and Pull-Down Setup for Control Pin 0 from the LED Matrix as Example*

<img src="Hardware/LED Matrix 2973 Adafruit Schematics & Layout & Simulatoin/LUT Pull-Up and Pull-Down Pin Configuration for Each LED Activation.png" height="250" />
*Table 3: Configuration values for LED matrix control. For each LED position, the table shows which pins should be set to 5V and which to GND*

## Power Supply
<img src="Assets/Multiplexed LED Matrix and nMOSFETs Drivers Section.png" height="250" />
*Figure 6: Photo of the LED Display and Driver Section, consists of 36x 2N7000 nMOSFETs, 16x 100R Resistors and the LED display. (the accelerometer is missing in this photo). Wire colors: **Green & Blue on the right:** nMOSFETs' gate signal wires connecting to the interface board. **White:** Wires Connecting to the LED display. **Red:** 5V Power Supply. **Black:** Ground.**

