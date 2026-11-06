#!/bin/bash

#SETUP libraries
if [[ ! -d "libraries/" ]]; then
	mkdir -p libraries
	cd libraries

	cd ..
fi

#SETUP Clone git libraries
mkdir -p libraries
cd libraries

echo "Cloning git libraries..."

REPO_URL="https://github.com/bblanchon/ArduinoJson.git"
DEST_DIR="$(basename "$REPO_URL" .git)"
echo "$REPO_URL"
if [ -d "$DEST_DIR" ]; then
	(cd "$DEST_DIR" && git pull)
else
	(git clone "$REPO_URL")
fi

REPO_URL="https://github.com/adafruit/Adafruit_NeoPixel.git"
DEST_DIR="$(basename "$REPO_URL" .git)"
echo "$REPO_URL"
if [ -d "$DEST_DIR" ]; then
	(cd "$DEST_DIR" && git pull)
else
	(git clone "$REPO_URL")
fi

REPO_URL="https://github.com/ESP32Async/AsyncTCP.git"
DEST_DIR="$(basename "$REPO_URL" .git)"
echo "$REPO_URL"
if [ -d "$DEST_DIR" ]; then
	(cd "$DEST_DIR" && git pull)
else
	(git clone "$REPO_URL")
fi

REPO_URL="https://github.com/ESP32Async/ESPAsyncWebServer.git"
DEST_DIR="$(basename "$REPO_URL" .git)"
echo "$REPO_URL"
if [ -d "$DEST_DIR" ]; then
	(cd "$DEST_DIR" && git pull)
else
	(git clone "$REPO_URL")
fi

cd ..
