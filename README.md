# Servidor de Espaço de Tuplas — Linda (C++)

Implementação de um servidor concorrente de espaço de tuplas inspirado no modelo Linda, com comunicação via TCP. O servidor aceita múltiplos clientes simultaneamente e oferece as operações WR, RD, IN e EX sobre pares `(chave, valor)`.

---

## Estrutura de Arquivos

```
linda/
├── main.hpp            # Definição da classe TupleServer
├── main.cpp            # Entry point: instancia TupleServer e TcpServer
├── tuplespace.cpp      # Lógica do espaço de tuplas (WR, RD, IN, EX, serviços)
├── tcp_server.hpp      # Definição da classe TcpServer
├── tcp_server.cpp      # Implementação TCP com Winsock2 (Windows)
├── tests.cpp           # Testes unitários (sem dependência de rede)
├── tester_linda.cpp    # Cliente de teste fornecido pelo professor (adaptado para Windows)
├── Makefile            # Compilação do servidor e dos testes
└── config.txt          # (opcional) Número da porta na primeira linha
```

---

## Porta

A porta padrão é **54321**.

Para usar outra porta, crie um arquivo `config.txt` na mesma pasta do executável com o número da porta na primeira linha:

```
54321
```

Qualquer porta no intervalo **49152–65535** é adequada para evitar conflitos com serviços do sistema.

---

## Compilação

### Pré-requisitos

- Compilador MinGW-w64 (g++) com suporte a C++17
- Windows 7 ou superior (Winsock2 já incluso no sistema)

### Compilar o servidor

```bash
make server
```

Ou manualmente:

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 main.cpp tcp_server.cpp tuplespace.cpp -o linda_server.exe -lws2_32 -lpthread
```

### Compilar os testes unitários

```bash
make tests
```

Ou manualmente:

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 tests.cpp tuplespace.cpp -o linda_tests.exe -lws2_32 -lpthread
```

### Compilar o cliente de teste do professor

O `tester_linda.cpp` é compilado separadamente pois não faz parte do servidor:

```bash
g++ -std=c++17 tester_linda.cpp -o tester_linda -lws2_32
```

> **Atenção:** o comando original do professor (`g++ -std=c++17 tester_linda.cpp -o tester_linda`) não inclui `-lws2_32` e falha no Windows. É obrigatório adicionar esse flag para linkar a biblioteca Winsock2.

### Compilar tudo de uma vez

```bash
make all
```

### Limpar binários

```bash
make clean
```

---

## Execução

### 1. Rodar os testes unitários (sem rede)

```bash
./linda_tests.exe
```

Saída esperada:

```
=== Testes do Espaco de Tuplas (TupleServer) ===
[OK]    RD retorna valor escrito por WR
[OK]    RD nao remove: segunda leitura igual
[OK]    IN retorna valor da tupla
...
Todos os testes passaram.
```

### 2. Subir o servidor (Terminal 1)

```bash
./linda_server.exe
```

Saída esperada:

```
Servidor Linda ouvindo na porta 54321
```

O servidor fica bloqueado aguardando conexões. Para encerrar: `Ctrl+C`.

### 3. Rodar o tester do professor (Terminal 2)

```bash
./tester_linda.exe 127.0.0.1 54321
```

Saída esperada:

```
Conectado a 127.0.0.1:54321
[OK] WR teste1 resposta: "OK"
[OK] RD teste1 resposta: "OK valor1"
 (RD teste1 retornou: "OK valor1")
[OK] IN teste1 resposta: "OK valor1"
 (IN teste1 retornou: "OK valor1")
[OK] WR in1 resposta: "OK"
[OK] EX 1 resposta: "OK"
[OK] RD out1 apos EX 1 resposta: "OK ABCDEF"
 (RD out1 apos EX 1 retornou: "OK ABCDEF")
[OK] WR in2 resposta: "OK"
[OK] EX 2 resposta: "OK"
[OK] RD out2 apos EX 2 resposta: "OK lkjihg"
 (RD out2 apos EX 2 retornou: "OK lkjihg")
[OK] WR in3 resposta: "OK"
[OK] EX 99 resposta: "NO-SERVICE"
Testes basicos concluidos.
```

