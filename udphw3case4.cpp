#include <cstdlib>  //srand, rand
#include <ctime>    //TIME
#include <vector>
#include "hw3case4.cpp"

#define TIMEOUT 1500  // specified timeout interval
/**
 * serverReliable
 * repeats receiving message[] and sending an acknowledgment at a server side
 * max (=20,000) times using the sock object.
 *
 * @param {UdpSocket} &sock : reference to socket
 * @param {const int} max   : max number of messages
 * @param {int} message[]   : message buffer
 *
 * */
void serverReliable(UdpSocket &sock, const int max, int message[]) {
    cerr << "server reliable test:" << endl;
    int acksSent = 0;
    // receive message[] max times
    for (int sequence = 0; sequence < max; sequence++) {
        do {
            sock.recvFrom((char *)message, MSGSIZE);  // udp message receive
            if (message[0] == sequence) {
                sock.ackTo((char *)&sequence, sizeof(int));
                acksSent++;
            }

        } while (message[0] != sequence);

        // cerr << message[0] << endl;  // print out message
    }
    cout << "Server sent " << acksSent << " Acks" << endl;
}
/**
 *
 * sends message[] and receives an acknowledgment from the server max (=20,000)
 * times using the sock object. If the client cannot receive an acknowledgment
 * immediately, it should start a Timer. If a timeout occurs (i.e., no response
 * after 1500 usec), the client must resend the same message. The function must
 * count the number of messages retransmitted and return it to the main function
 * as its return value.
 *
 * @param {UdpSocket} &sock     : reference to socket
 * @param {const int} max       : max number of messages
 * @param {int} message[]       : message buffer
 * @return {int} retransmitted      : number of packets resent by the client
 **/
int clientStopWait(UdpSocket &sock, const int max, int message[]) {
    cerr << "client: stop and wait test:" << endl;
    Timer timer;

    int retransmitted = 0;
    // int numGOTOs = 0;
    // transfer message[] max times
    for (int sequence = 0; sequence < max; sequence++) {
        message[0] = sequence;                  // message[0] has a sequence #
        sock.sendTo((char *)message, MSGSIZE);  // udp message send

        // start timout counter
        timer.start();
    POLL:
        // wait for a packet to be receivable
        while (sock.pollRecvFrom() <= 0) {
            if (timer.lap() >= TIMEOUT) {               // resend after timeout
                sock.sendTo((char *)message, MSGSIZE);  // resend udp message
                timer.start();                          // restart timer
                retransmitted++;
            }
        }
        int ack;
        sock.recvFrom((char *)&ack, sizeof(int));
        // client checks to see if this ack is
        // for the correct packet before moving on to the next packet
        if (ack != sequence) {
            // numGOTOs++;
            goto POLL;
        }

        // cerr << "message = " << message[0] << endl;
    }
    // cout << "Used GOTO " << numGOTOs << " times" << endl;
    return retransmitted;
}
/**
 * clientSlidingWindow
 *
 *sends message[] and receiving an acknowledgment from a server max (=20,000)
 *times using the sock object. As described above, the client can continuously
 *send a new message[] and increasing the sequence number as long as the number
 *of in-transit messages (i.e., # of unacknowledged messages) is less than
 *"windowSize." That number should be decremented every time the client receives
 *an acknowledgment. If the number of unacknowledged messages reaches
 *"windowSize," the client should start a Timer. If a timeout occurs (i.e., no
 *response after 1500 usec), it must resend the message with the minimum
 *sequence number among those which have not yet been acknowledged. The function
 *must count the number of messages (not bytes) re-transmitted and return it to
 *the main function as its return value.
 *
 * @param {UdpSocket} &sock     : reference to socket
 * @param {const int} max       : max number of messages
 * @param {int} message[]       : message buffer
 * @param {int} windowSize      : size of sliding window on client side
 * @return {int} retransmitted      : number of packets resent by the client
 **/
int clientSlidingWindow(UdpSocket &sock, const int max, int message[],
                        int windowSize) {
    cerr << "client: sliding window test:" << endl;
    Timer timer;
    // vector<bool> to mark packets that have been sent
    vector<bool> sent(max, false);
    int retransmitted = 0;
    int nextInSequence = 0;
    int windowBase = 0;

    // loop until all packets have been sent and all acks received
    while (nextInSequence < max || windowBase < max) {
        if ((nextInSequence < (windowBase + windowSize)) &&
            (nextInSequence < max)) {
            message[0] = nextInSequence;
            sock.sendTo((char *)message, MSGSIZE);  // udp message send
            // check if packet was already sent once
            if (sent[nextInSequence]) {
                retransmitted++;
            }
            // mark as sent
            sent[nextInSequence] = true;
            nextInSequence++;
        } else {
            timer.start();
            bool windowMoved = false;

            // spin wait for either ack or timeout
            while ((timer.lap() < TIMEOUT) && (sock.pollRecvFrom() <= 0))
                ;

            // check if spinwait ended because an ack was received
            if (sock.pollRecvFrom() > 0) {
                int ack;
                sock.recvFrom((char *)&ack, sizeof(int));
                // cerr << "received Ack " << ack << endl;
                if (ack > windowBase) {
                    windowBase = ack;
                    windowMoved = true;
                }
            }
            // spinwait ended due to timeout set expected packet to first in
            // window
            else {
                if (!windowMoved) {
                    // cerr << "Timout windowBase: " << windowBase << endl;
                    nextInSequence = windowBase;
                }
            }

            cerr << "message = " << message[0] << endl;
        }
    }
    return retransmitted;
}

/**
 * serverEarlyRetrans
 *
 *receives message[] and sends an acknowledgment to the client max (=20,000)
 *times using the sock object. Every time the server receives a new
 *message[], it must save the message's sequence number in an array and
 *return a cumulative acknowledgment, i.e., the last received message in
 *order.
 *
 * @param {UdpSocket} &sock : reference to socket
 * @param {const int} max   : max number of messages
 * @param {int} message[]   : message buffer
 * @param {int} windowSize  : size of sliding window on server side
 **/
void serverEarlyRetrans(UdpSocket &sock, const int max, int message[],
                        int windowSize) {
    srand(time(nullptr));
    // random number between 0 and 10 for packet loss percentage between 0-10%
    int lossInterval = rand() % 11;
    bool droppedOnce = false;
    cerr << "server early retransmit test:" << endl;
    vector<bool> sent(max, false);
    int acksSent = 0;
    int nextExpectedSequenceNum = 0;
    while (nextExpectedSequenceNum < max) {
        sock.recvFrom((char *)message, MSGSIZE);  // udp message receive
        int seqNum = message[0];
        // handle packet if it is not "dropped"
        if ((rand() % 100) > lossInterval) {
            sent[seqNum] = true;
            if (seqNum == nextExpectedSequenceNum) {
                // increase expected count until you hit an unsent packet
                while (nextExpectedSequenceNum < max &&
                       sent[nextExpectedSequenceNum]) {
                    nextExpectedSequenceNum++;
                }
            }
            int ackNum = nextExpectedSequenceNum;
            if (ackNum <= max) {
                sock.ackTo((char *)&ackNum, sizeof(int));
                acksSent++;
            }
        }
        cerr << "message = " << message[0] << endl;
    }
    cout << "Packet drop percentage: " << lossInterval << endl;
    cout << "Server sent " << acksSent << " Acks" << endl;
}