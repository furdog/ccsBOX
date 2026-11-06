#!/bin/bash

# Set environment variables
if [ -z "${GIT_REPO_VERSION+x}" ]; then
	export GIT_REPO_VERSION=$(git describe --tags)
fi

# 2. Get the branch (e.g., test/name)
raw_branch=$(git rev-parse --abbrev-ref HEAD)

# 3. Replace / with - and export (e.g., test-name)
export GIT_REPO_BRANCH=${raw_branch//\//-}

# 4. Combine them into your final name
export FULL_VERSION="${GIT_REPO_VERSION}-${GIT_REPO_BRANCH}"

echo $FULL_VERSION

###############################################################################
# CONFIGURATION:
###############################################################################
COMPILER=arduino-cli

SERIAL_PORT=COM113
MONITOR_BAUD=115200
export OTA_IP="7.7.7.7"

export PROJECT="ccsBOX"
export HARDWARE=esp32c6

#EXTRA_FLAGS="-v"

###############################################################################
# TARGETS AVAILABLE
###############################################################################
export DEVICE_ID=$PROJECT"_"$HARDWARE"_"$FULL_VERSION

if [ "$HARDWARE" == "esp32c6" ]; then
	BOARD=esp32:esp32:esp32c6
	FQBN=:CDCOnBoot=cdc
	echo "#define HARDWARE $HARDWARE" > target.gen.h
else
	echo "Bad target!"
	exit 1
fi

echo "#define DEVICE_ID \"$DEVICE_ID\"" >> target.gen.h

###############################################################################
# MAIN
###############################################################################
mk_base64_updater()
{
	local BOARD_PATH="${BOARD//:/\.}"  # Replace ':' with '.'
	local FILE="build/${BOARD_PATH}/$(basename "$PWD").ino.bin"

	# Generate BASE64 firmware
	export BASE64_ENCODED_FIRMWARE=$(base64 -w 0 "$FILE")

	# Generate updater html file (with embedded base64 firmware)
	../awk/ENV.awk ../web/updater.html > \
		     build/"$BOARD_PATH"/"$DEVICE_ID".html
}

mk_web_interface()
{
	#export LANG=$(cat web/$1.json) # Select language based on choice

	echo "Building web..."

	# Replace environment variables inside web page
	awk/ENV.awk web/index.html > build/index.html

	# Compress WEB
	gzip -9 -c build/index.html > build/index.html.gz
}

compile() {
	# enumerate (and empty) all generated files
	mkdir -p build
	mkdir -p build/src

	rm build/* 2> /dev/null # Clean build
	rm build/src/* 2> /dev/null # Clean src

	echo "Initial setup..."

	# Setup tools and libraries
	./setup.sh
	if [[ $? -ne 0 ]]; then
	    echo "FATAL ERROR: Setup failed."
	    exit 1
	fi


	echo "Testing..."

	#cd tests/
	#./make.sh
	#if [[ $? -ne 0 ]]; then
	#    echo "FATAL ERROR: Test failed."
	#    exit 1
	#fi
	#cd ..

	echo "Copying..."

	# Copy all necessary files into build/
	cp *.ino build/build.ino
	cp -r src/*.h build/src/
	cp ./*.h build/

	mk_web_interface #${TARGET_LANG}

	echo "Compiling..."

	# Goto build directory with all generated files
	cd build

	# Make C array from WEB
	echo "#pragma once" >> index.h
	xxd -i index.html.gz >> index.h

	echo "PROPS: " ${PROPS}
	echo "FQBN: " ${FQBN}
	while ! ${COMPILER} compile -b ${BOARD}${FQBN} --warnings "all" \
			   ${PROPS} -e --libraries "../libraries/" \
			   ${EXTRA_FLAGS}; do
		read -p "Press any key to continue "
		exit
	done

	mk_base64_updater
}

# Function to monitor
monitor() {
	if [ -n "${SERIAL_PORT+x}" ]; then
		while true; do
			${COMPILER} monitor -p ${SERIAL_PORT} \
			      --config baudrate=${MONITOR_BAUD} ${EXTRA_FLAGS};
			sleep 1
		done
	fi
}

upload() {
	if [ -n "${SERIAL_PORT+x}" ]; then
		while ! ${COMPILER} upload -b ${BOARD}${FQBN} \
				   -p ${SERIAL_PORT} ${EXTRA_FLAGS}; do
			sleep 1
		done
	fi
}

web_upload() {
	local BOARD_PATH="${BOARD//:/\.}"  # Replace ':' with '.'
	local FILE="build/${BOARD_PATH}/$(basename "$PWD").ino.bin"

	if [ ! -f "$FILE" ]; then
		echo "Firmware file not found: $FILE"
		return 1
	fi

	while true; do
		echo "Uploading $FILE to http://${OTA_IP}/update..."
		curl -F "file=@$FILE" http://${OTA_IP}/update && break
		echo "Upload failed, retrying in 1 second..."
		sleep 1
	done

	echo "Upload complete."
}

# Check if the argument is "monitor"
if [ "$1" == "monitor" ]; then
	monitor
elif [ "$1" == "upload" ]; then
	cd build
	upload
elif [ "$1" == "flash" ]; then
	cd build
	upload
elif [[ "$1" == "web" && ( "$2" == "upload" || "$2" == "flash" ) ]]; then
	cd build
	web_upload
elif [ "$1" == "web" ]; then
	compile
	web_upload
else
	compile
	upload
	monitor
fi

read -p "Press any key to continue "
