#!/usr/bin/env bash

# Reference: https://docs.muse.dev/docs/configuring-muse/

apt-get -u update
DEBIAN_FRONTEND=noninteractive apt-get -y install wget ninja-build git gettext asciidoctor zip unzip
DEBIAN_FRONTEND=noninteractive apt-get -y install libpng-dev libsdl2-dev libopenal-dev libphysfs-dev libvorbis-dev libtheora-dev libxrandr-dev qttools5-dev qtscript5-dev qt5-default libfribidi-dev libfreetype6-dev libharfbuzz-dev libfontconfig1-dev libcurl4-gnutls-dev gnutls-dev libsodium-dev

# Initialize Git submodules
git submodule update --init --recursive

mkdir build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B ./build .
mv build/compile_commands.json ./

# # MuseDev seems to currently run in-source builds
# # Truncate the DisallowInSourceBuilds.cmake file to allow this (for now)
# echo "" > ./cmake/DisallowInSourceBuilds.cmake
