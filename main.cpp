#include "main.hpp"
#include "tcp_server.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

// Lê a porta de um arquivo "config.txt" (primeira linha = número inteiro).
// Se o arquivo não existir ou for inválido, retorna o valor padrão.
static unsigned short load_port(const char* config_file, unsigned short default_port) {
    std::ifstream f(config_file);
    if (!f.is_open())
        return default_port;

    int port = 0;
    f >> port;
    if (port <= 0 || port > 65535) {
        std::cerr << "[AVISO] Porta inválida em " << config_file
                  << "; usando " << default_port << ".\n";
        return default_port;
    }
    return static_cast<unsigned short>(port);
}

int main() {
    const unsigned short DEFAULT_PORT = 54321;
    unsigned short port = load_port("config.txt", DEFAULT_PORT);

    try {
        TupleServer ts;
        TcpServer   server(ts, port);
        server.run();   // bloqueia até o processo ser encerrado (Ctrl+C)
    } catch (const std::exception& e) {
        std::cerr << "[ERRO FATAL] " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}