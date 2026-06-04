// VideojuegoEntornosLib.cpp : common socket helpers for client and server.

#include "pch.h"
#include "framework.h"
#include "VideojuegoEntornosLib.h"

#include <cctype>
#include <sstream>

namespace {
// Buffer de lectura para JSON, dejar margen al texto escapado

constexpr int JSON_BUFFER_SIZE = MSG_SIZE * 2;

void assignMsg(PDataPacket packet, const std::string& msg)
{
    // Copia segura al array fijo del DataPacket: deja siempre sitio para '\0'
    memset(packet->msg, 0, MSG_SIZE);
    const size_t copySize = (msg.size() < MSG_SIZE - 1) ? msg.size() : MSG_SIZE - 1;
    memcpy(packet->msg, msg.c_str(), copySize);
}

std::string escapeJson(const char* text)
{
    // Convierte caracteres especiales para que el campo msg sea JSON valido
    std::string out;
    for (const unsigned char c : std::string(text)) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (c < 0x20) out += ' ';
            else out += static_cast<char>(c);
            break;
        }
    }
    return out;
}

std::string packetToJson(const DataPacket& packet)
{
    std::ostringstream oss;
    oss << "{\"client_id\":" << packet.client_id
        << ",\"sequence\":" << packet.sequence
        << ",\"msg\":\"" << escapeJson(packet.msg) << "\"}";
    return oss.str();
}

bool findValueStart(const std::string& json, const std::string& key, size_t& pos)
{
    // Parser simple hecho para nuestro formato fijo: client_id, sequence y msg.
    const std::string quotedKey = "\"" + key + "\"";
    pos = json.find(quotedKey);
    if (pos == std::string::npos) return false;

    pos = json.find(':', pos + quotedKey.size());
    if (pos == std::string::npos) return false;
    ++pos;

    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    return pos < json.size();
}

bool readJsonInt(const std::string& json, const std::string& key, int& value)
{
    // Lee enteros del JSON sin depender de librerias externas.
    size_t pos = 0;
    if (!findValueStart(json, key, pos)) return false;

    bool negative = false;
    if (json[pos] == '-') {
        negative = true;
        ++pos;
    }

    if (pos >= json.size() || !std::isdigit(static_cast<unsigned char>(json[pos]))) return false;

    long long parsed = 0;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
        parsed = (parsed * 10) + (json[pos] - '0');
        ++pos;
    }

    value = static_cast<int>(negative ? -parsed : parsed);
    return true;
}

bool readJsonString(const std::string& json, const std::string& key, std::string& value)
{
    // Lee strings escapados; soporta los escapes que genera escapeJson.
    size_t pos = 0;
    if (!findValueStart(json, key, pos)) return false;
    if (json[pos] != '"') return false;
    ++pos;

    value.clear();
    while (pos < json.size()) {
        const char c = json[pos++];
        if (c == '"') return true;

        if (c != '\\') {
            value.push_back(c);
            continue;
        }

        if (pos >= json.size()) return false;
        const char escaped = json[pos++];
        switch (escaped) {
        case '"': value.push_back('"'); break;
        case '\\': value.push_back('\\'); break;
        case '/': value.push_back('/'); break;
        case 'b': value.push_back('\b'); break;
        case 'f': value.push_back('\f'); break;
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        case 'u':
            if (pos + 4 > json.size()) return false;
            value.push_back('?');
            pos += 4;
            break;
        default:
            return false;
        }
    }

    return false;
}

bool packetFromJson(const std::string& json, PDataPacket packet)
{
    // Reconstruye el DataPacket que el resto del proyecto usa internamente.
    int clientId = 0;
    int sequence = 0;
    std::string msg;

    if (!readJsonInt(json, "client_id", clientId)) return false;
    if (!readJsonInt(json, "sequence", sequence)) return false;
    if (!readJsonString(json, "msg", msg)) return false;

    packet->client_id = clientId;
    packet->sequence = sequence;
    assignMsg(packet, msg);
    return true;
}
}

std::ostream& operator << (std::ostream& os, const DataPacket& dp) {
    return (os << "DataPacket{client: " << dp.client_id << " seq: " << dp.sequence << " msg: " << dp.msg << " }");
}

