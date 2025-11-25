#pragma once
#include <time.h>
#include <Windows.h>
#include <iostream>
#include <ostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <assert.h>

#define MSG_SIZE 256

typedef class DataPacket {
public:
    int client_id;
    int sequence;
    char msg[MSG_SIZE];
    DataPacket() {
        memset(msg, 0, MSG_SIZE); //clear all the memory in packet.msg before copying a new msg
    };
    DataPacket(int _client_id, int _sequence, std::string _msg) {
        client_id = _client_id;
        sequence = _sequence;
        //deep copy of string into the struct so that data reaches the server, NEVER send a pointer / shallow copy
        memset(msg, 0, MSG_SIZE); //clear all the memory in packet.msg before copying a new msg
        _msg.copy(msg, _msg.size(), 0);
    }
} *PDataPacket;

typedef class ThreadInfo {
public:
    int thread_id;
    SOCKET s;
    std::string prefix;
    ThreadInfo() {};
    ThreadInfo(int _thread_id, SOCKET _s, std::string _prefix) {
        thread_id = _thread_id;
        s = _s;
        prefix = _prefix.c_str();
    }
    ~ThreadInfo() {
        closesocket(s);
    }
} *PThreadInfo;

std::ostream& operator << (std::ostream& os, const DataPacket& dp);
// The reason this is in a separate file is because I want to use this
// on the server and the client

void treatError(const std::string msg, SOCKET s);

void treatErrorExit(const std::string msg, SOCKET s, int error);


//UDP calls

int udpCommonSocketSetup(SOCKET& s, PCSTR address, u_short port, sockaddr_in* addr, std::string prefix);

int udpServerSocketSetup(SOCKET& s, PCSTR address, u_short port, sockaddr_in* addr, std::string prefix);

int sendtoMsg(SOCKET& s, sockaddr_in* dest_addr, PDataPacket packet, std::string prefix);

int recvfromMsg(SOCKET& s, sockaddr_in* sender_addr, PDataPacket response, std::string prefix);

int sendtorecvfromMsg(SOCKET& s, sockaddr_in* dest_addr, PDataPacket packet, PDataPacket response, std::string prefix);

int recvfromsendtoMsg(SOCKET& s, PDataPacket response, std::string prefix);

//TCP calls


int tcpCommonSocketSetup(SOCKET& s, PCSTR address, u_short port, sockaddr_in* addr, std::string prefix);

int tcpServerSocketSetup(SOCKET& s, PCSTR address, u_short port, sockaddr_in* addr, std::string prefix);

int sendMsg(SOCKET& s, PDataPacket packet, std::string prefix);

int recvMsg(SOCKET& s, PDataPacket response, std::string prefix);

int sendrecvMsg(SOCKET& s, PDataPacket packet, PDataPacket response, std::string prefix);

int recvsendMsg(SOCKET& s, PDataPacket response, std::string prefix);

int getAssignedPort(SOCKET& s, sockaddr_in* my_addr);
