#pragma comment(lib, "ws2_32.lib")

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "VideojuegoEntornosLib.h"

class MinesweeperGame {
public:
    // Toda la autoridad de la partida vive en esta clase del servidor. El
    // cliente nunca sabe donde estan minas, puerta, cofres ni recompensas.
    static constexpr int LIFE_PRICE = 5;
    static constexpr int SCAN_PRICE = 3;

    // Configuracion del tablero y matriz principal: -1 significa mina, 0..8
    // significa numero de minas vecinas.
    int rows = 0;
    int cols = 0;
    int mines = 0;
    std::vector<std::vector<int>> board;
    std::vector<std::vector<bool>> revealed;
    std::vector<std::vector<bool>> flagged;
    std::vector<std::vector<bool>> chest;

    // Economia y progreso anadidos sobre el Buscaminas basico de la memoria.
    int lives = 1;
    int coins = 0;
    int scans = 0;
    int turns = 0;
    int streak = 0;

    // Flags de estado para validar comandos y saber si la partida acabo.
    bool initialized = false;
    bool gameOver = false;
    bool win = false;
    bool doorFound = false;
    int doorX = -1;
    int doorY = -1;

    MinesweeperGame()
        : rng(static_cast<unsigned int>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
            ^ std::random_device{}())) {
        // Cada sesion tiene semilla propia para que dos clientes no compartan tablero.
    }

    bool inBounds(int x, int y) const
    {
        return x >= 0 && x < rows && y >= 0 && y < cols;
    }

    bool init(int r, int c, int m)
    {
        // INIT reinicia toda la partida y valida tamano/dificultad.
        if (r < 2 || c < 2 || r > 30 || c > 30) return false;
        if (m < 0 || m >= r * c) return false;

        rows = r;
        cols = c;
        mines = m;

        board.assign(rows, std::vector<int>(cols, 0));
        revealed.assign(rows, std::vector<bool>(cols, false));
        flagged.assign(rows, std::vector<bool>(cols, false));
        chest.assign(rows, std::vector<bool>(cols, false));

        placeMines();
        placeDoor();

        lives = 1;
        coins = 0;
        scans = 0;
        turns = 0;
        streak = 0;
        gameOver = false;
        win = false;
        doorFound = false;
        initialized = true;
        return true;
    }

    int spawnChests(int k)
    {
        // CHEST|SPAWN|k coloca cofres ocultos solo en celdas seguras y no reveladas.
        if (!initialized || k <= 0) return 0;

        std::vector<std::pair<int, int>> candidates;
        for (int x = 0; x < rows; ++x) {
            for (int y = 0; y < cols; ++y) {
                if (board[x][y] == -1) continue;
                if (revealed[x][y]) continue;
                if (chest[x][y]) continue;
                if (x == doorX && y == doorY) continue;
                candidates.push_back({ x, y });
            }
        }

        std::shuffle(candidates.begin(), candidates.end(), rng);
        const int total = (std::min)(k, static_cast<int>(candidates.size()));
        for (int i = 0; i < total; ++i) {
            chest[candidates[i].first][candidates[i].second] = true;
        }
        return total;
    }

    std::string reveal(int x, int y, std::string& events, bool& safeProgress, bool& actionTaken)
    {
        // REVEAL es la accion central: valida, revela, gestiona mina, cofre,
        // puerta y cascada de ceros. Devuelve texto RPC, no imprime nada.
        safeProgress = false;
        actionTaken = false;

        if (!initialized) return "ERROR|NO_INIT";
        if (gameOver) return "ERROR|GAME_OVER";
        if (!inBounds(x, y)) return "ERROR|OUT_OF_RANGE";
        if (flagged[x][y]) return "ERROR|FLAGGED";
        if (revealed[x][y]) return "OK";

        actionTaken = true;
        revealed[x][y] = true;

        if (board[x][y] == -1) {
            if (lives > 1) {
                --lives;
                appendEvent(events, "EVENT: MINE -1 life");
                return "OK";
            }

            gameOver = true;
            win = false;
            revealAllMines();
            return "GAMEOVER|LOSE";
        }

        safeProgress = true;
        handleDoorReveal(x, y, events);

        if (chest[x][y]) {
            const int reward = randomInt(1, 3);
            coins += reward;
            chest[x][y] = false;
            appendEvent(events, "EVENT: CHEST +" + std::to_string(reward) + " coins");
        }

        if (board[x][y] == 0) {
            floodZeros(x, y, events);
        }

        return "OK";
    }

    bool toggleFlag(int x, int y)
    {
        // FLAG alterna marca de sospecha, pero nunca sobre una celda revelada.
        if (!initialized || gameOver) return false;
        if (!inBounds(x, y)) return false;
        if (revealed[x][y]) return false;
        flagged[x][y] = !flagged[x][y];
        return true;
    }

    char cellChar(int x, int y) const
    {
        // Caracter visible que se usa en STATE y SHOW.
        if (!initialized || !inBounds(x, y)) return '?';
        if (flagged[x][y]) return 'F';
        if (!revealed[x][y]) return '#';
        if (x == doorX && y == doorY) return 'D';
        if (board[x][y] == -1) return '*';
        return static_cast<char>('0' + board[x][y]);
    }

    bool checkWin() const
    {
        // En esta version hay que encontrar la puerta y revelar todas las seguras.
        if (!initialized || !doorFound) return false;

        for (int x = 0; x < rows; ++x) {
            for (int y = 0; y < cols; ++y) {
                if (board[x][y] != -1 && !revealed[x][y]) return false;
            }
        }
        return true;
    }

    int applyStreakReward(int sx, int sy, int target, std::string& events)
    {
        // Recompensa de racha: tras 5 aciertos, revela hasta target celdas
        // seguras cercanas sin dar monedas por cofres.
        if (!initialized || target <= 0) return 0;

        std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
        std::queue<std::pair<int, int>> q;
        q.push({ sx, sy });
        visited[sx][sy] = true;

        const int dirs[8][2] = {
            {-1, -1}, {-1, 0}, {-1, 1},
            {0, -1},           {0, 1},
            {1, -1},  {1, 0},  {1, 1}
        };

        int count = 0;
        while (!q.empty() && count < target) {
            const auto [x, y] = q.front();
            q.pop();

            for (const auto& d : dirs) {
                const int nx = x + d[0];
                const int ny = y + d[1];
                if (!inBounds(nx, ny) || visited[nx][ny]) continue;
                visited[nx][ny] = true;

                if (board[nx][ny] != -1 && !revealed[nx][ny] && !flagged[nx][ny]) {
                    revealed[nx][ny] = true;
                    chest[nx][ny] = false;
                    handleDoorReveal(nx, ny, events);
                    ++count;
                    if (count >= target) break;
                }

                q.push({ nx, ny });
            }
        }

        return count;
    }

    std::string scanCell(int x, int y)
    {
        // Objeto comprado en tienda: informa el tipo de celda sin revelarla.
        if (!initialized) return "ERROR|NO_INIT";
        if (gameOver) return "ERROR|GAME_OVER";
        if (!inBounds(x, y)) return "ERROR|OUT_OF_RANGE";
        if (scans <= 0) return "ERROR|NO_SCAN_ITEM";

        --scans;
        ++turns;

        std::string kind = "SAFE";
        if (board[x][y] == -1) kind = "MINE";
        else if (x == doorX && y == doorY) kind = "DOOR";
        else if (chest[x][y]) kind = "CHEST";

        return "OK|SCAN|" + std::to_string(x) + "|" + std::to_string(y)
            + "|" + kind + "|visible=" + cellChar(x, y);
    }

