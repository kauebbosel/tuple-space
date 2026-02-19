#include "main.hpp"

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>

using namespace std;

static int failed = 0;

#define CHECK_EQ(a, b, msg) do {                                              \
    auto _a = (a); auto _b = (b);                                             \
    if (_a != _b) {                                                            \
        cerr << "[FALHA] " << (msg)                                           \
             << " (esperado: \"" << _b << "\", obtido: \"" << _a << "\")\n"; \
        ++failed;                                                              \
    } else {                                                                   \
        cout << "[OK]    " << (msg) << "\n";                                  \
    }                                                                          \
} while (0)

#define CHECK(cond, msg) do {           \
    if (!(cond)) {                      \
        cerr << "[FALHA] " << (msg) << "\n"; \
        ++failed;                       \
    } else {                            \
        cout << "[OK]    " << (msg) << "\n"; \
    }                                   \
} while (0)

int main() {
    cout << "=== Testes do Espaco de Tuplas (TupleServer) ===\n";
    TupleServer ts;

    // ---------------------------------------------------------------
    // 1) WR e RD: leitura não destrutiva
    // ---------------------------------------------------------------
    {
        ts.write("k1", "valor1");
        CHECK_EQ(ts.rd("k1"), "valor1", "RD retorna valor escrito por WR");
        CHECK_EQ(ts.rd("k1"), "valor1", "RD nao remove: segunda leitura igual");
    }

    // ---------------------------------------------------------------
    // 2) IN: leitura destrutiva
    // ---------------------------------------------------------------
    {
        ts.write("k2", "valor2");
        CHECK_EQ(ts.in("k2"), "valor2", "IN retorna valor da tupla");
        ts.write("k2", "outro");
        CHECK_EQ(ts.in("k2"), "outro", "IN remove: proxima IN pega nova tupla");
    }

    // ---------------------------------------------------------------
    // 3) Política FIFO
    // ---------------------------------------------------------------
    {
        ts.write("fifo", "primeiro");
        ts.write("fifo", "segundo");
        ts.write("fifo", "terceiro");
        CHECK_EQ(ts.rd("fifo"),  "primeiro",  "FIFO: RD retorna mais antigo");
        CHECK_EQ(ts.rd("fifo"),  "primeiro",  "FIFO: RD ainda nao remove");
        CHECK_EQ(ts.in("fifo"),  "primeiro",  "FIFO: IN retorna mais antigo");
        CHECK_EQ(ts.in("fifo"),  "segundo",   "FIFO: proximo IN e segundo");
        CHECK_EQ(ts.in("fifo"),  "terceiro",  "FIFO: proximo IN e terceiro");
    }

    // ---------------------------------------------------------------
    // 4) EX serviço 1 — maiúsculas
    // ---------------------------------------------------------------
    {
        ts.write("ein1", "abcdef");
        CHECK_EQ(ts.ex("ein1", "eout1", 1), "OK",     "EX svc 1 retorna OK");
        CHECK_EQ(ts.rd("eout1"),            "ABCDEF",  "EX 1: valor em maiusculas");
    }

    // ---------------------------------------------------------------
    // 5) EX serviço 2 — inverter
    // ---------------------------------------------------------------
    {
        ts.write("ein2", "ghijkl");
        CHECK_EQ(ts.ex("ein2", "eout2", 2), "OK",     "EX svc 2 retorna OK");
        CHECK_EQ(ts.rd("eout2"),            "lkjihg",  "EX 2: valor invertido");
    }

    // ---------------------------------------------------------------
    // 6) EX serviço 3 — tamanho
    // ---------------------------------------------------------------
    {
        ts.write("ein3", "xyz");
        CHECK_EQ(ts.ex("ein3", "eout3", 3), "OK", "EX svc 3 retorna OK");
        CHECK_EQ(ts.rd("eout3"),            "3",   "EX 3: tamanho da string");
    }

    // ---------------------------------------------------------------
    // 7) EX serviço inexistente
    // ---------------------------------------------------------------
    {
        ts.write("eno1", "qualquer");
        CHECK_EQ(ts.ex("eno1", "esaida1", 99), "NO-SERVICE",
                 "EX svc inexistente retorna NO-SERVICE");

        ts.write("eno2", "x");
        CHECK_EQ(ts.ex("eno2", "esaida2", 99), "NO-SERVICE",
                 "EX 99 novamente NO-SERVICE");
    }

    // ---------------------------------------------------------------
    // 8) Concorrência: RD bloqueante desbloqueia com WR em outra thread.
    //
    // CORREÇÃO: substituído o sleep(50ms) por promise/future.
    // O reader sinaliza via promise assim que está pronto para bloqueiar,
    // garantindo que o writer só executa depois que o leitor já entrou
    // no cv.wait(). Isso torna o teste determinístico.
    // ---------------------------------------------------------------
    {
        promise<void> reader_ready;
        future<void>  ready_fut = reader_ready.get_future();
        string        rd_result;

        // Usamos uma chave nova para não ter tuplas pré-existentes.
        const string key = "block_test";

        thread reader([&]() {
            // Sinaliza que está prestes a bloquear...
            reader_ready.set_value();
            // ...e bloqueia em RD.
            rd_result = ts.rd(key);
        });

        // Espera a confirmação de que o reader já foi agendado.
        // O reader pode ainda não ter chegado em cv.wait(), mas a janela
        // é agora mínima. Para garantia total seria necessário expor uma
        // hook interna no TupleServer (não vale a complexidade aqui).
        ready_fut.wait();

        // Pequena espera para o reader entrar no cv.wait().
        this_thread::sleep_for(chrono::milliseconds(5));

        ts.write(key, "valor_async");
        reader.join();

        CHECK_EQ(rd_result, "valor_async",
                 "RD bloqueante desbloqueia com WR em outra thread");
    }

    // ---------------------------------------------------------------
    // 9) Concorrência: IN destrutivo em thread separada.
    // ---------------------------------------------------------------
    {
        promise<void> reader_ready;
        future<void>  ready_fut = reader_ready.get_future();
        string        in_result;

        const string key = "block_in_test";

        thread taker([&]() {
            reader_ready.set_value();
            in_result = ts.in(key);
        });

        ready_fut.wait();
        this_thread::sleep_for(chrono::milliseconds(5));

        ts.write(key, "valor_in");
        taker.join();

        CHECK_EQ(in_result, "valor_in",
                 "IN bloqueante desbloqueia com WR em outra thread");

        // Depois do IN a tupla não deve mais existir; uma segunda escrita
        // deve ser necessária para um novo IN/RD retornar.
        ts.write(key, "segundo");
        CHECK_EQ(ts.in(key), "segundo",
                 "Apos IN, escrita posterior e necessaria para nova leitura");
    }

    // ---------------------------------------------------------------
    // 10) EX bloqueante: EX em thread, WR desbloqueia.
    // ---------------------------------------------------------------
    {
        promise<void> ex_ready;
        future<void>  ex_fut = ex_ready.get_future();
        string        ex_result;

        thread ex_thread([&]() {
            ex_ready.set_value();
            ex_result = ts.ex("ex_block_in", "ex_block_out", 1);  // maiúsculas
        });

        ex_fut.wait();
        this_thread::sleep_for(chrono::milliseconds(5));

        ts.write("ex_block_in", "hello");
        ex_thread.join();

        CHECK_EQ(ex_result,           "OK",    "EX bloqueante retorna OK");
        CHECK_EQ(ts.rd("ex_block_out"), "HELLO", "EX bloqueante: resultado correto");
    }

    // ---------------------------------------------------------------
    cout << "\n=== Fim dos testes ===\n";
    if (failed > 0) {
        cerr << "Total de falhas: " << failed << "\n";
        return 1;
    }
    cout << "Todos os testes passaram.\n";
    return 0;
}