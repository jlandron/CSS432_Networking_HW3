#!/bin/bash

g++ Timer.cpp UdpSocket.cpp udphw3.cpp -o hw3
g++ Timer.cpp UdpSocket.cpp udphw3case4.cpp -o hw3case4

echo "Standard tests compiled to hw3"
echo "Test case 4 compiled to hw3cas4"