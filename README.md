# Mini-Sistema de Arquivos em Memória (C)

Trabalho M3 de **Sistemas Operacionais** (UNIVALI, Ciência da Computação, 5º período).
Enunciado: [docs/enunciado.pdf](docs/enunciado.pdf)

O programa é um **terminal parecido com o do Linux** em que dá para criar pastas e arquivos, escrever e ler conteúdo, copiar, mover, apagar e controlar permissões. A diferença é que **nada vai para o disco de verdade**: as pastas e os arquivos são estruturas em C na memória, e o conteúdo dos arquivos é guardado num **disco simulado** dividido em blocos.

---

## Sumário

1. [Como compilar e executar](#1-como-compilar-e-executar)
2. [Comandos disponíveis](#2-comandos-disponíveis)
3. [Organização do código](#3-organização-do-código)
4. [Escolhas de design e estruturas de dados](#4-escolhas-de-design-e-estruturas-de-dados)
5. [Relação com a teoria da disciplina](#5-relação-com-a-teoria-da-disciplina)
6. [Exemplos de uso e comparação com o Linux](#6-exemplos-de-uso-e-comparação-com-o-linux)
7. [Tratamento de erros](#7-tratamento-de-erros)
8. [Gerenciamento de memória](#8-gerenciamento-de-memória)
9. [Limitações conhecidas](#9-limitações-conhecidas)

---

## 1. Como compilar e executar

Precisa de `gcc` e `make` (Linux, WSL ou macOS). No Ubuntu/WSL: `sudo apt install build-essential`.

```bash
make          # compila e gera o executável "minifs"
./minifs      # abre o terminal do simulador
```

Outros alvos do Makefile:

```bash
make valgrind   # roda com o valgrind procurando memory leaks (Linux)
make clean      # apaga os arquivos compilados
```

Também dá para rodar uma sequência de comandos pronta, sem digitar:

```bash
./minifs < demo/roteiro_apresentacao.txt
```

---

## 2. Comandos disponíveis

| Comando | O que faz | Objetivo do enunciado |
|---------|-----------|-----------------------|
| `mkdir <caminho>` | cria uma pasta (aceita `docs`, `a/b` ou `/home/alice`) | 3.1 |
| `cd [caminho]` | entra numa pasta (sem caminho volta para `/`) | 3.1 |
| `ls [-l] [caminho]` | lista o conteúdo (`-l` mostra permissões, dono, tamanho, data) | 3.1 |
| `pwd` | mostra a pasta atual | 3.1 |
| `rmdir <pasta>` | apaga uma pasta vazia | 3.1 |
| `touch <arquivo>` | cria arquivo vazio (ou atualiza as datas se já existe) | 3.2 |
| `echo <texto> > <arquivo>` | escreve no arquivo, substituindo o conteúdo | 3.2 |
| `echo <texto> >> <arquivo>` | adiciona no final do arquivo | 3.2 |
| `cat <arquivo>` | mostra o conteúdo | 3.2 |
| `cp <origem> <destino>` | copia um arquivo | 3.2 |
| `mv <origem> <destino>` | move ou renomeia (arquivo ou pasta) | 3.2 |
| `rm <arquivo>` | apaga um arquivo | 3.2 |
| `stat <arquivo>` | mostra todos os campos do FCB (inode), incluindo os blocos | 3.2 / 3.4 |
| `chmod <modo> <arquivo>` | muda as permissões, ex.: `chmod 754 a.txt` | 3.3 |
| `./<arquivo>` | "executa" o arquivo (testa a permissão `x`) | 3.3 |
| `su <usuario>` | troca de usuário: `root`, `alice`, `bob`, `guest` | 3.3 |
| `whoami` | mostra o usuário atual | 3.3 |
| `disk` | mostra o mapa de blocos do disco simulado | 3.4 |
| `help` / `exit` | ajuda / sair | |

Todos os comandos aceitam caminhos absolutos (`/home/alice/a.txt`) e relativos (`../docs/a.txt`).

**Usuários de teste** (para demonstrar as três classes de permissão):

| Usuário | id | grupo | Para que serve |
|---------|----|-------|----------------|
| `root`  | 0    | 0   | pode tudo, ignora permissões (igual ao Linux) |
| `alice` | 1000 | 100 | usuária comum |
| `bob`   | 1001 | 100 | mesmo grupo da alice → testa a classe **grupo** |
| `guest` | 2000 | 200 | grupo diferente → testa a classe **outros** |

---

## 3. Organização do código

Todos os nomes (arquivos, funções, variáveis) estão **em português e descrevem o que fazem**. Os termos da disciplina (FCB, inode) foram mantidos. Os comandos do terminal seguem os nomes do Linux.

| Arquivo | O que tem |
|---------|-----------|
| [`src/sistema_arquivos.h`](src/sistema_arquivos.h) | Constantes, `enum TipoArquivo`, `struct FCB`, `struct NoArvore`, `struct Usuario` e a declaração de todas as funções |
| [`src/arvore.c`](src/arvore.c) | Criar, percorrer e liberar a árvore; transformar caminho em nó |
| [`src/comandos_diretorio.c`](src/comandos_diretorio.c) | `mkdir`, `cd`, `ls`, `pwd`, `rmdir` (objetivo 3.1) |
| [`src/comandos_arquivo.c`](src/comandos_arquivo.c) | `touch`, `echo`, `cat`, `cp`, `mv`, `rm`, `stat`, `./` (objetivo 3.2) |
| [`src/permissoes.c`](src/permissoes.c) | Verificação RWX com bitmask, texto `drwxr-xr-x`, `chmod` (objetivo 3.3) |
| [`src/disco.c`](src/disco.c) | Disco simulado, reservar/liberar blocos, gravar/ler conteúdo, `disk` (objetivo 3.4) |
| [`src/terminal.c`](src/terminal.c) | Lê a linha digitada e chama o comando certo; usuários de teste |
| [`src/main.c`](src/main.c) | Inicia o sistema, roda o terminal e libera a memória no final |

---

## 4. Escolhas de design e estruturas de dados

### 4.1 O FCB (o "inode" simulado)

Cada arquivo e cada pasta tem um `struct FCB` com todos os seus atributos:

```c
typedef struct {
    int          numero_inode;        /* identificador único */
    char         nome[32];
    TipoArquivo  tipo;                /* diretório, texto, numérico, binário, programa */
    size_t       tamanho_em_bytes;
    time_t       data_de_criacao;
    time_t       data_de_modificacao;
    time_t       data_do_ultimo_acesso;
    int          permissoes;          /* bitmask, ex.: 0754 */
    int          id_do_dono;
    int          id_do_grupo;
    int          blocos_do_arquivo[16];   /* alocação indexada */
    int          quantidade_de_blocos;
} FCB;
```

O FCB **não guarda o conteúdo** do arquivo. Ele guarda só a lista de **quais blocos do disco** têm o conteúdo, igual a um inode real.

### 4.2 A árvore de diretórios: "primeiro filho / próximo irmão"

Uma pasta pode ter qualquer quantidade de filhos. Em vez de usar um vetor de tamanho fixo de filhos, cada nó tem só três ponteiros:

```c
typedef struct NoArvore {
    FCB              fcb;
    struct NoArvore *pai;             /* para o "cd .." e para montar o caminho */
    struct NoArvore *primeiro_filho;  /* início da lista de filhos */
    struct NoArvore *proximo_irmao;   /* próximo item da mesma pasta */
} NoArvore;
```

```
        /                         /  ──primeiro_filho──▶ home
        │                                                 │
       home                                        primeiro_filho
      ┌─┴──┐                                              ▼
   alice   bob                           alice ──proximo_irmao──▶ bob
```

Vantagens dessa escolha:
- **Sem limite** de arquivos por pasta: a lista cresce com `malloc` conforme a necessidade.
- **Sem desperdício**: uma pasta vazia não reserva espaço para filhos.
- Inserir é **O(1)**: o novo filho entra no começo da lista. Por isso o `ls` mostra primeiro o item mais recente.
- Procurar um nome numa pasta é **O(n)**, sendo n o número de itens **daquela pasta**, e não do sistema inteiro.

Os nós são criados com `calloc` (memória zerada, ponteiros começam em `NULL`) e liberados com `free` de forma recursiva.

### 4.3 Permissões como bitmask

As permissões são um único `int` com 9 bits (`rwxrwxrwx`), escrito em octal no código (`0754`). Todas as operações usam **operadores bit a bit**: `>>` e `&` para ler um trio, `<<` e `|` para montar o número no `chmod`. Os detalhes estão na seção 5.5.

### 4.4 O disco simulado e a alocação indexada

```c
static char disco[256 * 64];          /* 256 blocos de 64 bytes = 16 KB */
static bool bloco_esta_ocupado[256];  /* mapa de blocos livres */
```

Para gravar, o conteúdo é cortado em pedaços de 64 bytes. Cada pedaço vai para o **primeiro bloco livre** (estratégia *first-fit*), e o número do bloco é anotado em `fcb.blocos_do_arquivo[]`. Os detalhes estão na seção 5.6.

### 4.5 Usuário atual

Uma variável global `Usuario usuario_atual`, como o enunciado sugere. O comando `su` troca o valor dela, e todos os comandos consultam essa variável antes de agir.

---

## 5. Relação com a teoria da disciplina

### 5.1 Conceito de arquivo e seus atributos

Na teoria, um arquivo é um **tipo abstrato de dados**: uma sequência de bytes com um nome e um conjunto de **atributos** (nome, identificador, tipo, localização, tamanho, proteção, datas). No projeto, cada atributo é um campo do `FCB`:

| Atributo (teoria) | Campo no código |
|-------------------|-----------------|
| Nome | `nome` |
| Identificador | `numero_inode` (gerado por um contador, nunca se repete) |
| Tipo | `tipo` (`enum TipoArquivo`). É definido pela extensão (`.sh` = programa, `.bin` = binário) ou pelo conteúdo (só dígitos = numérico) |
| Localização | `blocos_do_arquivo[]` + `quantidade_de_blocos` |
| Tamanho | `tamanho_em_bytes` |
| Proteção | `permissoes`, `id_do_dono`, `id_do_grupo` |
| Data e hora | `data_de_criacao`, `data_de_modificacao`, `data_do_ultimo_acesso` (`time_t`) |

O comando `stat` mostra todos esses campos de uma vez.

### 5.2 Operações com arquivos

A teoria lista seis operações básicas. Todas existem no simulador e atualizam os atributos corretos:

| Operação (teoria) | Comando | O que acontece por dentro |
|-------------------|---------|---------------------------|
| **Criar** | `touch`, `echo >` | aloca um `NoArvore` com `calloc`, preenche o FCB, liga na pasta |
| **Escrever** | `echo >`, `echo >>` | corta o texto em blocos, grava no disco, atualiza tamanho e data de modificação |
| **Ler** | `cat` | junta os blocos na ordem do FCB, atualiza a data de acesso |
| **Reposicionar** | `echo >>` | a escrita começa no fim do arquivo, em vez do começo |
| **Excluir** | `rm` | tira o nó da pasta, devolve os blocos ao disco, `free` no nó |
| **Truncar** | `echo >` sobre arquivo existente | descarta o conteúdo antigo (libera os blocos) e grava o novo |

Também foram implementadas operações compostas: `cp` (ler + criar + escrever), `mv` (só move o nó na árvore, sem copiar dados) e `stat` (ler os atributos).

**`cp` × `mv`:** o `cp` cria um **inode novo** com **blocos novos**. O `mv` mantém **o mesmo inode e os mesmos blocos**: só troca o nó de lugar e/ou de nome. Dá para ver isso com o `stat` antes e depois de cada um.

### 5.3 O File Control Block (FCB) e o inode simulado

O FCB é a estrutura que o sistema operacional mantém para cada arquivo. No Unix ele se chama **inode**. Duas ideias da teoria aparecem no código:

1. **Metadados separados do conteúdo:** o FCB fica no nó da árvore, e o conteúdo fica no disco. Um arquivo vazio tem FCB, mas nenhum bloco.
2. **Nome × identidade:** o nome é só um rótulo dentro da pasta, e a identidade é o `numero_inode`. Por isso o `mv` renomeia sem mudar o inode.

### 5.4 Estrutura de diretórios em árvore

A teoria apresenta diretórios de nível único, de dois níveis e em árvore. O projeto implementa a **árvore**, que é o modelo do Linux:

- **Eficiência:** para achar um arquivo, o sistema só percorre as pastas do caminho, e não todos os arquivos. `encontrar_pelo_caminho` divide `/home/alice/a.txt` em partes e desce um nível por vez.
- **Nomeação:** dois usuários podem ter arquivos com o mesmo nome em pastas diferentes (`/home/alice/notas.txt` e `/home/bob/notas.txt`). O nome só precisa ser único **dentro da pasta**, e `mkdir`/`touch` verificam isso.
- **Agrupamento:** arquivos relacionados ficam juntos na mesma pasta, e subpastas organizam em níveis.
- **Caminhos absolutos e relativos:** começam em `/` (raiz) ou na pasta atual, e `.` e `..` funcionam. O pai da raiz é a própria raiz, então `cd ..` em `/` continua em `/`.
- **Proteção contra ciclos:** `mv pasta pasta/sub` é bloqueado (`esta_dentro_de`), porque criaria um laço e a estrutura deixaria de ser árvore.

### 5.5 Proteção de acesso: RWX, dono/grupo/outros e bitmask

Cada arquivo tem 9 bits de permissão:

```
   dono    grupo   outros
   r w x   r w x   r w x
   4 2 1   4 2 1   4 2 1
```

**`chmod 754`** vira o número octal `0754`. O código monta esse número com deslocamento e OU bit a bit (`permissoes.c`, `converter_modo_para_permissoes`):

```
  7 << 6  =  111 000 000
  5 << 3  =      101 000
  4 << 0  =          100
            ─────────────  |
             111 101 100   = 0754 = rwxr-xr--
```

**Verificação de acesso** (`usuario_tem_permissao`), igual ao Linux:
1. Se o usuário é o **root** → permitido.
2. Escolhe **uma** classe: dono (se o id bate), senão grupo (se o grupo bate), senão outros.
3. Pega os 3 bits da classe: `(permissoes >> posicao) & 7`.
4. Testa o bit pedido: `trio & PERMISSAO_DE_LEITURA` (ou escrita, ou execução).

**Onde cada permissão é verificada** (todas as tentativas são verificadas, simulando o **bloqueio obrigatório**: se não pode, a operação é negada com mensagem):

| Operação | Permissão exigida | Em quê |
|----------|-------------------|--------|
| `cat`, `cp` (origem) | `r` | no arquivo |
| `echo >`, `echo >>`, `touch` (existente), `cp` (destino existente) | `w` | no arquivo |
| `./arquivo` | `x` | no arquivo |
| criar (`touch`, `echo`, `mkdir`, `cp`), `rm`, `rmdir`, `mv` | `w` | **na pasta** |
| `ls` | `r` | na pasta |
| `cd` | `x` | na pasta |
| `chmod` | ser o **dono** ou root | — |

Um detalhe fiel ao Linux: **apagar depende da permissão da pasta, e não do arquivo**. Apagar significa tirar o nome da lista da pasta, ou seja, escrever na pasta.

### 5.6 Simulação da alocação de blocos

A teoria apresenta três métodos para guardar um arquivo em blocos:

| Método | Como funciona | Vantagem | Desvantagem |
|--------|---------------|----------|-------------|
| **Contígua** | todos os blocos em sequência (ex.: 10, 11, 12) | leitura rápida, acesso direto fácil | **fragmentação externa**: pode faltar um "pedaço seguido" mesmo havendo espaço livre total; difícil aumentar o arquivo |
| **Encadeada** | cada bloco guarda o número do próximo | sem fragmentação externa, arquivo cresce fácil | acesso direto lento (precisa seguir a corrente); perde espaço com os ponteiros; um bloco corrompido quebra o resto |
| **Indexada** | um **bloco de índice** lista todos os blocos do arquivo | sem fragmentação externa **e** acesso direto (o bloco N está em `indice[N]`) | gasta espaço com o índice; tamanho máximo limitado pelo tamanho do índice |

**Este projeto usa a alocação indexada.** O vetor `blocos_do_arquivo[16]` dentro do FCB é o bloco de índice.

```
  echo (150 bytes) > relatorio.txt

  conteúdo: [  bytes 0–63  ][ bytes 64–127 ][ bytes 128–149 ]
                  │                │                │
                  ▼                ▼                ▼
  disco:  ... [bloco 4] ... [bloco 9] [bloco 10] ...

  FCB de relatorio.txt:
     tamanho_em_bytes  = 150
     blocos_do_arquivo = {4, 9, 10}
     quantidade_de_blocos = 3
```

Os blocos **não precisam ser vizinhos**. Depois de apagar alguns arquivos, o disco fica com "buracos", e um arquivo novo aproveita esses buracos espalhados. Na alocação contígua isso não funcionaria. A demonstração mostra isso com `rm`, `echo`, `stat` e `disk`.

Decisões da implementação:
- **Tamanho do bloco:** 64 bytes. É pequeno de propósito, para que arquivos de texto curtos já ocupem vários blocos e a divisão apareça.
- **Escolha do bloco livre:** *first-fit*, o primeiro bloco livre a partir do 0.
- **Antes de gravar**, o código confere se cabe (`ERRO_ARQUIVO_GRANDE_DEMAIS` acima de 16 blocos, `ERRO_DISCO_CHEIO` sem blocos livres). Assim o conteúdo antigo não se perde se a gravação falhar.
- **Fragmentação interna:** o último bloco quase sempre sobra parcialmente vazio (ex.: o bloco 10 acima usa 22 de 64 bytes). Esse é o custo de dividir em blocos de tamanho fixo.
- `rm` e a sobrescrita **devolvem os blocos** ao mapa de livres, e o comando `disk` mostra o mapa (`#` ocupado, `.` livre).

---

## 6. Exemplos de uso e comparação com o Linux

### 6.1 Sessão de exemplo

```
root@minifs:/# mkdir home
root@minifs:/# cd home
root@minifs:/home# echo oi > a.txt
root@minifs:/home# echo 12345 > n.txt
root@minifs:/home# ls -l
-rw-r--r--     0     0      6  14/11 12:56  n.txt
-rw-r--r--     0     0      3  14/11 12:56  a.txt
root@minifs:/home# stat a.txt
  Nome:          a.txt
  Inode:         3
  Tipo:          texto
  Tamanho:       3 bytes
  Permissões:    0644 (-rw-r--r--)
  Dono (id):     0
  Grupo (id):    0
  Criado:        14/11/2026 12:56:16
  Modificado:    14/11/2026 12:56:16
  Acessado:      14/11/2026 12:56:16
  Blocos:        0  (1 de no máximo 16)
```

`a.txt` tem 3 bytes porque o `echo` acrescenta uma quebra de linha (`o`, `i`, `\n`), exatamente como no Linux.

### 6.2 Permissões em ação

```
root@minifs:/home# echo echo ola > s.sh
root@minifs:/home# su alice
alice@minifs:/home$ ./s.sh
s.sh: permissão negada (o arquivo não tem permissão de execução para você)
alice@minifs:/home$ su root
root@minifs:/home# chmod 755 s.sh
root@minifs:/home# su alice
alice@minifs:/home$ ./s.sh
[executando 's.sh' (simulado)]
echo ola
alice@minifs:/home$ rm s.sh
rm: 's.sh': permissão negada
```

### 6.3 Tabela de comparação

| Linux real | Simulador | Igual? |
|------------|-----------|--------|
| `mkdir docs` | `mkdir docs` | sim |
| `cd ..`, `cd /`, `cd` | `cd ..`, `cd /`, `cd` | sim (`cd` sem argumento vai para `/`, já que não há "home") |
| `ls -l` → `-rw-r--r-- 1 alice users 3 nov 14 12:56 a.txt` | `-rw-r--r--  1000  100  3  14/11 12:56  a.txt` | mesmas colunas, menos o número de links; dono e grupo aparecem como id |
| `echo oi > a.txt` (3 bytes) | `echo oi > a.txt` (3 bytes) | sim |
| `echo mais >> a.txt` | `echo mais >> a.txt` | sim |
| `cat`, `cp`, `mv`, `rm`, `rmdir` | mesmos | sim, inclusive `mv`/`cp` para dentro de uma pasta |
| `stat a.txt` → mostra `Inode`, `Size`, `Blocks`, `Access: (0644/-rw-r--r--)` | mostra inode, tamanho, **lista** dos blocos, `0644 (-rw-r--r--)` | no Linux "Blocks" é só a quantidade; aqui mostramos quais são |
| `chmod 754 a.txt` | `chmod 754 a.txt` | sim (só modo numérico) |
| `chmod` em arquivo de outro usuário → `Operation not permitted` | `operação não permitida (só o dono ou o root)` | sim |
| `rm` depende de `w` na pasta | idem | sim |
| `cd` depende de `x` na pasta | idem | sim |
| root ignora permissões | idem | sim |
| `./script.sh` precisa de `x` | idem (execução simulada) | sim |
| `su alice` pede senha | `su alice` troca direto | simplificado |

---

## 7. Tratamento de erros

Todo comando verifica os casos de erro antes de agir e mostra uma mensagem clara no formato `comando: 'alvo': motivo`:

- arquivo ou pasta não encontrado
- é um diretório / não é um diretório
- nome inválido (vazio, com `/`, `.` ou `..`, maior que 31 caracteres)
- já existe (no `mkdir`, `touch` com criação, `mv`)
- permissão negada (com a operação que foi negada)
- operação não permitida (`chmod` por quem não é dono)
- modo inválido no `chmod` (precisa de 3 dígitos de 0 a 7)
- pasta não está vazia (`rmdir`)
- não é possível remover a pasta atual ou uma acima dela
- não é possível mover uma pasta para dentro dela mesma
- arquivo grande demais (acima de 1024 bytes) / disco cheio
- argumentos faltando → mostra como se usa o comando
- comando não encontrado

---

## 8. Gerenciamento de memória

- Cada nó da árvore é criado com `calloc` em `criar_no` e liberado com `free` em `liberar_no_e_todos_os_filhos`.
- `rm` e `rmdir` liberam o nó **e** devolvem os blocos ao disco.
- Ao digitar `exit` (ou Ctrl+D), `encerrar_sistema_de_arquivos` percorre a árvore inteira, de baixo para cima, liberando tudo.
- O disco simulado é um vetor estático (não usa `malloc`), então não precisa ser liberado.
- Verificado **sem vazamentos** com `leaks --atExit` (macOS) depois de uma sessão com todos os comandos. No Linux: `make valgrind`.

---

## 9. Limitações conhecidas

- Tudo fica na memória: ao sair, os arquivos somem (o enunciado permite "em memória").
- Arquivo de no máximo 1024 bytes (16 blocos × 64 bytes), por causa do índice de tamanho fixo.
- `chmod` só no modo numérico (`754`), sem `u+x`.
- `rm -r` não existe; pastas são apagadas vazias com `rmdir`.
- Não há links (`ln`), um dos bônus citados no enunciado.
- O `ls` mostra o item mais novo primeiro, porque cada filho novo entra no começo da lista.
