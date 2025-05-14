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
<img src="Assets/Circuit.jpg" height="400" />
*Figure 1: Project Luminous Flow in Its Entirety *

## System Architecture
We are designing an interactive LED matrix that simulates fluid motion in response to the system's tilt. Figure 2 shows the functional block diagram of the system. 

<img src="Assets/Project Functional Block Diagram.png" height="400" />
*Figure 2: Functional Block Diagram*

As illustrated in Figure 1, the system architecture is divided into three primary sections. The far-right segment comprises four full-sized breadboards, collectively housing our custom-designed GPU. This region includes the main Arduino UNO, responsible for generating all driving signals, and the array of logic IC chips that help distribute the signals.

To the left, a pair of breadboards make up the LED display section. This section houses the 16×9 LED matrix, the accelerometer, all the nMOSFETs, and the current-limiting resistors.

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
<img src="Assets/Arduino Bit-Bang Pin Mapping for Addressing the Multiplexed LED Matrix.png" height="100" />
*Table 2: Arduino Bit-Bang Pin Mapping for Addressing the Multiplexed LED Matrix. (See Table 3: Pixel Addressing) Pin 11 is used for the PWM Signal Broadcast. *


<img src="Assets/GPU Top Down Photo.jpg" height="250" />
*Figure 3: Photo of the GPU we constructed. On the rightmost 3 breadboard columns: **top left:** 4x SN7442N decimal decoder array. **top center & top right:** 6x SN7404 hex inverter (NOT gate) array. **bottom left, center, & right:** 9x SN7408 AND gate array. Wire colors: **Orange & Blue:** internal digital signal wires, alternates every 4 bit and after passing through NOT gates. **Yellow:** PWM signal broadcast wires. **Green:** Output signal wires. **Red:** 5V Vcc. **Black:** Ground.*

## LED Matrix and nMOSFET Array
We are using the Adafruit 2973 16x9 blue LED matrix to display the behavior of water under varying tilt angles. This matrix comprises 144 individual LEDs, and driving each LED with a separate GPIO pin would be impractical. Instead, we employ a common technique called multiplexing. Specifically, this board utilizes a row-column multiplexing method. 

