#Twitch3DS

Twitch.tv client for 3ds!

#Requirements

* DevkitArm
* Latest ctrulib
* Latest citro3d
* ffmpeg compiled with the following instructions

#Building 

##FFMPEG

* Copy the ffmpeg-configure3ds script in your ffmpeg source folder
* Open a shell/command line in ffmpeg directory
    - Windows users please use `sh` before starting the script
* ./ffmpeg-configure3ds
* make install

This will compile ffmpeg (with only a few features) with devkitArm and install it as a portlib

##Twitch3DS

###With the Makefile

Simply use `make`.

#Thanks

* Lectem for his original repo this is based on: [3Damnesic] (https://github.com/Lectem/3Damnesic)
* #3ds-dev community for all the help!

