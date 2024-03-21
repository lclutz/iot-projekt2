#!/bin/sh

set -e
CALLING_FROM="$PWD"
cd "$(dirname "$0")"

echo "Removing packages from non-official sources..."
sudo apt-get -qq remove docker.io docker-doc docker-compose podman-docker containerd runc

echo "Installing prerequisits..."
sudo apt-get -qq update
sudo apt-get -qq install ca-certificates curl

echo "Install signing key..."
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/debian/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc

echo "Add official repo to sources..."
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/debian \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get -qq update

echo "Installing docker..."
sudo apt-get -qq install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

echo "Add current user to docker group..."
sudo usermod -aG docker $USER
sudo newgrp docker

echo "Done."
cd "$CALLING_FROM"
