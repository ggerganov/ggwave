#!/bin/bash


# This wraps any long lines at 100 characters before sending
#fold -s -w 100 

cat answer1.md | while IFS= read -r line; do
# -p2 makes it fastest, but too many errors then
# apt install mpv is needed: 
  time echo "$line" | ggwave-to-file  -f/data/data/com.termux/files/usr/tmp/ggwave_temp.wav && mpv /data/data/com.termux/files/usr/tmp/ggwave_temp.wav
echo "$line" | lolcat
#ggwave-to-file -f/data/data/com.termux/files/usr/tmp/ggwave_temp.wav && play /data/data/com.termux/files/usr/tmp/ggwave_temp.wav
echo

date 
echo
# sleep 1

done
