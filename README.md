rtd_fileplayer
==============

07102014

Description
-----------
The purpose of this program is to "play back" data that has already been acquired and memmap it for either prtd ("Process Real-Time Display") or cprtd ("Complex"), which in turn make FFT(W)s available to rtdgui.



Playing back TCP/IP data from Wallops
-------------------------------------
Setting the command-line option "-t" will enable a routine in rtd_fileplayer that looks for TCP/IP packet headers and removes them from data being read before the data are sent to the real-time display file. 
It seems that not all packets come through entirely, as rtd_fileplayer still doesn't seem to find all TCP headers despite the data being combed twice within the program. (It should only be combed once, but
I'm not a sufficiently slick programmer to guarantee all headers are found on the first pass.) Also, it appears that sometimes our internal Dartmouth headers are sometimes overwritten or not transmitted by TCP/IP,
which can occasionally create odd features in processed spectrograms--generally these are just broad vertical lines across the FFT.
*Note that endianness can be an issue in properly locating TCP/IP headers! If rtd_fileplayer complains, swap the endianness.

Playing back digitized data with "-g"
------------------------------------
If you want to inspect a digitized file, it's important to note that the rate at which the digitized data was originally acquired is important. If, for example, the data were acquired at a 20kHz sample rate, 
you might consider setting the RTD outputsize to 4096 and the acquisition size to 8192. 

Endianness issues
-----------------
If the file you are inspecting was acquired through the Commtech-Fastcom FSCC-LVDS serial card, then you will want to be sure to swap the endianness using '-E'. If you otherwise get complaints from the program 
about not being able to find Dartmouth headers or TCP/IP packet headers, toggle the endianness.