private:
    std::mt19937 rng;

    int randomInt(int minValue, int maxValue)
    {
        std::uniform_int_distribution<int> dist(minValue, maxValue);
        return dist(rng);
    }

    static void appendEvent(std::string& events, const std::string& event)
    {
        if (!events.empty()) events += "\n";
        events += event;
    }

    void placeMines()
    {
        // Coloca minas y actualiza los numeros de las 8 celdas vecinas.
        int placed = 0;
        while (placed < mines) {
            const int x = randomInt(0, rows - 1);
            const int y = randomInt(0, cols - 1);
            if (board[x][y] == -1) continue;

            board[x][y] = -1;
            ++placed;

            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (inBounds(nx, ny) && board[nx][ny] != -1) {
                        ++board[nx][ny];
                    }
                }
            }
        }
    }

    void placeDoor()
    {
        // La puerta siempre se coloca en una celda segura.
        std::vector<std::pair<int, int>> safeCells;
        for (int x = 0; x < rows; ++x) {
            for (int y = 0; y < cols; ++y) {
                if (board[x][y] != -1) safeCells.push_back({ x, y });
            }
        }

        const int index = randomInt(0, static_cast<int>(safeCells.size()) - 1);
        doorX = safeCells[index].first;
        doorY = safeCells[index].second;
    }

    void revealAllMines()
    {
        // Al perder, se ensenan las minas para que el tablero final tenga sentido.
        for (int x = 0; x < rows; ++x) {
            for (int y = 0; y < cols; ++y) {
                if (board[x][y] == -1) revealed[x][y] = true;
            }
        }
    }

    void handleDoorReveal(int x, int y, std::string& events)
    {
        // Centraliza el evento de puerta para REVEAL, cascada y recompensa de racha.
        if (x == doorX && y == doorY && !doorFound) {
            doorFound = true;
            appendEvent(events, "EVENT: DOOR_FOUND");
        }
    }

    void floodZeros(int sx, int sy, std::string& events)
    {
        // Cascada clasica del Buscaminas: si una celda 0 se revela, abre vecinas.
        std::queue<std::pair<int, int>> q;
        q.push({ sx, sy });

        const int dirs[8][2] = {
            {-1, -1}, {-1, 0}, {-1, 1},
            {0, -1},           {0, 1},
            {1, -1},  {1, 0},  {1, 1}
        };

        while (!q.empty()) {
            const auto [x, y] = q.front();
            q.pop();

            for (const auto& d : dirs) {
                const int nx = x + d[0];
                const int ny = y + d[1];
                if (!inBounds(nx, ny) || flagged[nx][ny] || revealed[nx][ny]) continue;
                if (board[nx][ny] == -1) continue;

                revealed[nx][ny] = true;
                chest[nx][ny] = false;
                handleDoorReveal(nx, ny, events);

                if (board[nx][ny] == 0) q.push({ nx, ny });
            }
        }
    }
};

