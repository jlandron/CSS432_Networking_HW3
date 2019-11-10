#!/bin/bash

g++ -O3 Timer.cpp UdpSocket.cpp udphw3.cpp -o hw3
g++ -O3 Timer.cpp UdpSocket.cpp udphw3case4.cpp -o hw3case4

echo "Standard tests compiled to hw3"
echo "Test case 3 compiled to hw3cas4"