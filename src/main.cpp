#include <iostream>
#include <map>
#include <deque>
#include <mutex>
#include <condition_variable>


using namespace std;

class TupleServer {
    map<string, deque<string>> tuple_space;     //cria mapa de tupla com palavra e Double-Ended Queue (deque) já que é uma FIFO
    mutex mtx;                                 //mutex para sincronização de acesso ao tuple_space, já que o deque pode dar problemas de concorrência
    condition_variable cv;                     //condition variable para notificar threads que estão esperando por uma tupla específica

public:
    void placeholder(){
        
    }
};