static std::string upper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
        });
    return text;
}

static std::vector<std::string> split(const std::string& text, char delimiter)
{
    // Los comandos RPC usan separador '|', por ejemplo REVEAL|2|3.
    std::vector<std::string> parts;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        parts.push_back(item);
    }
    return parts;
}

static bool parseInt(const std::string& text, int& value)
{
    // Conversor defensivo: evita que un comando mal escrito provoque excepciones fuera.
    try {
        size_t parsed = 0;
        const int result = std::stoi(text, &parsed);
        if (parsed != text.size()) return false;
        value = result;
        return true;
    }
    catch (...) {
        return false;
    }
}

static std::string renderBoardPretty(const MinesweeperGame& game)
{
    // El servidor renderiza el tablero para mantener al cliente como terminal simple.
    if (!game.initialized) return "NO BOARD\n";

    std::ostringstream out;
    out << "Lives: " << game.lives
        << " | Coins: " << game.coins
        << " | Scans: " << game.scans
        << " | Turn: " << game.turns
        << " | Life Price: " << MinesweeperGame::LIFE_PRICE
        << " | Scan Price: " << MinesweeperGame::SCAN_PRICE << "\n";
    out << "Door: " << (game.doorFound ? "found" : "hidden")
        << " | Streak: " << game.streak << "/5\n";

    out << "    ";
    for (int y = 0; y < game.cols; ++y) {
        out << (y % 10) << ' ';
    }
    out << "\n";

    for (int x = 0; x < game.rows; ++x) {
        out << std::setw(2) << (x % 100) << ": ";
        for (int y = 0; y < game.cols; ++y) {
            out << game.cellChar(x, y) << ' ';
        }
        out << "\n";
    }

    return out.str();
}

