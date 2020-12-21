#!/bin/bash
set -ex # exit with nonzero exit code if anything fails

# Build times benefit from parallel building
export MAKE_PARALLEL="make -j3"

# Make sure Sming picks up this version of the library
export COMPONENT_SEARCH_DIRS=$TRAVIS_BUILD_DIR/..
touch .submodule

#
ln -s Sming-NVS ../NVS

# Pull in Sming to a subdirectory of this Component
SMING_ROOT=$TRAVIS_BUILD_DIR/Sming
git clone https://github.com/mikee47/Sming --branch develop --single-branch --depth 1 $SMING_ROOT
export SMING_HOME=$SMING_ROOT/Sming

# Full compile checks please
export STRICT=1

env

if [ "$SMING_ARCH" == "Esp8266" ]; then
	export ESP_HOME=$TRAVIS_BUILD_DIR/opt/esp-alt-sdk
	mkdir -p $ESP_HOME

	if [ "$TRAVIS_OS_NAME" == "linux" ]; then
	  wget --no-verbose https://github.com/nodemcu/nodemcu-firmware/raw/2d958750b56fc60297f564b4ec303e47928b5927/tools/esp-open-sdk.tar.xz
	  tar -Jxf esp-open-sdk.tar.xz; ln -s $(pwd)/esp-open-sdk/xtensa-lx106-elf $ESP_HOME/.
	fi

	export PATH=$PATH:$ESP_HOME/xtensa-lx106-elf/bin:$ESP_HOME/utils/

	cd test
	$MAKE_PARALLEL

else
	# Build and run our tests
	cd test
	$MAKE_PARALLEL execute SMING_ARCH=Host
fi

