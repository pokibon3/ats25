#User Manual

This skech demonstrates a way to report the current status of the receiver via Morse Code.
However, the prototype used by this sketch is not intended for blind people.
It is just an idea to inspire people to build receivers with accessibility features.

###Commands

1. BAND SELECTION

    1.1 Select the band by pressing the encoder push button once and then rotate the encoder clockwise or counterclockwise.  
    1.2 When the desired band is shown on display, you can press the button once again or wait for about 2 seconds.
        The control will then go back to the VFO.

2. STEP, MODE, AGC/Attenuation, bandwidth, Soft Mute and VOLUME

    2.1. Press the encoder push button twice (within 1/2 second).  
    2.2. After that, the display will show you the Menu text. Rotate the encoder clockwise or counterclockwise
      to select the option (STEP, MODE, AGC/Attenuation, bandwidth, VOLUME, etc).  
    2.3. After that, select the option you want to setup by pressing the encoder push button once again.  
    2.4. After that, rotate the encoder clockwise or counterclockwise to select the parameter.  
    2.5. Finally, you can press the button once again or wait for about 2 seconds.
         The control will go back to the VFO.

3. VFO/BFO Switch

    3.1. Press the encoder push button twice (within 1/2 second).  
    3.2. Rotate the encoder clockwise or counterclockwise and go to the BFO option. This option is shown only on SSB mode.  
    3.3. Press the encoder push button once again.  
    3.4. Rotate the encoder clockwise or counterclockwise to increment or decrement the BFO (select the offset).  
    3.5. If you press the button again or stop rotating the encoder for about 2 seconds, the control will go back to the VFO.

4. SEEK

    4.1. Select the menu by pressing twice the encoder push button. The seek direction is based on the last encoder
        movement. If clockwise, the seek will go up. If counterclockwise, the seek will go down.

    ATTENTION: Try to press and release the push button quickly. If you hold the button too long, the board may randomly alternate
    the command status (enable and disable).
    After about 2 seconds, the current command or action is disabled automatically and the interface goes back
    to VFO control.

###TIPS:

You can adjust the time to disable a current command or action by changing the constant ELAPSED_COMMAND
(#define ELAPSED_COMMAND 2000). The value 2000 means 2 seconds. Adjust this value according to your preference.
Try to refine the constant ELAPSED_CLICK (#define ELAPSED_CLICK 1500). This constant controls the time
for double click on encoder push button. Smaller values demand more agility in the double click.

