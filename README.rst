====================================
Ahah: Arduino Home Automation Helper
====================================

What is it
==========

**Ahah** (I need a better name:) is home automation sofware to run on an
Arduino Mega 2560 to automatically control your home automation needs.  The
project was forked from OpenSprinkler and is growing from there.  The reason
for the fork is I already had an Arduino Mega 2560 and some relays bought for
doing home automation and I didn't want to buy specialized hardware.  The Mega
2560 has 8x more memory and about 8x more IO than OpenSprinkler and the parts
are widely available.  The cost of a Mega is quite cheap as Arduino is open
source hardware and clones are welcome (although they are not suppose to use
the "Arduino" name as it is trademarked).

This project is still young and looking for contributors.  The code does not quite
compile yet but it is getting close.

Recommended Hardware
====================

If you find better hardware please let me know.  

 - Mega 2560 R2 (older version) from `dx.com <https://dx.com/p/arduino-mega2560-atmega2560-16au-usb-board-118047?item=4>`__
 - W5100 R2? (older version, $10) from Ebay.  Search: arduino w5100.  For R3 ($15) search for: arduino w5100 r3

For the price difference I think the older revisions of the 2560 and W5100 are a better deal.  I can't say for sure but it seems the hardware updates were quite minimal.

  - 8 channel, 5V relay extension board (`$12 <https://dx.com/p/8-channel-5v-relay-module-board-for-arduino-red-156424?item=5>`__ to `$16 <https://dx.com/p/8-channel-5v-relay-module-extension-board-for-arduino-avr-arm-51-140703?item=1>`__).
  - Jumper cables (male to female) 40 pieces ($3).  Ebay search: dupont wire color jumper cable male female 2.54mm
  - For standoffs the regular 3mm threads are too large.  I'm drilling into a metal case and hoping to use `2.5mm screws <http://www.ebay.com/itm/180677742101?ssPageName=STRK:MEWNX:IT&_trksid=p3984.m1439.l2649>`__ and a stack `washers <http://www.ebay.com/itm/320975892026?ssPageName=STRK:MEWNX:IT&_trksid=p3984.m1439.l2649>`__ to get right stand-off height.


Contributing
=============

  - Follow the GitHub guides on how to download the software.  
  - Then install arduino (apt-get install arduino)
  - Then add simlinks to your arduino install to point to where you installed Ahah:

    cd /usr/share/arduino/examples

    sudo ln -s '/<path to>/opensprinkler/OpenSprinkler Controller/software/libraries/OpenSprinkler'

    cd /usr/share/arduino/libraries

    sudo ln-s '/<path to>/opensprinkler/OpenSprinkler Controller/software/libraries/OpenSprinkler'

   - Start **arduino** and specify your hardware (Mega 2560) from **Tools>>Board** menu.
   - Open the source code with **File>>Examples>>OpenSprinkler>>interval_program**
   
Now you can help.  Maybe take the GitHub tour to figure out the rest.

License
=======

Creative Commons Attribution-ShareAlike 3.0 license

Credits
=======

This project started as fork of RAYSHOBBYY's `OpenSprinkler <http://opensprinkler.com>`__.  Many thanks
to him for his excellent work.