### 4. Teste manual com telnet ou netcat

```bash
# Opção A: telnet
telnet 127.0.0.1 54321

# Opção B: netcat (se disponível)
nc 127.0.0.1 54321
```

Exemplo de interação:

```
WR nome joao
OK
RD nome
OK joao
WR x abcdef
OK
EX x y 2
OK
RD y
OK fedcba
IN nome
OK joao
```

---

## Protocolo TCP

O servidor recebe comandos em texto puro, uma linha por comando, terminada em `\n`.

### Comandos

| Comando | Formato | Descrição |
|---|---|---|
| WR | `WR chave valor` | Insere a tupla. O valor pode conter espaços. |
| RD | `RD chave` | Leitura não destrutiva, bloqueante. |
| IN | `IN chave` | Leitura destrutiva, bloqueante. |
| EX | `EX chave_entrada chave_saida svc_id` | Executa serviço sobre a tupla. |

### Respostas

| Situação | Resposta |
|---|---|
| WR bem-sucedido | `OK` |
| RD ou IN bem-sucedido | `OK valor` |
| EX com serviço válido | `OK` |
| EX com serviço inexistente | `NO-SERVICE` |
| Comando inválido ou mal-formado | `ERROR` |

Todas as respostas são terminadas em `\n`.

---

## Semântica das Operações

**WR(chave, valor):** insere a tupla no espaço. Nunca bloqueia e nunca falha.

**RD(chave):** retorna o valor da tupla mais antiga com aquela chave (FIFO) sem removê-la. Se não existir tupla, bloqueia até que uma seja inserida por WR ou EX.

**IN(chave):** igual ao RD, mas remove a tupla retornada.

**EX(chave_entrada, chave_saida, svc_id):** bloqueia até existir uma tupla com `chave_entrada`, consome-a (como IN), aplica o serviço `svc_id` sobre o valor e insere o resultado como `(chave_saida, resultado)`. Se o serviço não existir, retorna `NO-SERVICE` sem inserir nada.

---

## Serviços Registrados

| svc_id | Descrição | Exemplo de entrada | Exemplo de saída |
|---|---|---|---|
| 1 | Converter para maiúsculas | `abcdef` | `ABCDEF` |
| 2 | Inverter a string | `ghijkl` | `lkjihg` |
| 3 | Retornar o tamanho como texto | `xyz` | `3` |

Qualquer `svc_id` não listado acima retorna `NO-SERVICE`.

---

## Concorrência e Sincronização

O servidor utiliza um modelo de **um thread por cliente**. Cada conexão aceita recebe um `std::thread` dedicado que é desvinculado com `detach()` imediatamente após a criação — o thread fecha o socket e se encerra automaticamente ao fim da sessão.

O `TupleServer` é protegido por um único `std::mutex` com `std::condition_variable`. Operações bloqueantes (RD, IN, EX) usam `cv.wait()` com predicado, sem busy-waiting. A notificação é feita fora do lock em `write()` para reduzir contenção.

---

## Adaptações para Windows

O projeto originalmente utilizava **sockets POSIX** (`arpa/inet.h`, `sys/socket.h`, `unistd.h`), que não estão disponíveis no Windows. Todas as chamadas foram migradas para **Winsock2** (`winsock2.h`, `ws2tcpip.h`). As diferenças práticas são:

O `tester_linda.cpp` recebeu as mesmas adaptações, e o comando de compilação foi ajustado para incluir `-lws2_32`:

```bash
# Comando correto no Windows (com -lws2_32):
g++ -std=c++17 tester_linda.cpp -o tester_linda -lws2_32

A lógica de protocolo, parsing e integração com o `TupleServer` permanece idêntica entre as versões.
