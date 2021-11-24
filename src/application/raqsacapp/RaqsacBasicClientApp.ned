
package raqsac.application.raqsacapp;

import inet.applications.contract.IApp;

simple RaqsacBasicClientApp like IApp
{
    parameters:
        @class("inet::RaqsacBasicClientApp");
        string localAddress = default(""); // may be left empty ("")
        int localPort = default(-1); // port number to listen on
        string connectAddress = default("");  // server address (may be symbolic)
        int connectPort = default(1000); // port number to connect to
        double startTime @unit(s) = default(1s); // time first session begins
        double stopTime @unit(s) = default(-1s);  // time of finishing sending, negative values mean forever

		int priorityValue = default(0);
        volatile int sendBytes @unit(B) = default(1MiB);
        int numSymbolsToSend;  // if this is a sender

        volatile double reconnectInterval @unit(s) = default(30s);  // if connection breaks, waits this much before trying to reconnect

        @lifecycleSupport;
        double stopOperationExtraTime @unit(s) = default(-1s);    // extra time after lifecycle stop operation finished
        double stopOperationTimeout @unit(s) = default(2s);    // timeout value for lifecycle stop operation

        @display("i=msg/mail;i2=status/down");
    gates:
        input socketIn @labels(RaqsacCommand/up);
        output socketOut @labels(RaqsacCommand/down);
}

