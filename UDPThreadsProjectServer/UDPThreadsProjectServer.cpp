#pragma comment(lib, "ws2_32.lib") //add WinSock2 library

#include <stdio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <assert.h>
#include "UDPThreadsProjectLib.h"
#include <iostream>
#include <math.h>
#include <format>
#include <thread>

#define MAX_THREADS 5

std::thread serverFun(PDataPacket clientPacket, SOCKET s, sockaddr_in* client_addr, int i, std::string prefix);
int threadFun(ThreadInfo* thInfo);


int main()
{
    std::string prefix = "Server: ";
    std::cout << "Server: starting..." << std::endl;
    //required intialization of WinSock 2 library, it writes some data om wsaData to check everything is ok
    int result;
    WSAData wsaData;
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    assert(result == NO_ERROR);

    std::cout << "Server: WinSock started correctly" << std::endl;

    //now we create a socket that uses IP (AF_INET) with UDP (SOCK_DGRAM and IPPROTO_UDP)
    SOCKET s;
    sockaddr_in my_addr;
    udpServerSocketSetup(s, "127.0.0.1", 4000, &my_addr, prefix);

    PDataPacket packet = new DataPacket();
    std::thread hThreadArray[MAX_THREADS]; //handlers of threads
    int i = 0;
    while (i < MAX_THREADS) {
        std::cout << "Server ready to recv" << std::endl;
        //recv msg and call serverFun
        sockaddr_in client_addr;
        recvfromMsg(s, &client_addr, packet, prefix);

        //do something
        hThreadArray[i] = serverFun(packet, s, &client_addr, i, prefix);
        i++;
    }
    // Wait until all threads have terminated.
    //equivalent to joining all threads, even slightly innefficient
    for (int i = 0; i < MAX_THREADS; i++) {
        hThreadArray[i].join();
    }
    std::cout << "Server: cleaning up and returning" << std::endl;
    // cleanup
    closesocket(s);
    WSACleanup();
}

//makes operation with op1 and op2 storing the result in res, all of them fields of clientPacket
std::thread serverFun(PDataPacket clientPacket, SOCKET s, sockaddr_in* client_addr, int i, std::string prefix) {
    int result = -1;
    //now we create a socket that uses IP (AF_INET) with UDP (SOCK_DGRAM, IPPROTO_UDP) 
    SOCKET s_new;
    sockaddr_in my_addr;
    udpServerSocketSetup(s_new, "127.0.0.1", 0, &my_addr, prefix);

    //sendtoMsg(...new_port...) through the new one, because getsockname apparently only works with connection oriented sockets!
    // remember that ALL UDP DATAGRAMS contain implicitly the network address of the sender, and this address is available when
    // doing recvfrom
    sendtoMsg(s_new, client_addr, clientPacket, prefix);
    //create object that serves as param for the thread function
    PThreadInfo thInfo = new ThreadInfo(i, s_new, prefix);
    std::thread hThread = std::thread(threadFun, thInfo);
    return hThread;
}

//function of dedicated thread in the server for a specific client
int threadFun(ThreadInfo* thInfo) {
    bool serve = true;
    PDataPacket packet = new DataPacket();
    while (serve) {
        std::cout << "Server Thread ready to recv" << std::endl;
        //TODO:invoke recvfromMsg

        //TODO:do something
        //do ECO of the received msg

    }
    //TODO:cleanup of thread
    return 0;
}



