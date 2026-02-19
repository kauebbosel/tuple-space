#include "tcp_server.hpp"

#include <cerrno>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

using namespace std;

// ---------------------------------------------------------------------------
// Auxiliar: retorna a mensagem de erro do último erro Winsock.
// WSAGetLastError() é o equivalente Windows de errno para sockets.
// ---------------------------------------------------------------------------
static string wsa_error(const char* prefix) {
    return string(prefix) + " (WSA erro: " + to_string(WSAGetLastError()) + ")";
}

// ---------------------------------------------------------------------------
// Construtor: inicializa Winsock e cria o socket de escuta.
// ---------------------------------------------------------------------------
TcpServer::TcpServer(TupleServer& ts, unsigned short port)
    : ts_(ts), port_(port), server_sock_(INVALID_SOCKET) {

    // WSAStartup: obrigatório no Windows antes de qualquer chamada Winsock.
    // Versão 2.2 é a mais recente e amplamente suportada.
    WSADATA wsa_data;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (ret != 0)
        throw runtime_error("WSAStartup() falhou: " + to_string(ret));

    server_sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock_ == INVALID_SOCKET)
        throw runtime_error(wsa_error("socket()"));

    // SO_REUSEADDR evita "Address already in use" ao reiniciar o servidor.
    int opt = 1;
    ::setsockopt(server_sock_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&opt), sizeof(opt));
    //           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // No Windows, setsockopt espera const char* em vez de const void*.

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);

    if (::bind(server_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
        throw runtime_error(wsa_error("bind()"));

    if (::listen(server_sock_, SOMAXCONN) == SOCKET_ERROR)
        throw runtime_error(wsa_error("listen()"));
}

// ---------------------------------------------------------------------------
// Destrutor: fecha o socket e encerra o Winsock.
// ---------------------------------------------------------------------------
TcpServer::~TcpServer() {
    if (server_sock_ != INVALID_SOCKET) {
        ::closesocket(server_sock_);  // Windows: closesocket() em vez de close()
        server_sock_ = INVALID_SOCKET;
    }
    WSACleanup();  // Encerra o subsistema Winsock; par obrigatório do WSAStartup.
}

// ---------------------------------------------------------------------------
// run(): loop principal de accept.
// ---------------------------------------------------------------------------
void TcpServer::run() {
    cout << "Servidor Linda ouvindo na porta " << port_ << endl;

    while (true) {
        sockaddr_in client_addr{};
        int client_len = sizeof(client_addr);  // Windows usa int, não socklen_t

        SOCKET client_sock = ::accept(
            server_sock_,
            reinterpret_cast<sockaddr*>(&client_addr),
            &client_len
        );

        if (client_sock == INVALID_SOCKET) {
            cerr << "[ERRO] accept(): WSA erro " << WSAGetLastError() << endl;
            continue;
        }

        // Cada cliente recebe um thread dedicado.
        // detach() imediato: o thread fecha o socket e se auto-destrói.
        thread([this, client_sock]() {
            session(client_sock);
            ::closesocket(client_sock);  // Windows: closesocket()
        }).detach();
    }
}

// ---------------------------------------------------------------------------
// read_line(): lê bytes até '\n', descartando '\r'.
// Retorna false se a conexão foi encerrada.
// ---------------------------------------------------------------------------
bool TcpServer::read_line(SOCKET client_sock, string& out) {
    out.clear();
    char ch;
    while (true) {
        // recv() é idêntico em POSIX e Windows; retorna SOCKET_ERROR em vez de -1.
        int n = ::recv(client_sock, &ch, 1, 0);
        if (n <= 0)
            return false;   // 0 = conexão encerrada; SOCKET_ERROR = erro
        if (ch == '\n')
            return true;
        if (ch != '\r')
            out += ch;
    }
}

// ---------------------------------------------------------------------------
// session(): loop por cliente.
// ---------------------------------------------------------------------------
void TcpServer::session(SOCKET client_sock) {
    string line;
    while (read_line(client_sock, line)) {
        if (line.empty())
            continue;

        string response = process_command(line);

        // Envio completo mesmo que o Winsock fragmente internamente.
        const char* buf       = response.data();
        int         remaining = static_cast<int>(response.size());
        while (remaining > 0) {
            // send() no Windows também retorna SOCKET_ERROR em caso de falha.
            int sent = ::send(client_sock, buf, remaining, 0);
            if (sent == SOCKET_ERROR) return;
            buf       += sent;
            remaining -= sent;
        }
    }
}

// ---------------------------------------------------------------------------
// process_command(): parse e despacho para o TupleServer.
// ---------------------------------------------------------------------------
string TcpServer::process_command(const string& line) {
    istringstream iss(line);
    string cmd;
    if (!(iss >> cmd))
        return "ERROR\n";

    // ------------------------------------------------------------------ WR
    if (cmd == "WR") {
        string key;
        if (!(iss >> key))
            return "ERROR\n";
        string value;
        getline(iss, value);
        if (!value.empty() && value.front() == ' ')
            value.erase(0, 1);
        ts_.write(move(key), move(value));
        return "OK\n";
    }

    // ------------------------------------------------------------------ RD
    if (cmd == "RD") {
        string key;
        if (!(iss >> key))
            return "ERROR\n";
        return "OK " + ts_.rd(key) + "\n";
    }

    // ------------------------------------------------------------------ IN
    if (cmd == "IN") {
        string key;
        if (!(iss >> key))
            return "ERROR\n";
        return "OK " + ts_.in(key) + "\n";
    }

    // ------------------------------------------------------------------ EX
    if (cmd == "EX") {
        string k_in, k_out;
        int    svc_id;
        if (!(iss >> k_in))   return "ERROR\n";
        if (!(iss >> k_out))  return "ERROR\n";
        if (!(iss >> svc_id)) return "ERROR\n";
        return ts_.ex(k_in, k_out, svc_id) + "\n";
    }

    return "ERROR\n";
}