static std::string stateResponse(const std::string& prefix, const MinesweeperGame& game, const std::string& events = "")
{
    // Respuesta comun para acciones que devuelven tablero y eventos opcionales.
    std::string out = prefix + "|" + renderBoardPretty(game);
    if (!events.empty()) out += events + "\n";
    return out;
}

static std::string processReveal(MinesweeperGame& game, const std::vector<std::string>& parts)
{
    // Envoltorio RPC de REVEAL: ademas de revelar, actualiza turno, racha y victoria.
    if (parts.size() != 3) return "ERROR|BAD_REVEAL";

    int x = 0;
    int y = 0;
    if (!parseInt(parts[1], x) || !parseInt(parts[2], y)) return "ERROR|BAD_REVEAL";

    std::string events;
    bool safeProgress = false;
    bool actionTaken = false;
    const std::string result = game.reveal(x, y, events, safeProgress, actionTaken);

    if (actionTaken) {
        ++game.turns;
        if (safeProgress) {
            ++game.streak;
            if (game.streak >= 5 && !game.gameOver) {
                const int bonus = game.applyStreakReward(x, y, 9, events);
                if (bonus > 0) {
                    if (!events.empty()) events += "\n";
                    events += "EVENT: STREAK_REWARD +" + std::to_string(bonus) + " cells";
                }
                game.streak = 0;
            }
        }
        else {
            game.streak = 0;
        }
    }

    if (!game.gameOver && game.checkWin()) {
        game.gameOver = true;
        game.win = true;
    }

    if (game.gameOver) {
        return stateResponse(std::string("GAMEOVER|") + (game.win ? "WIN|STATE" : "LOSE|STATE"), game, events);
    }

    if (result == "OK") return stateResponse("OK|STATE", game, events);
    return result;
}

