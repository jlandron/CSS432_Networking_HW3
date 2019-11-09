#include "hw3.cpp"

int clientStopWait(UdpSocket &sock, const int max, int message[]) {}
int clientSlidingWindow(UdpSocket &sock, const int max, int message[],
                        int windowSize) {}
void serverReliable(UdpSocket &sock, const int max, int message[]) {}
void serverEarlyRetrans(UdpSocket &sock, const int max, int message[],
                        int windowSize) {}