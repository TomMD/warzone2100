#!/usr/bin/env bash

# Reference: https://docs.muse.dev/docs/configuring-muse/

apt update
apt install -y wget ninja-build git gettext asciidoctor zip unzip
apt install -y libpng-dev libsdl2-dev libopenal-dev libphysfs-dev libvorbis-dev libtheora-dev libxrandr-dev qtscript5-dev qt5-default libfribidi-dev libfreetype6-dev libharfbuzz-dev libfontconfig1-dev libcurl4-gnutls-dev gnutls-dev libsodium-dev

wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | apt-key add -
wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.2.148-focal.list https://packages.lunarg.com/vulkan/1.2.148/lunarg-vulkan-1.2.148-focal.list
apt update
apt install vulkan-sdk
