#!/bin/sh
clear 

sudo apt-get install libboost-dev
sudo apt-get -y install cmake
echo "Running CMAKE to build and install..."
cmake .
echo "Running MAKE to build..."
make
echo "build completed run the program as echo ./groundlabs ./moby.txt 20"
./groundlabs ./moby.txt 20
echo "end!!!"
exit 0