void treatError(const std::string msg, SOCKET s) {
    // Version menos brusca que la de clase: informa y cierra el socket si existe.
    std::cout << msg << WSAGetLastError() << std::endl;
    if (s != INVALID_SOCKET) closesocket(s);
}

void treatErrorExit(const std::string msg, SOCKET s, int error) {
    std::cout << msg << WSAGetLastError() << std::endl;
    if (s != INVALID_SOCKET) closesocket(s);
    WSACleanup();
    ExitProcess(error);
}

int udpCommonSocketSetup(SOCKET& s, PCSTR address, u_short port, sockaddr_in* addr, std::string prefix) {
    // Factoriza la creacion de socket UDP igual que en la libreria de clase.
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        treatErrorExit(prefix + ": socket creation error: ", s, -1);
    }

    memset(addr, 0, sizeof(sockaddr_in));
    if (inet_pton(AF_INET, address, &(addr->sin_addr.s_addr)) != 1) {
        treatErrorExit(prefix + ": error converting IP in string to binary: ", s, -1);
    }
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    std::cout << prefix << ": UDP socket created" << std::endl;
    return 0;
}

int udpServerSocketSetup(SOCKET& s_server, PCSTR address, u_short port, sockaddr_in* my_addr, std::string prefix) {

    udpCommonSocketSetup(s_server, address, port, my_addr, prefix);

    const int result = bind(s_server, (sockaddr*)my_addr, sizeof(sockaddr_in));
    if (result == SOCKET_ERROR) {
        treatErrorExit(prefix + ": bind error: ", s_server, -1);
    }

    getAssignedPort(s_server, my_addr);
    std::cout << prefix << ": UDP socket bound to " << address
        << ":" << ntohs(my_addr->sin_port) << std::endl;
    return 0;
}

int sendtoMsg(SOCKET& s, sockaddr_in* dest_addr, PDataPacket packet, std::string prefix) {
    const std::string jsonPayload = packetToJson(*packet);
    const int result = sendto(
        s,
        jsonPayload.c_str(),
        static_cast<int>(jsonPayload.size()),
        0,
        (SOCKADDR*)dest_addr,
        sizeof(sockaddr_in));

    if (result == SOCKET_ERROR) {
        std::cout << prefix << " sendto error: " << WSAGetLastError() << std::endl;
        return SOCKET_ERROR;
    }

    std::cout << prefix << " sent JSON: " << jsonPayload << std::endl;
    return result;
}

int recvfromMsg(SOCKET& s, sockaddr_in* sender_addr, PDataPacket response, std::string prefix) {
    char buffer[JSON_BUFFER_SIZE + 1];
    int fromlen = sizeof(sockaddr_in);
    const int result = recvfrom(s, buffer, JSON_BUFFER_SIZE, 0, (SOCKADDR*)sender_addr, &fromlen);

    if (result == SOCKET_ERROR) {
        std::cout << prefix << " recvfrom error: " << WSAGetLastError() << std::endl;
        return SOCKET_ERROR;
    }

    buffer[result] = '\0';
    const std::string jsonPayload(buffer, result);
    if (!packetFromJson(jsonPayload, response)) {
        response->client_id = 0;
        response->sequence = 0;
        assignMsg(response, "ERROR|BAD_JSON");
        std::cout << prefix << " received invalid JSON: " << jsonPayload << std::endl;
    }
    else {
        std::cout << prefix << " received JSON: " << jsonPayload << std::endl;
    }

    return result;
}

int sendtorecvfromMsg(SOCKET& s, sockaddr_in* dest_addr, PDataPacket packet, PDataPacket response, std::string prefix) {
    const int sent = sendtoMsg(s, dest_addr, packet, prefix);
    if (sent == SOCKET_ERROR) return SOCKET_ERROR;
    return recvfromMsg(s, dest_addr, response, prefix);
}

int recvfromsendtoMsg(SOCKET& s, PDataPacket response, std::string prefix) {
    sockaddr_in sender_addr;
    const int received = recvfromMsg(s, &sender_addr, response, prefix);
    if (received == SOCKET_ERROR) return SOCKET_ERROR;
    return sendtoMsg(s, &sender_addr, response, prefix);
}

