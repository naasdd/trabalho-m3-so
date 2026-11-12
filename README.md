# Mini-Sistema de Arquivos em Memória (C)

Trabalho M3 de Sistemas Operacionais (UNIVALI). Enunciado: [docs/enunciado.pdf](docs/enunciado.pdf)

> 🚧 Em desenvolvimento. Este README vai virar a documentação completa exigida (seção 5.5 do enunciado).

## O que deve ser feito (resumo)

Um **shell interativo em C** que simula um sistema de arquivos Unix-like **inteiramente em memória**, sem tocar no FS real.

| # | Requisito | Comandos / estruturas |
|---|-----------|-----------------------|
| 3.1 | Diretórios em **árvore** | `mkdir`, `cd`, `ls`, `pwd` |
| 3.2 | **FCB/inode** por arquivo (nome, tamanho, tipo, datas `time_t`, id, permissões) | `touch`, `echo >`, `cat`, `cp`, `mv`, `rm`, `stat` |
| 3.3 | Permissões **RWX** owner/group/other via **bitmask** + usuário atual | `chmod 754`, `ls -l`, `su`/`whoami`, "Permissão negada" |
| 3.4 | **Disco simulado** (`char[]` em blocos) + alocação (indexada/contígua) | mostrar blocos de cada arquivo, mapa do disco |

Também: tratamento de erros claro, código comentado/modular, **sem memory leak** (valgrind), README teórico, vídeo no YouTube.

## Compilar e executar

```bash
make
./fs
```
