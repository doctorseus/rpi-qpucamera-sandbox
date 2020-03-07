# rpi-qpucamera-sandbox

Note: This repo is work-in-progress. I set it to public because I have no time to go further with this experiment but someone might find something useful here.

What is this? This project aims to run custom shader programs on the Raspberry Pis GPU (specifically the QPU) in order to do arbitrary video processing directly on frames taken by the Raspberry Pi camera.
Most Raspberry Pis, like the Zero, would not be able to process these high resolution images in realtime on their weaker CPU.

Currently the only user shader implemented can do a 720p 60fps threshold effect with very little cpu time needed. Example: https://www.youtube.com/watch?v=1tzmiDmDwtw

Some low level details can be found here: https://www.raspberrypi.org/forums/viewtopic.php?f=43&t=167652
