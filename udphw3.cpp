#include <vector>
#include "hw3.cpp"

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
                cerr << "Ack = " << sequence << endl;
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
            (nextInSequence < max)) {  // message is in window
            message[0] = nextInSequence;
            sock.sendTo((char *)message, MSGSIZE);  // udp message send
            cerr << "message = " << message[0] << endl;
            //!!!!testing out of order packets comment out!!!
            // message[0] = (nextInSequence * nextInSequence) % max;
            // sock.sendTo((char *)message, MSGSIZE);  // udp message send
            // cerr << "message = " << message[0] << endl;
            //!!!END TESTING!!!
            // check if packet was already sent once
            if (sent[nextInSequence]) {
                retransmitted++;
            }
            // mark as sent
            sent[nextInSequence] = true;
            // increment next in sequence
            nextInSequence++;
        } else {
            timer.start();

            // spin wait for either ack or timeout
            while ((timer.lap() < TIMEOUT * 10) && (sock.pollRecvFrom() <= 0))
                ;

            // check if spinwait ended because an ack was received
            if (sock.pollRecvFrom() > 0) {
                int ackRecv;
                sock.recvFrom((char *)&ackRecv, sizeof(int));
                // check if you received an ack that is in the window
                if (ackRecv > windowBase) {  // if you get an ack from the
                                             // "future", move window up
                    windowBase = ackRecv;
                } else if (ackRecv ==
                           windowBase) {  // if ack is for windowBase, slide
                                          // window up one
                    windowBase++;
                }
            }
            // spinwait ended due to timeout set expected packet to first in
            // window
            else {
                // set the next in sequence to the window base for resending
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
 *message[], it marks the message's sequence number as received in an array and
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
    cerr << "server early retransmit test:" << endl;
    vector<bool> sent(max, false);
    // buffer for out of order packets
    int acksSent = 0;
    int lastInOrderSequenceNum = 0;
    while (lastInOrderSequenceNum < max) {
        sock.recvFrom((char *)message, MSGSIZE);  // udp message receive
        int seqNum = message[0];
        sent[seqNum] = true;  // marked acked
        // cerr << "message = " << seqNum << endl;
        int ackNum = lastInOrderSequenceNum;
        // send ack
        if (ackNum < max) {
            sock.ackTo((char *)&ackNum, sizeof(int));
            // cerr << "Acked " << ackNum << endl;
            acksSent++;
        }
        // check sequence order
        if (seqNum == lastInOrderSequenceNum) {  // server received the next
                                                 // packet in order
            lastInOrderSequenceNum++;
            while (lastInOrderSequenceNum < max &&
                   sent[lastInOrderSequenceNum]) {
                // cerr << "Fastforward past: " << lastInOrderSequenceNum <<
                // endl;
                lastInOrderSequenceNum++;

            }  // fast forward past
            // received messages
        } else {
            // handle out of order packets with buffer
        }

        // ack any packet, even out of order
    }
    cout << "Server sent " << acksSent << " Acks" << endl;
}