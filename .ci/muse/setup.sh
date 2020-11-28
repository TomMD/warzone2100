#!/usr/bin/env bash

# Reference: https://docs.muse.dev/docs/configuring-muse/

apt-get -u update
DEBIAN_FRONTEND=noninteractive apt-get -y install wget ninja-build git gettext asciidoctor zip unzip
DEBIAN_FRONTEND=noninteractive apt-get -y install libpng-dev libsdl2-dev libopenal-dev libphysfs-dev libvorbis-dev libtheora-dev libxrandr-dev qtscript5-dev qt5-default libfribidi-dev libfreetype6-dev libharfbuzz-dev libfontconfig1-dev libcurl4-gnutls-dev gnutls-dev libsodium-dev

# Initialize Git submodules
git submodule update --init --recursive
