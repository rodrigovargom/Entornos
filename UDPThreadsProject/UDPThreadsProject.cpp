#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
//#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <assert.h>
#include "UDPThreadsProjectLib.h"
#include <iostream>
#include <string>


#define MAX_MSGS 5
//using namespace std;

int obtainNewPort(SOCKET& s, sockaddr_in* server_addr, std::string prefix);

int main(int argc, char* argv[])
{

    std::string prefix = "Client";
    if (argc < 2) {
        std::cout << "UDPClient usage: .\\UDPClient <id>" << std::endl;
        ExitProcess(-1);
    }
    int client = atoi(argv[1]);
    std::cout << "Client: starting..." << std::endl;
    //required intialization of WinSock 2 library, it writes some data om wsaData to check everything is ok

    int result;
    WSAData wsaData;
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    assert(result == NO_ERROR);
    std::cout << "Client: WinSock started correctly" << std::endl;

    //now we create a socket that uses IP (AF_INET) with UDP (SOCK_DGRAM and IPPROTO_UDP) 
    SOCKET s;
    sockaddr_in server_addr;
    udpCommonSocketSetup(s, "127.0.0.1", 4000, &server_addr, prefix);

    obtainNewPort(s, &server_addr, prefix);
    std::cout << "Client already obtained new port: " << ntohs(server_addr.sin_port) << std::endl;

    //TODO:change accordingly
    std::string msgs[MAX_MSGS] = { "hola", "quetal","quetal","quetal", "adios" };
    PDataPacket packet;
    for (int i = 0; i < MAX_MSGS; i++) {
        //TODO:create packet and allocate for response
        //TODO:send and receive response
    }

    std::cout << "Client finishing..." << std::endl;
    int iResult = closesocket(s);
    if (iResult == SOCKET_ERROR) {
        wprintf(L"closesocket failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    WSACleanup();
    return 0;
}

//sends first msg to server and returns with the new server_addr used for the server for the dedicated socket
int obtainNewPort(SOCKET& s, sockaddr_in* server_addr, std::string prefix) {
    PDataPacket packet = new DataPacket(); //I don't initialize because the server won't care
    std::cout << "Client ready to send: " << *packet << std::endl;
    PDataPacket response = new DataPacket();
    //IMPORTANT: will overwrite server_addr with the server addr with the new port, since the response msg in the server is sent through the new socket!
    sendtorecvfromMsg(s, server_addr, packet, response, prefix);
    return 0;
}