The 16x9 LED matrix is internally structured as two independent 8x9 LED matrices, Matrix A and Matrix B. Each of these 8x9 matrices is controlled by nine dedicated pins, as illustrated in the schematic (Figure \ref{p3} or the project's GitHub page for a higher-resolution image). Each of these nine pins is connected to a pair of 2N70000 nMOSFETs. One MOSFET, when the gate receives an activation signal, connects the pin to ground. The other MOSFET, when activated, connects the pin through a 100R current-limiting resistor to the 5V power supply. It is also worth noting that the connections for each row and column are unique and should be carefully examined. 

LEDs are unidirectional devices, meaning they only emit light when current flows through them in the forward direction. Additionally, in a multiplexed matrix, even if an LED has the correct forward voltage across it, if a lower-resistance path from another LED exists for the current, the current won't go through other LEDs. This current flow characteristic dictates how we can selectively activate individual LEDs within the matrix. 

<img src="Hardware/LED Matrix 2973 Adafruit Schematics & Layout & Simulatoin/led_matrix_matrixsch.png" height="400" />
*Figure 4: 2973 Adafruit 16x9 Blue LED Matrix Schematic*

By precisely controlling which of the nine control lines are driven high (connected to approximately 5V through a current-limiting resistor) and which are driven low (connected to ground) at any given moment, we can create a specific voltage potential across only the desired LED, ensuring current flows through it and no other unintended paths are completed. Rapidly switching these control line states allows us to create the illusion of a stable display with individual pixel control using fewer pins than the total number of LEDs. 

Each submatrix of the 16×8 LED display requires nine control pins, resulting in a total of eighteen control pins for the full board. Since each control pin is connected to two nMOSFETs, the system needs to control 36 nMOSFETs to operate the entire display. While this is still far fewer than the 128 GPIO pins that would be needed to control each LED individually, 36 GPIO pins is still beyond what an Arduino UNO can provide. Additionally, to create a more lifelike fluid simulation, we aim to vary the brightness of the LEDs—requiring 36 PWM-capable outputs, which is impractical without designing a custom shield board from scratch. 

After extensive brainstorming, we developed a simple yet effective solution using only the logic gate components available in our physics lab. More information on this is explained in the earlier GPU section. 

We use 2N7000 through-hole nMOSFETs to pull the LED display control pins high or low. The schematic for a typical control pin setup is shown in Figure \ref{p4}. All MOSFETs are controlled by the logic gate array and the Arduino, based on output from the fluid physics engine. Although the Arduino’s GPIO pins output around 5V, after passing through logic gates, various wire lengths, and the breadboard, the voltage reaching the nMOSFET gates remains above 4V, still well above the gate threshold voltage and safely below the maximum voltage rating. 


<img src="Assets/nMOSFETs Controls for Each LED Matrix Pin.png" height="250" />
*Figure 5: Schematics of the Pull-Up and Pull-Down Setup for Control Pin 0 from the LED Matrix as Example*

When an nMOSFET receives a turn-on signal, it allows current to flow from drain to source. For the pull-up configuration, the drain is connected to the 5V rail, and the source is connected to a 100R current-limiting resistor, which then connects to the LED control pin. In the pull-down configuration, the source is connected to ground, and the drain is connected to the LED control pin. We intentionally swap the drain and source for the pull-down setup because during testing, we found that not doing so created unwanted current paths to ground, unintentionally activating multiple LEDs at once. 

In our row-column multiplexing method, to keep the firmware simple, we only turn on one LED per submatrix at a time. To create the illusion of a smooth and continuous pattern across the entire display, the system must run at a very high refresh rate. As a final precaution, we verified the switching speed of the 2N7000 MOSFETs. Both the turn-on and turn-off times are approximately 10ns \cite{2n7000}, corresponding to a theoretical switching frequency of up to 50MHz. However, accounting for real-world delays such as gate charging and discharging, we introduced a slight delay between operations. 

Since only one LED is active at any given moment, the 100R current-limiting resistor is selected based on the current requirements of a single blue LED operating at 5V. 

To simplify debugging and quickly determine which pins need to be pulled high or low, we created a lookup table, as shown in Table \ref{tab:LUT}. This table is then used in the firmware to control the corresponding MOSFETs for each LED.


<img src="Hardware/LED Matrix 2973 Adafruit Schematics & Layout & Simulatoin/LUT Pull-Up and Pull-Down Pin Configuration for Each LED Activation.png" height="500" />
*Table 3: Configuration values for LED matrix control. For each LED position, the table shows which pins should be set to 5V and which to GND*

## Power Supply
For this project, we used a Keysight E36311A as an external power supply. As a safety precaution, we powered the LED display and the accelerometer using one 5V channel, while a second 5V channel supplied power to the rest of the system. Using two separate channels allowed us to monitor the current draw of each section independently, which proved helpful during debugging. 

Since only one LED is active at a time, the power consumption of the first 5V channel remained very low, drawing less than 10mA in total. However, the logic gate array drew approximately 0.5A, far exceeding what the Arduino UNOs can provide. This made an external power supply essential for everything to work. 

Although the power supply may not deliver exactly 5V, small ripples and voltage offsets are still within the acceptable range to reliably power our entire system. Similarly, while the current readings may not be perfectly accurate, they are sufficient for identifying anomalies. In practice, if something is wired incorrectly, the current draw typically spikes and quickly hits the 0.6A current limit, providing a clear indication that something is wrong. 

Both 5V channels from the external power supply and the two Arduino UNOs share a common ground to ensure proper signal referencing and avoid potential floating ground issues.

<img src="Assets/Multiplexed LED Matrix and nMOSFETs Drivers Section.png" height="250" />
*Figure 6: Photo of the LED Display and Driver Section, consists of 36x 2N7000 nMOSFETs, 16x 100R Resistors and the LED display. (the accelerometer is missing in this photo). Wire colors: **Green & Blue on the right:** nMOSFETs' gate signal wires connecting to the interface board. **White:** Wires Connecting to the LED display. **Red:** 5V Power Supply. **Black:** Ground.**



