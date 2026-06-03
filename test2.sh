#!/bin/bash
while IFS= read -r line; do
time   echo "$line" | ggwave-cli
date
done < answer1.md
