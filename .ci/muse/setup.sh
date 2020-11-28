#!/usr/bin/env bash

# Reference: https://docs.muse.dev/docs/configuring-muse/

apt-get -u update
DEBIAN_FRONTEND=noninteractive apt-get -y install wget ninja-build git gettext asciidoctor zip unzip
DEBIAN_FRONTEND=noninteractive apt-get -y install libpng-dev libsdl2-dev libopenal-dev libphysfs-dev libvorbis-dev libtheora-dev libxrandr-dev qtscript5-dev qt5-default libfribidi-dev libfreetype6-dev libharfbuzz-dev libfontconfig1-dev libcurl4-gnutls-dev gnutls-dev libsodium-dev

echo "Installing Vulkan SDK..."
echo "wget lunarg signing-key"
wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc| apt-key add -
echo "wget sources.list.d"
wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.2.148-focal.list https://packages.lunarg.com/vulkan/1.2.148/lunarg-vulkan-1.2.148-focal.list
echo "apt-get -u update"
apt-get -u update
echo "apt-get -y install vulkan-sdk"
DEBIAN_FRONTEND=noninteractive apt-get -y install vulkan-sdk
