#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <assert.h>
#include <cstring>
#include <iostream>
#include <ostream>
#include <string>
#include <time.h>

// Se amplio porque ahora las respuestas incluyen
// tablero completo, HUD y eventos, con 256 bytes se podian cortar mensajes.
#define MSG_SIZE 8192

// Ahora se serializa a JSON antes de enviarlo.
typedef class DataPacket {
public:
    int client_id = 0;
    int sequence = 0;
    char msg[MSG_SIZE];
    DataPacket() {
        memset(msg, 0, MSG_SIZE);
    };
    DataPacket(int _client_id, int _sequence, std::string _msg) {
        client_id = _client_id;
        sequence = _sequence;
        //deep copy of string into the struct so that data reaches the server, NEVER send a pointer / shallow copy
        memset(msg, 0, MSG_SIZE);//clear all the memory in packet.msg before copying a new msg

        const size_t copySize = (_msg.size() < MSG_SIZE - 1) ? _msg.size() : MSG_SIZE - 1;
        memcpy(msg, _msg.c_str(), copySize);
    }
} *PDataPacket;

// Clase auxiliar conservada del planteamiento original de hilos. El servidor
// actual usa std::thread directamente y cierra cada socket dedicado al terminar.
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

// Handshake nuevo: el cliente empieza en el lobby 127.0.0.1:4000 y esta
// funcion actualiza server_addr con el puerto UDP dedicado que responde.
int obtainNewPort(SOCKET& s, sockaddr_in* server_addr, std::string prefix);

//TCP calls


int tcpCommonSocketSetup(SOCKET& s, PCSTR address, u_short port, sockaddr_in* addr, std::string prefix);

int tcpServerSocketSetup(SOCKET& s, PCSTR address, u_short port, sockaddr_in* addr, std::string prefix);

int sendMsg(SOCKET& s, PDataPacket packet, std::string prefix);

int recvMsg(SOCKET& s, PDataPacket response, std::string prefix);

int sendrecvMsg(SOCKET& s, PDataPacket packet, PDataPacket response, std::string prefix);

int recvsendMsg(SOCKET& s, PDataPacket response, std::string prefix);

int getAssignedPort(SOCKET& s, sockaddr_in* my_addr);
