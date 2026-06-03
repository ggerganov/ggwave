#!/bin/bash
# This wraps any long lines at 100 characters before sending
fold -s -w 100 answer1.md | while IFS= read -r line; do
  echo "$line" 
done
