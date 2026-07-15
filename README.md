# Flipper-Zero-Firmware-on-Cardputer-ADV // mostly-a-FLIPPER

<img src="https://mostlyawesome.de/dev/p2.jpg" alt="Another one" width="40%" height="40%"> <img src="https://mostlyawesome.de/dev/p1.jpg" alt="Picture of mostly-A-FLIPPER" width="40%" height="40%"> 

This is a port of the Flipper Zero firmware for the Cardputer &amp; Cardputer ADV. It's a fork of Sor3nt's Flipper-Zero-ESP32-Port.

I put my focus on rf emission and rf signal intelligence in general, and thus I'm going to update and enhance the firmware's functionality in future updates.




<h2>Installation</h2>
I made the firmware compatible with  <a href="https://github.com/bmorcelli/Launcher">bmorcelli's launcher</a>. 
You can install the firmware with launcher, or you can install it with the web flasher.

<h2>SD.zip - download and path:</h2> You need to download this file <a href="https://github.com/pinchepasta/Flipper-Zero-Firmware-on-Cardputer-ADV/blob/main/sdcard.zip">sd.zip</a> and unzip it to the root of your sd card.

The firmware is <i>only fully functional with this sd files</i>.


<h2>IR-Payload-Collection.zip:</h2> Download the zip file and use it in your sd root, to have a solid starting point for ir pentesting.
Download it <a href="https://github.com/pinchepasta/Flipper-Zero-Firmware-on-Cardputer-ADV/blob/main/IR-Payload-Collection.zip">right here</a>




<h2>Supported devices</h2>

M5Stack Cardputer & Cardputer ADV

<i>*I'm also working on a Lilygo T-Embed CC1101 Plus version I will integrate here</i>



<h2>Bugfixes</h2>

<b>1.19 Fixes:</b> Passport let's you change your username now. Waterfall in sub-ghz section now works well enough. 


-


<b>1.18 Fixes:</b> RF-Shortcuts got a uniform remote ui and bugs fixed that made the app crash. Now I'm working to fix the BadBLE feature

-

<b>1.17 Fixes:</b> I fixed the "out of PSRAM" bug, and the soft reset after leaving wifi section is also fixed, the device now doesn't need to reboot at any point.

-

<b>1.16 Fixes:</b>  RF related issues with the Cardputer ADV and the Pingequa NRF24/CC1101 module, like not being able to transmit .sub files are fixed, also now the rf jammer, and all bruteforce modes are fully functional.


<h2>What I'm working on:</h2>

- espConnect integration to make file sharing with bruce devices possible (95% done)
- Improved rolling code handling, more protocols. (90% done)
- Pwnagotchi integration of some kind. (50% done)
- IR-Shortcuts & system wide shortcuts with a push of a button. (100%)
- BLESP v3 integration and remote control of the flipper. (100)
- IMSI Catcher detector (50% done)
Stay tuned!




