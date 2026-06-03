#!/bin/bash
# Path to your file and the ggwave-cli binary
FILE="/data/data/com.termux/files/home/Downloads/answer1.md"
GGWAVE="./build/bin/ggwave-cli"

echo "Starting transmission of $FILE..."
echo "Protocol: Fast (Using full 140 byte buffer)"

# 140 is the hard limit for 'Fast' protocol in the source code.
# 5 seconds sleep ensures the receiver has finished 'Analyzing' 
# and is back in 'Receiving' mode before we chirp again.
(
  cat "$FILE" | base64 | tr -d '\n' | fold -w 140 | while read chunk; do
    echo "Sending chunk..." >&2
    echo "$chunk"
    sleep 5
  done
) | $GGWAVE