static std::string processCommand(MinesweeperGame& game, const std::string& rawCommand, bool& closeAfterResponse)
{
    // Despachador central de RPC. Cada rama valida argumentos y devuelve OK, ERROR,
    // BYE o GAMEOVER sin cerrar el proceso completo.
    const std::vector<std::string> parts = split(rawCommand, '|');
    if (parts.empty() || parts[0].empty()) return "ERROR|UNKNOWN_CMD";

    const std::string action = upper(parts[0]);

    if (action == "INIT") {
        // INIT es obligatorio antes de jugar porque crea el tablero del hilo.
        if (parts.size() != 4) return "ERROR|BAD_INIT";

        int rows = 0;
        int cols = 0;
        int mines = 0;
        if (!parseInt(parts[1], rows) || !parseInt(parts[2], cols) || !parseInt(parts[3], mines)) {
            return "ERROR|BAD_INIT";
        }

        if (!game.init(rows, cols, mines)) return "ERROR|BAD_INIT";
        return "OK|INIT";
    }

    if (action == "EXIT") {
        // Cierre ordenado: el hilo respondera BYE y terminara la sesion.
        closeAfterResponse = true;
        return "BYE";
    }

    if (action == "STATE") {
        // Consulta de solo lectura, util para jugar y para trazas de depuracion.
        if (!game.initialized) return "ERROR|NO_INIT";
        return stateResponse("OK|STATE", game);
    }

    if (action == "PASS") {
        // Turno sin accion de tablero; reinicia la racha porque se rompe la cadena.
        if (!game.initialized) return "ERROR|NO_INIT";
        if (game.gameOver) return "ERROR|GAME_OVER";
        ++game.turns;
        game.streak = 0;
        return stateResponse("OK|PASS|turn=" + std::to_string(game.turns) + "|STATE", game);
    }

    if (action == "REVEAL") {
        return processReveal(game, parts);
    }

    if (action == "FLAG") {
        if (parts.size() != 3) return "ERROR|BAD_FLAG";
        int x = 0;
        int y = 0;
        if (!parseInt(parts[1], x) || !parseInt(parts[2], y)) return "ERROR|BAD_FLAG";
        if (!game.toggleFlag(x, y)) return game.initialized ? "ERROR|FLAG_FAIL" : "ERROR|NO_INIT";

        ++game.turns;
        return stateResponse("OK|STATE", game);
    }

    if (action == "SHOW") {
        if (parts.size() != 3) return "ERROR|BAD_SHOW";
        if (!game.initialized) return "ERROR|NO_INIT";

        int x = 0;
        int y = 0;
        if (!parseInt(parts[1], x) || !parseInt(parts[2], y)) return "ERROR|BAD_SHOW";
        if (!game.inBounds(x, y)) return "ERROR|OUT_OF_RANGE";

        return std::string("OK|CELL|") + std::to_string(x) + "|" + std::to_string(y) + "|" + game.cellChar(x, y);
    }

    if (action == "SHOP") {
        // Tienda de la memoria: consultar estado o comprar LIFE/SCAN si hay monedas.
        if (parts.size() < 2) return "ERROR|BAD_SHOP";
        if (!game.initialized) return "ERROR|NO_INIT";

        const std::string sub = upper(parts[1]);
        if (sub == "STATE" && parts.size() == 2) {
            return "SHOP|coins=" + std::to_string(game.coins)
                + "|lives=" + std::to_string(game.lives)
                + "|scans=" + std::to_string(game.scans)
                + "|price_life=" + std::to_string(MinesweeperGame::LIFE_PRICE)
                + "|price_scan=" + std::to_string(MinesweeperGame::SCAN_PRICE);
        }

        if (sub == "BUY" && parts.size() == 3) {
            if (game.gameOver) return "ERROR|GAME_OVER";
            const std::string item = upper(parts[2]);
            if (item == "LIFE") {
                if (game.coins < MinesweeperGame::LIFE_PRICE) return "ERROR|SHOP_NOT_ENOUGH_COINS";
                game.coins -= MinesweeperGame::LIFE_PRICE;
                ++game.lives;
                ++game.turns;
                return stateResponse("OK|SHOP_BUY|LIFE|STATE", game);
            }
            if (item == "SCAN") {
                if (game.coins < MinesweeperGame::SCAN_PRICE) return "ERROR|SHOP_NOT_ENOUGH_COINS";
                game.coins -= MinesweeperGame::SCAN_PRICE;
                ++game.scans;
                ++game.turns;
                return stateResponse("OK|SHOP_BUY|SCAN|STATE", game);
            }
        }

        return "ERROR|BAD_SHOP";
    }

    if (action == "USE") {
        // Uso de objetos. Actualmente solo existe SCAN para mantenerlo simple.
        if (parts.size() != 4) return "ERROR|BAD_USE";
        if (upper(parts[1]) != "SCAN") return "ERROR|BAD_USE";

        int x = 0;
        int y = 0;
        if (!parseInt(parts[2], x) || !parseInt(parts[3], y)) return "ERROR|BAD_USE";

        const std::string scanResult = game.scanCell(x, y);
        if (scanResult.rfind("OK|SCAN|", 0) != 0) return scanResult;
        return scanResult + "\n" + stateResponse("OK|STATE", game);
    }

    if (action == "CHEST") {
        // Comando administrativo/jugable para generar recompensas en el tablero.
        if (parts.size() != 3 || upper(parts[1]) != "SPAWN") return "ERROR|BAD_CHEST";
        if (!game.initialized) return "ERROR|NO_INIT";
        if (game.gameOver) return "ERROR|GAME_OVER";

        int count = 0;
        if (!parseInt(parts[2], count) || count < 0) return "ERROR|BAD_CHEST_SPAWN";

        const int spawned = game.spawnChests(count);
        ++game.turns;
        return "OK|CHEST|spawned=" + std::to_string(spawned) + "\n" + stateResponse("OK|STATE", game);
    }

    return "ERROR|UNKNOWN_CMD";
}

