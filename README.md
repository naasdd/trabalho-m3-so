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
./minifs
```

`make valgrind` roda com verificação de memory leak (Linux).

## Estrutura do código

Todos os nomes (arquivos, funções, variáveis) estão em português e descrevem o que fazem.
Os termos da matéria (FCB, inode) foram mantidos. Os comandos do terminal seguem os do Linux.

| Arquivo | O que tem |
|---------|-----------|
| `src/sistema_arquivos.h` | Constantes, `TipoArquivo`, `struct FCB` (inode), `struct NoArvore`, `struct Usuario` e a lista de todas as funções |
| `src/arvore.c` | Montar, percorrer e liberar a árvore; transformar caminho em nó (`encontrar_pelo_caminho`) |
| `src/comandos_diretorio.c` | `mkdir`, `cd`, `ls [-l]`, `pwd` (objetivo 3.1) |
| `src/comandos_arquivo.c` | `touch`, `echo >`, `cat`, `cp`, `mv`, `rm`, `stat` (objetivo 3.2) |
| `src/permissoes.c` | `usuario_tem_permissao`, texto `drwxr-xr-x`, `chmod` (objetivo 3.3) |
| `src/disco.c` | Disco simulado em blocos, reservar/liberar blocos, `disk` (objetivo 3.4) |
| `src/terminal.c` | Lê o que você digita e chama o comando certo; `su` e `whoami` |
| `src/main.c` | Inicia, roda o terminal, libera a memória |

## Progresso

- [x] 3.1 Árvore de diretórios: `mkdir`, `cd`, `ls [-l]`, `pwd`
- [x] Verificação de permissões (`usuario_tem_permissao`) e usuários (`su`, `whoami`)
- [ ] 3.2 `touch`, `echo >`, `cat`, `cp`, `mv`, `rm`, `stat`
- [ ] 3.3 `chmod` numérico
- [ ] 3.4 Escrita/leitura em blocos (`gravar_conteudo_no_disco` / `ler_conteudo_do_disco`)
- [ ] README teórico completo + exemplos comparando com Linux
- [ ] Vídeo no YouTube
