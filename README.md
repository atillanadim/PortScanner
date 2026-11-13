██████╗  ██████╗ ██████╗ ████████╗    ███████╗ ██████╗ █████╗ ███╗   ██╗███╗   ██╗███████╗██████╗
██╔══██╗██╔═══██╗██╔══██╗╚══██╔══╝    ██╔════╝██╔════╝██╔══██╗████╗  ██║████╗  ██║██╔════╝██╔══██╗
██████╔╝██║   ██║██████╔╝   ██║       ███████╗██║     ███████║██╔██╗ ██║██╔██╗ ██║█████╗  ██████╔╝
██╔═══╝ ██║   ██║██╔══██╗   ██║       ╚════██║██║     ██╔══██║██║╚██╗██║██║╚██╗██║██╔══╝  ██╔══██╗
██║     ╚██████╔╝██║  ██║   ██║       ███████║╚██████╗██║  ██║██║ ╚████║██║ ╚████║███████╗██║  ██║
╚═╝      ╚═════╝ ╚═╝  ╚═╝   ╚═╝       ╚══════╝ ╚═════╝╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝  ╚═══╝╚══════╝╚═╝  ╚═╝



Scanner de portas TCP assíncrono em C++17, feito com Boost.Asio e
Boost.Program_options.

## Como funciona

1. `main.cpp` lê os argumentos da linha de comando com `boost::program_options`.
2. `PortScanner::set_options()` converte o intervalo de portas, resolve o
   domínio/IP e guarda o endpoint.
3. `PortScanner::start()` monta uma fila com todas as portas e dispara
   `MAX_THREADS` tarefas de conexão.
4. Cada tarefa (`scan()`) abre um socket, inicia um timer de timeout e chama
   `async_connect`.
   - conexão aceita -> lê um banner (com timeout) e marca a porta como **OPEN**;
   - conexão recusada -> marca **CLOSED**;
   - timeout -> fecha o socket e marca **FILTERED**.
5. `PortScanner::run()` executa o `io_context` e imprime o resumo.

Apesar do nome `threads`, o programa é **assíncrono em uma única thread**: o
`io_context` com uma `strand` processa os eventos em sequência. O valor de
`-t` é a quantidade máxima de conexões simultâneas em andamento.

## Dependências

- Compilador C++17 (MinGW-w64/GCC ou MSVC)
- CMake 3.16+
- Boost (Asio + Program_options)

### Instalar o Boost no Windows (MSYS2)

```powershell
C:\msys64\usr\bin\pacman.exe -S --needed mingw-w64-x86_64-boost
```

## Compilar

Com CMake (recomendado):

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

O executável fica em `build\port_scanner.exe`.

Sem CMake, usando o g++ do PATH:

```powershell
g++ -std=c++17 -O2 -IC:/msys64/mingw64/include `
    main.cpp src/PortScanner.cpp `
    C:/msys64/mingw64/lib/libboost_program_options-mt.a `
    -o port_scanner.exe -lws2_32 -lmswsock
```

## Executar

```powershell
.\build\port_scanner.exe -h
.\build\port_scanner.exe -i 127.0.0.1 -p 1-1024
.\build\port_scanner.exe -i 192.168.1.1 -p 1-65535 -t 200
.\build\port_scanner.exe -i example.com -p 80-443 -e 5
```

Opções:

| Opção | Significado |
| --- | --- |
| `-i`, `--dname` | domínio ou IP (padrão `127.0.0.1`) |
| `-p`, `--ports` | intervalo de portas, `inicio-fim` (padrão `1-1024`) |
| `-t`, `--threads` | máximo de conexões simultâneas (padrão `100`) |
| `-e`, `--expiry_time` | timeout em segundos (padrão `2`) |
| `-h`, `--help` | ajuda |
| `-v`, `--verbose` | reservado (ainda não usado) |

> Use apenas em sistemas que você possui ou tem autorização explícita para
> testar.
