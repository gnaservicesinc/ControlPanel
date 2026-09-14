#!/bin/bash
cd "$(/usr/bin/dirname "$0")" || exit 1
./build.sh
result=$?
if [ "$result" -eq 0 ]; then /usr/bin/open dist; fi
echo "Press Return to close."
read -r
exit "$result"