int obtainNewPort(SOCKET& s, sockaddr_in* server_addr, std::string prefix) {
    DataPacket request(0, 0, "CONNECT");
    DataPacket response;

    const int result = sendtorecvfromMsg(s, server_addr, &request, &response, prefix);
    if (result == SOCKET_ERROR) return SOCKET_ERROR;

    const std::string msg(response.msg);
    if (msg.rfind("OK|PORT|", 0) != 0) {
        std::cout << prefix << " unexpected handshake response: " << msg << std::endl;
        return SOCKET_ERROR;
    }

    return 0;
}

int tcpCommonSocketSetup(SOCKET& s, PCSTR address, u_short port, sockaddr_in* addr, std::string prefix) {
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        treatErrorExit(prefix + ": socket creation error: ", s, -1);
    }

    memset(addr, 0, sizeof(sockaddr_in));
    if (inet_pton(AF_INET, address, &(addr->sin_addr.s_addr)) != 1) {
        treatErrorExit(prefix + ": error converting IP in string to binary: ", s, -1);
    }
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    std::cout << prefix << ": TCP socket created" << std::endl;
    return 0;
}

int tcpServerSocketSetup(SOCKET& s_server, PCSTR address, u_short port, sockaddr_in* my_addr, std::string prefix) {
    tcpCommonSocketSetup(s_server, address, port, my_addr, prefix);

    const int result = bind(s_server, (sockaddr*)my_addr, sizeof(sockaddr_in));
    if (result == SOCKET_ERROR) {
        treatErrorExit(prefix + ": bind error: ", s_server, -1);
    }

    if (listen(s_server, 1) == SOCKET_ERROR) {
        treatErrorExit(prefix + ": listen error: ", s_server, -1);
    }
    return 0;
}

int sendMsg(SOCKET& acceptSocket, PDataPacket packet, std::string prefix) {
    const std::string jsonPayload = packetToJson(*packet);
    const int result = send(acceptSocket, jsonPayload.c_str(), static_cast<int>(jsonPayload.size()), 0);

    if (result == SOCKET_ERROR) {
        std::cout << prefix << " send error: " << WSAGetLastError() << std::endl;
        return SOCKET_ERROR;
    }

    std::cout << prefix << " sent JSON: " << jsonPayload << std::endl;
    return result;
}

int recvMsg(SOCKET& acceptSocket, PDataPacket response, std::string prefix) {
    char buffer[JSON_BUFFER_SIZE + 1];
    const int result = recv(acceptSocket, buffer, JSON_BUFFER_SIZE, 0);

    if (result == SOCKET_ERROR) {
        std::cout << prefix << " recv error: " << WSAGetLastError() << std::endl;
        return SOCKET_ERROR;
    }

    buffer[result] = '\0';
    const std::string jsonPayload(buffer, result);
    if (!packetFromJson(jsonPayload, response)) {
        response->client_id = 0;
        response->sequence = 0;
        assignMsg(response, "ERROR|BAD_JSON");
    }

    std::cout << prefix << " received JSON: " << jsonPayload << std::endl;
    return result;
}

int sendrecvMsg(SOCKET& s, PDataPacket packet, PDataPacket response, std::string prefix) {
    const int sent = sendMsg(s, packet, prefix);
    if (sent == SOCKET_ERROR) return SOCKET_ERROR;
    return recvMsg(s, response, prefix);
}

int recvsendMsg(SOCKET& s, PDataPacket response, std::string prefix) {
    const int received = recvMsg(s, response, prefix);
    if (received == SOCKET_ERROR) return SOCKET_ERROR;
    return sendMsg(s, response, prefix);
}

int getAssignedPort(SOCKET& s, sockaddr_in* my_addr) {
    int namelen = sizeof(sockaddr_in);
    if (getsockname(s, (SOCKADDR*)my_addr, &namelen) != 0) {
        treatErrorExit("Socket getsockname error: ", s, -1);
    }

    const int assigned_port = ntohs(my_addr->sin_port);
    std::cout << "Socket bound to port: " << assigned_port << std::endl;
    return assigned_port;
}
