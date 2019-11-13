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
            goto POLL;
        }

        // cerr << "message = " << message[0] << endl;
    }
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
    vector<int> numAcks(max, 0);
    int retransmitted = 0;
    int nextInSequence = 0;
    int windowBase = 0;

    // loop until all packets have been sent and all acks received
    while (nextInSequence < max || windowBase < max) {
        if ((nextInSequence < (windowBase + windowSize)) &&
            (nextInSequence < max)) {
            message[0] = nextInSequence;
            sock.sendTo((char *)message, MSGSIZE);  // udp message send
            // cerr << "Sent = " << nextInSequence << endl;
            // check if packet was already sent once
            if (sent[nextInSequence]) {
                retransmitted++;
            }
            // mark as sent
            sent[nextInSequence] = true;
            nextInSequence++;
        } else {
            timer.start();

            // spin wait for either ack or timeout
            while ((timer.lap() < TIMEOUT) && (sock.pollRecvFrom() <= 0))
                ;

            // check if spinwait ended because an ack was received
            if (sock.pollRecvFrom() > 0) {
                int ack;
                sock.recvFrom((char *)&ack, sizeof(int));
                // check if you recieved that is in the window
                // cerr << "Received = " << ack << endl;
                if (ack > windowBase) {
                    windowBase = ack;
                }

            }
            // spinwait ended due to timeout set expected packet to first in
            // window
            else {
                nextInSequence = windowBase;
            }
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
    srand(time(0));
    // random number between 0 and 10 for packet loss percentage between 0-10%
    int lossInterval = rand() % 11;
    cerr << "server early retransmit test:" << endl;
    vector<bool> received(max, false);
    int acksSent = 0;
    int nextExpectedSequenceNum = 0;
    bool startedReceiving = false;
    while (nextExpectedSequenceNum < max) {
        int seqNum;
        Timer timer;
        /// expect to receive windowSize packets, set timer to keep server from
        // hanging
        for (int i = 0; i < windowSize; i++) {
            timer.start();
            while ((timer.lap() < TIMEOUT) && (sock.pollRecvFrom() <= 0))
                ;

            if (sock.pollRecvFrom() > 0) {
                startedReceiving = true;
                sock.recvFrom((char *)message,
                              MSGSIZE);  // udp message receive
                seqNum = message[0];
                if ((rand() % 100) > lossInterval) {
                    received[seqNum] = true;  // marked received
                    // cerr << "Received " << seqNum << endl;
                } else {
                    continue;
                }
            } else {
                break;
            }
        }

        // increase expected count until you hit an unsent packet
        // this means that the server will ack packets either in order or
        // after a retransmission from the client
        while (nextExpectedSequenceNum < max &&
               received[nextExpectedSequenceNum]) {
            nextExpectedSequenceNum++;
        }

        // ack any packet, even out of order
        if (nextExpectedSequenceNum <= max && startedReceiving) {
            sock.ackTo((char *)&nextExpectedSequenceNum, sizeof(int));
            // cerr << "Cumulative ack: " << nextExpectedSequenceNum << endl;
            acksSent++;
        }
        // cerr << "message = " << message[0] << endl;
    }
    cout << "Loss Interval: " << lossInterval << endl;
    cout << "Server sent " << acksSent << " Acks" << endl;
}