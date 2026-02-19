#pragma once

// Winsock2 deve ser incluído antes de qualquer header do Windows
// para evitar conflitos com <windows.h>.
#include <winsock2.h>
#include <ws2tcpip.h>

#include "main.hpp"
#include <string>

// Linka automaticamente com a biblioteca Winsock (equivale a -lws2_32).
#pragma comment(lib, "ws2_32.lib")

class TcpServer {
public:
    // port: porta TCP a escutar (ex: 54321).
    TcpServer(TupleServer& ts, unsigned short port);
    ~TcpServer();

    // Bloqueia aceitando conexões até o processo ser encerrado.
    void run();

private:
    // Loop de sessão: roda em thread dedicada por cliente.
    void session(SOCKET client_sock);

    // Parse e execução de um comando de texto.
    // Retorna a string de resposta (com '\n' no final).
    std::string process_command(const std::string& line);

    // Lê bytes até '\n' (descartando '\r').
    // Retorna false se a conexão foi encerrada.
    bool read_line(SOCKET client_sock, std::string& out);

    TupleServer&   ts_;
    unsigned short port_;
    SOCKET         server_sock_;  // socket de escuta
};