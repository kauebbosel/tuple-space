#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>

class TupleServer {
public:
    TupleServer();

    // WR: insere tupla (key, value). Nunca bloqueia.
    void write(std::string key, std::string value);

    // RD: leitura não destrutiva, bloqueante. Política FIFO por chave.
    std::string rd(std::string key);

    // IN: leitura destrutiva, bloqueante. Política FIFO por chave.
    std::string in(std::string key);

    // EX: consume tupla k_in, aplica svc_id, insere resultado em k_out.
    //     Retorna "OK" ou "NO-SERVICE".
    std::string ex(std::string k_in, std::string k_out, int svc_id);

private:
    // Checa se há ao menos uma tupla para a chave sem criar entrada no mapa.
    bool has_tuple(const std::string& key) const;

    std::mutex mtx;
    std::condition_variable cv;

    // Espaço de tuplas: chave -> fila FIFO de valores.
    std::map<std::string, std::deque<std::string>> tuple_space;

    // Tabela de serviços: svc_id -> função(string) -> string.
    std::map<int, std::function<std::string(std::string)>> services;
};