static void clientSession(SOCKET dedicatedSocket, int sessionId)
{
    // One thread executes this function for one client. Its game object is not shared.
    const std::string prefix = "Server[" + std::to_string(sessionId) + "]:";
    MinesweeperGame game;
    bool closeAfterResponse = false;

    std::cout << prefix << " session started" << std::endl;
    while (!closeAfterResponse) {
        sockaddr_in sender;
        DataPacket req;
        const int received = recvfromMsg(dedicatedSocket, &sender, &req, prefix);
        if (received == SOCKET_ERROR) continue;

        const std::string responseText = processCommand(game, req.msg, closeAfterResponse);
        DataPacket resp(req.client_id, req.sequence, responseText);
        sendtoMsg(dedicatedSocket, &sender, &resp, prefix);

        if (responseText.rfind("GAMEOVER|", 0) == 0) {
            closeAfterResponse = true;
        }
    }

    closesocket(dedicatedSocket);
    std::cout << prefix << " session closed" << std::endl;
}

int main()
{
    std::cout << "Servidor Buscaminas RPC UDP iniciando..." << std::endl;

    // WinSock se inicializa una vez en el proceso servidor.
    WSAData wsa;
    const int startup = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (startup != NO_ERROR) {
        std::cout << "Server: WSAStartup error: " << startup << std::endl;
        return 1;
    }

    // Socket de lobby: puerto fijo conocido por todos los clientes.
    SOCKET lobbySocket;
    sockaddr_in lobbyAddr;
    udpServerSocketSetup(lobbySocket, "127.0.0.1", 4000, &lobbyAddr, "Lobby");

    int nextSessionId = 1;
    while (true) {
        // Lobby socket: only accepts CONNECT and assigns a dedicated UDP port.
        sockaddr_in clientAddr;
        DataPacket req;
        const int received = recvfromMsg(lobbySocket, &clientAddr, &req, "Lobby");
        if (received == SOCKET_ERROR) continue;

        const std::string command = upper(req.msg);
        if (command != "CONNECT") {
            DataPacket error(req.client_id, req.sequence, "ERROR|CONNECT_FIRST");
            sendtoMsg(lobbySocket, &clientAddr, &error, "Lobby");
            continue;
        }

        // Socket dedicado: puerto dinamico para que cada cliente tenga su canal.
        SOCKET dedicatedSocket;
        sockaddr_in dedicatedAddr;
        udpServerSocketSetup(dedicatedSocket, "127.0.0.1", 0, &dedicatedAddr, "Dedicated");

        const int assignedPort = ntohs(dedicatedAddr.sin_port);
        DataPacket response(req.client_id, req.sequence, "OK|PORT|" + std::to_string(assignedPort));
        if (sendtoMsg(dedicatedSocket, &clientAddr, &response, "Lobby") == SOCKET_ERROR) {
            closesocket(dedicatedSocket);
            continue;
        }

        try {
            // Servidor multihilo pedido por el enunciado: el lobby no espera a que
            // acabe una partida, sino que crea un hilo para esa sesion.
            std::thread(clientSession, dedicatedSocket, nextSessionId++).detach();
        }
        catch (...) {
            closesocket(dedicatedSocket);
            DataPacket error(req.client_id, req.sequence, "ERROR|THREAD_START");
            sendtoMsg(lobbySocket, &clientAddr, &error, "Lobby");
        }
    }

    closesocket(lobbySocket);
    WSACleanup();
    return 0;
}
