#pragma comment(lib, "ws2_32.lib")

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "VideojuegoEntornosLib.h"

static void printUsageDefaults(int id, int rows, int cols, int mines)
{
    // Permite ejecutar desde Visual Studio sin argumentos de linea de comandos.
    std::cout << "No se ingresaron argumentos. Usando valores por defecto.\n"
        << "Uso: .\\VideojuegoEntornos <id> <rows> <cols> <mines>\n"
        << "Por defecto: id=" << id
        << " rows=" << rows
        << " cols=" << cols
        << " mines=" << mines << std::endl;
}

static std::string trim(const std::string& text)
{
    // Limpia espacios iniciales/finales antes de interpretar comandos.
    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c);
        });
    if (first == text.end()) return "";

    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) {
        return std::isspace(c);
        }).base();
    return std::string(first, last);
}

static std::string upper(std::string text)
{
    // Normaliza comandos para aceptar "state", "State" o "STATE".
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
        });
    return text;
}

static std::string normalizeCommand(const std::string& rawLine)
{
    // Comfort layer only
    const std::string line = trim(rawLine);
    if (line.find('|') != std::string::npos) return line;

    std::istringstream iss(line);
    std::vector<std::string> parts;
    std::string part;
    while (iss >> part) parts.push_back(part);
    if (parts.empty()) return line;

    for (std::string& value : parts) value = upper(value);

    if ((parts[0] == "REVEAL" || parts[0] == "FLAG" || parts[0] == "SHOW") && parts.size() == 3) {
        return parts[0] + "|" + parts[1] + "|" + parts[2];
    }
    if (parts[0] == "CHEST" && parts.size() == 3) {
        return parts[0] + "|" + parts[1] + "|" + parts[2];
    }
    if (parts[0] == "SHOP" && parts.size() >= 2) {
        std::string out = parts[0];
        for (size_t i = 1; i < parts.size(); ++i) out += "|" + parts[i];
        return out;
    }
    if (parts[0] == "USE" && parts.size() == 4) {
        return parts[0] + "|" + parts[1] + "|" + parts[2] + "|" + parts[3];
    }

    return upper(line);
}

int main(int argc, char* argv[])
{
    std::string prefix = "Client";

    // Configuracion por defecto de partida para que la demo sea inmediata.
    int clientId = 1;
    int rows = 8;
    int cols = 8;
    int mines = 10;

    if (argc >= 5) {
        clientId = atoi(argv[1]);
        rows = atoi(argv[2]);
        cols = atoi(argv[3]);
        mines = atoi(argv[4]);
        std::cout << "Argumentos recibidos: id=" << clientId
            << " rows=" << rows
            << " cols=" << cols
            << " mines=" << mines << std::endl;
    }
    else {
        printUsageDefaults(clientId, rows, cols, mines);
    }

    // Validación suave: corrige entradas imposibles sin cerrar el programa
    if (rows <= 0) { rows = 8;  std::cout << "[Aviso] rows invalido, fijado a 8.\n"; }
    if (cols <= 0) { cols = 8;  std::cout << "[Aviso] cols invalido, fijado a 8.\n"; }
    if (mines < 0) { mines = 10; std::cout << "[Aviso] mines invalido, fijado a 10.\n"; }
    if (mines >= rows * cols) {
        int computed = (rows * cols) / 4;
        mines = (computed < 1) ? 1 : computed;
        std::cout << "[Aviso] Demasiadas minas para " << rows << "x" << cols
            << ", fijado a " << mines << ".\n";
    }

    std::cout << "Client: starting..." << std::endl;

    // Inicializacion de WinSock
    WSAData wsaData;
    const int startup = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (startup != NO_ERROR) {
        std::cout << "[Error] No se pudo iniciar WinSock: " << startup << "\n";
        return 1;
    }
    std::cout << "Client: WinSock started correctly" << std::endl;

    // Primer destino: lobby del servidor. Despues del handshake cambiara al
    // puerto dedicado que atiende solo a este cliente.
    SOCKET s;
    sockaddr_in server_addr;
    udpCommonSocketSetup(s, "127.0.0.1", 4000, &server_addr, prefix);

    if (obtainNewPort(s, &server_addr, prefix) == SOCKET_ERROR) {
        std::cout << "[Error] No se pudo obtener puerto dedicado del servidor.\n";
        closesocket(s);
        WSACleanup();
        return 1;
    }
    std::cout << "Client obtained dedicated port: " << ntohs(server_addr.sin_port) << std::endl;

    {
        // El servidor crea el tablero; el cliente solo pide la inicializacion.
        const std::string initCmd = "INIT|" + std::to_string(rows) + "|" + std::to_string(cols) + "|" + std::to_string(mines);
        DataPacket initPkt(clientId, 0, initCmd);
        DataPacket initResp;
        const int rr = sendtorecvfromMsg(s, &server_addr, &initPkt, &initResp, "Client:");
        if (rr == SOCKET_ERROR) {
            std::cout << "[Error] No se pudo enviar/recibir INIT.\n";
            closesocket(s);
            WSACleanup();
            return 1;
        }
        std::cout << initResp.msg << "\n";
    }

    int seq = 1;
    while (true) {
        std::cout << "\nComandos: \n"
            "  REVEAL|x|y\n"
            "  FLAG|x|y\n"
            "  SHOW|x|y\n"
            "  STATE\n"
            "  PASS\n"
            "  SHOP|STATE\n"
            "  SHOP|BUY|LIFE\n"
            "  SHOP|BUY|SCAN\n"
            "  USE|SCAN|x|y\n"
            "  CHEST|SPAWN|k\n"
            "  EXIT\n> ";

        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) continue;

        // El comando normalizado se envia como RPC textual dentro del JSON.
        const std::string command = normalizeCommand(line);
        DataPacket req(clientId, seq++, command);
        DataPacket resp;

        const int rr = sendtorecvfromMsg(s, &server_addr, &req, &resp, "Client:");
        if (rr == SOCKET_ERROR) {
            std::cout << "[Error] Comunicacion UDP en el comando.\n";
            continue;
        }

        std::cout << resp.msg << "\n";

        // BYE y GAMEOVER terminan la sesion desde el punto de vista del cliente.
        const std::string response(resp.msg);
        if (response.rfind("BYE", 0) == 0) break;
        if (response.rfind("GAMEOVER|", 0) == 0) break;
    }

    const int closeResult = closesocket(s);
    if (closeResult == SOCKET_ERROR) {
        std::cout << "closesocket failed with error: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    WSACleanup();
    return 0;
}
