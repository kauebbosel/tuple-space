#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <sstream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

// Envia uma linha (cmd + '\n') e lê uma linha de resposta
std::string send_command(SOCKET sock, const std::string &cmd) {
    std::string to_send = cmd + "\n";
    int total_sent = 0;

    while (total_sent < static_cast<int>(to_send.size())) {
        int sent = ::send(sock, to_send.data() + total_sent,
                          static_cast<int>(to_send.size()) - total_sent, 0);
        if (sent == SOCKET_ERROR) {
            throw std::runtime_error("Erro ao enviar comando ao servidor");
        }
        total_sent += sent;
    }

    std::string response;
    char ch;
    while (true) {
        int rec = ::recv(sock, &ch, 1, 0);
        if (rec <= 0) {
            throw std::runtime_error("Conexao encerrada pelo servidor");
        }
        if (ch == '\n') {
            break;
        }
        if (ch != '\r') {
            response.push_back(ch);
        }
    }
    return response;
}

void expect_prefix(const std::string &resp, const std::string &prefix,
                   const std::string &context) {
    if (resp.rfind(prefix, 0) != 0) {
        std::cerr << "[FALHA] " << context
                  << " resposta inesperada: \"" << resp << "\"\n";
    } else {
        std::cout << "[OK] " << context
                  << " resposta: \"" << resp << "\"\n";
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Uso: " << argv[0] << " <host> <porta>\n";
        std::cerr << "Exemplo: " << argv[0] << " 127.0.0.1 54321\n";
        return 1;
    }

    std::string host = argv[1];
    std::string port = argv[2];

    // Inicializa o subsistema Winsock
    WSADATA wsa_data;
    int wsa_ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_ret != 0) {
        std::cerr << "WSAStartup falhou: " << wsa_ret << "\n";
        return 1;
    }

    // Cria socket e conecta
    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *result  = nullptr;

    int ret = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    if (ret != 0) {
        std::cerr << "getaddrinfo falhou: " << ret << "\n";
        WSACleanup();
        return 1;
    }

    SOCKET sock = INVALID_SOCKET;
    for (addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
        sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == INVALID_SOCKET)
            continue;

        if (::connect(sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0) {
            break; // conectou
        }
        ::closesocket(sock);
        sock = INVALID_SOCKET;
    }
    freeaddrinfo(result);

    if (sock == INVALID_SOCKET) {
        std::cerr << "Nao foi possivel conectar ao servidor\n";
        WSACleanup();
        return 1;
    }

    try {
        std::cout << "Conectado a " << host << ":" << port << "\n";

        // 1) Teste básico de WR e RD
        {
            std::string resp = send_command(sock, "WR teste1 valor1");
            expect_prefix(resp, "OK", "WR teste1");

            resp = send_command(sock, "RD teste1");
            expect_prefix(resp, "OK", "RD teste1");
            std::cout << " (RD teste1 retornou: \"" << resp << "\")\n";
        }

        // 2) Teste de IN removendo a tupla
        {
            std::string resp = send_command(sock, "IN teste1");
            expect_prefix(resp, "OK", "IN teste1");
            std::cout << " (IN teste1 retornou: \"" << resp << "\")\n";
        }

        // 3) Teste de EX com svc_id = 1
        {
            expect_prefix(send_command(sock, "WR in1 abcdef"), "OK", "WR in1");
            expect_prefix(send_command(sock, "EX in1 out1 1"), "OK", "EX 1");

            std::string resp = send_command(sock, "RD out1");
            expect_prefix(resp, "OK", "RD out1 apos EX 1");
            std::cout << " (RD out1 apos EX 1 retornou: \"" << resp << "\")\n";
        }

        // 4) Teste de EX com svc_id = 2
        {
            expect_prefix(send_command(sock, "WR in2 ghijkl"), "OK", "WR in2");
            expect_prefix(send_command(sock, "EX in2 out2 2"), "OK", "EX 2");

            std::string resp = send_command(sock, "RD out2");
            expect_prefix(resp, "OK", "RD out2 apos EX 2");
            std::cout << " (RD out2 apos EX 2 retornou: \"" << resp << "\")\n";
        }

        // 5) Teste de EX com servico inexistente
        {
            expect_prefix(send_command(sock, "WR in3 xyz"),      "OK",         "WR in3");
            expect_prefix(send_command(sock, "EX in3 out3 99"),  "NO-SERVICE", "EX 99");
        }

        std::cout << "Testes basicos concluidos.\n";

    } catch (const std::exception &e) {
        std::cerr << "Erro: " << e.what() << "\n";
        ::closesocket(sock);
        WSACleanup();
        return 1;
    }

    ::closesocket(sock);
    WSACleanup();
    return 0;
}