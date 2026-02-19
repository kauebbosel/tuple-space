#include "main.hpp"

#include <algorithm>
#include <cctype>

using namespace std;

// ---------------------------------------------------------------------------
// Construtor: registra os três serviços obrigatórios.
// ---------------------------------------------------------------------------
TupleServer::TupleServer() {
    // Serviço 1: converter para maiúsculas.
    services[1] = [](string s) {
        string r;
        r.reserve(s.size());
        for (unsigned char c : s)
            r += static_cast<char>(std::toupper(c));
        return r;
    };

    // Serviço 2: inverter a string.
    services[2] = [](string s) {
        return string(s.rbegin(), s.rend());
    };

    // Serviço 3: retornar o tamanho como texto.
    services[3] = [](string s) {
        return to_string(s.size());
    };
}

// ---------------------------------------------------------------------------
// Auxiliar: verifica se existe ao menos uma tupla para a chave.
// Usa find() para NÃO criar entrada vazia no mapa com operator[].
// ---------------------------------------------------------------------------
bool TupleServer::has_tuple(const string& key) const {
    auto it = tuple_space.find(key);
    return it != tuple_space.end() && !it->second.empty();
}

// ---------------------------------------------------------------------------
// WR: insere sem bloquear.
// ---------------------------------------------------------------------------
void TupleServer::write(string key, string value) {
    {
        unique_lock<mutex> lock(mtx);
        tuple_space[key].push_back(move(value));
    }
    // Notifica fora do lock: threads acordadas não precisam esperar
    // que este thread libere o mutex para adquiri-lo.
    cv.notify_all();
}

// ---------------------------------------------------------------------------
// RD: bloqueante, não destrutivo, FIFO.
// ---------------------------------------------------------------------------
string TupleServer::rd(string key) {
    unique_lock<mutex> lock(mtx);
    // CORREÇÃO: usa has_tuple() com find() em vez de operator[],
    // que criaria uma entrada vazia no mapa para chaves inexistentes.
    cv.wait(lock, [this, &key] { return has_tuple(key); });
    return tuple_space.at(key).front();
}

// ---------------------------------------------------------------------------
// IN: bloqueante, destrutivo, FIFO.
// ---------------------------------------------------------------------------
string TupleServer::in(string key) {
    unique_lock<mutex> lock(mtx);
    cv.wait(lock, [this, &key] { return has_tuple(key); });
    auto& dq = tuple_space.at(key);
    string v = move(dq.front());
    dq.pop_front();
    return v;
}

// ---------------------------------------------------------------------------
// EX: consume k_in, aplica serviço, insere em k_out.
// Se o serviço não existir, a tupla de entrada já foi consumida e
// retorna "NO-SERVICE" sem inserir nada — conforme o enunciado.
// ---------------------------------------------------------------------------
string TupleServer::ex(string k_in, string k_out, int svc_id) {
    string v;
    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this, &k_in] { return has_tuple(k_in); });
        auto& dq = tuple_space.at(k_in);
        v = move(dq.front());
        dq.pop_front();
    }
    // Liberamos o lock antes de procurar o serviço e de chamar write(),
    // que vai adquirir o lock novamente. Isso reduz o tempo de contenção.

    auto it = services.find(svc_id);
    if (it == services.end())
        return "NO-SERVICE";

    string vout = it->second(move(v));
    write(move(k_out), move(vout));  // já faz notify_all internamente
    return "OK";
}