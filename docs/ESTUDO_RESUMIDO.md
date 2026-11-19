# Guia relâmpago: Mini-Sistema de Arquivos em C

Leitura de menos de 10 minutos. O guia completo está em `docs/ESTUDO.md`.

---

## 1. O que é o trabalho (diga isso primeiro)

É um **terminal parecido com o do Linux**, feito em C. Ele cria pastas e arquivos, escreve, lê, copia, move, apaga e controla permissões. **Nada vai para o disco de verdade**: tudo fica em structs na memória, e o conteúdo dos arquivos vai para um **disco simulado** dividido em blocos.

Rodar:

```bash
make          # compila
./minifs      # abre o terminal
./minifs < demo/roteiro_apresentacao.txt   # demo pronta
```

---

## 2. Os 4 objetivos e onde estão

| Objetivo | O que é | Arquivo |
|---|---|---|
| 3.1 Diretórios | `mkdir`, `cd`, `ls`, `pwd`, `rmdir` | `arvore.c`, `comandos_diretorio.c` |
| 3.2 Arquivos e FCB | `touch`, `echo`, `cat`, `cp`, `mv`, `rm`, `stat` | `comandos_arquivo.c` |
| 3.3 Permissões | `chmod`, `su`, `./arquivo` | `permissoes.c` |
| 3.4 Blocos | disco simulado, `disk` | `disco.c` |

Os tipos (`FCB`, `NoArvore`, `Usuario`) e as constantes ficam em `sistema_arquivos.h`. O `terminal.c` lê o comando digitado e chama a função certa.

---

## 3. FCB = o "inode"

Cada arquivo e cada pasta tem um `struct FCB` com os **atributos** da teoria:

- **nome**, **número do inode** (ID único, vem de um contador)
- **tipo** (`enum`: diretório, texto, numérico, binário, programa)
- **tamanho**, as **3 datas** (`time_t`: criação, modificação, acesso)
- **permissões** (bitmask, ex.: `0754`), **dono** e **grupo**
- **lista de blocos** (`blocos_do_arquivo[16]`)

Duas frases para decorar:

1. **O FCB não guarda o conteúdo**, só *onde* ele está (os números dos blocos).
2. **Nome não é identidade**: a identidade é o inode. Por isso o `mv` renomeia sem trocar o inode, e o `cp` cria um inode novo.

O comando `stat` mostra o FCB inteiro.

---

## 4. A árvore de diretórios: array de structs

O enunciado deixava escolher: árvore binária, lista encadeada ou **array de structs**. **O grupo escolheu array de structs.**

- Todos os arquivos e pastas ficam num **vetor de 128 posições**: a **tabela de nós**.
- Cada posição tem `em_uso`, o `FCB` e o **`indice_do_pai`**.
- A **raiz** fica no índice 0 e é pai dela mesma (por isso `cd ..` em `/` fica em `/`).
- **Filhos de uma pasta** = todos os nós cujo `indice_do_pai` é o índice dela.

```
 índice:  0     1      2       3
 nome:    /     home   alice   bob
 pai:     0     0      1       1        →   / ── home ── alice
                                                     └── bob
```

| Operação | Como funciona |
|---|---|
| Criar | pega a primeira posição livre |
| Apagar | devolve os blocos e zera a posição |
| Mover (`mv`) | só troca o `indice_do_pai` |
| `pwd` | sobe pelos pais até o índice 0 |

**Por que é bom:** é simples, não tem ponteiro para religar, usa 1 `calloc` e 1 `free`, e é parecido com a tabela de inodes do Linux.
**Ponto fraco:** o limite fixo de 128 itens, e a busca percorre o vetor todo (com 128 posições isso é instantâneo).

**Vantagens da árvore (da teoria):** eficiência (a busca desce pelo caminho), nomeação (o mesmo nome pode existir em pastas diferentes) e agrupamento (arquivos relacionados ficam juntos).

---

## 5. As 6 operações da teoria

| Teoria | Comando |
|---|---|
| Criar | `touch`, `echo >` |
| Escrever | `echo >`, `echo >>` |
| Ler | `cat` |
| Reposicionar | `echo >>` (escreve no fim) |
| Excluir | `rm` |
| Truncar | `echo >` em arquivo que já existe |

---

## 6. Permissões RWX com bitmask

São 9 bits: **dono / grupo / outros**, cada um com **r=4, w=2, x=1**.

```
chmod 754  →  7 = rwx (dono)   5 = r-x (grupo)   4 = r-- (outros)
```

No código: `(7 << 6) | (5 << 3) | 4` = `0754`.

**Como o acesso é verificado** (`usuario_tem_permissao`):

1. É **root**? Pode tudo.
2. Escolhe **uma** classe: dono → grupo → outros.
3. Pega os 3 bits: `(permissoes >> posicao) & 7`.
4. Testa o bit pedido com `&`.

**Usuários de teste:** `root`, `alice`, `bob` (mesmo grupo da alice) e `guest` (outro grupo).

**Pegadinha boa para citar:** **apagar** um arquivo depende de `w` **na pasta**, não no arquivo, igual ao Linux. Entrar numa pasta (`cd`) exige `x`, e listar (`ls`) exige `r`.

Quando a permissão falta, a operação é negada com mensagem: isso é o **bloqueio obrigatório**.

---

## 7. Alocação de blocos: indexada

O disco simulado tem **256 blocos de 64 bytes** (16 KB) e um mapa de blocos livres.

| Método | Ideia | Problema |
|---|---|---|
| Contígua | blocos em sequência | fragmentação externa |
| Encadeada | cada bloco aponta pro próximo | acesso direto lento |
| **Indexada (a nossa)** | um índice lista os blocos | o índice ocupa espaço / limita o tamanho |

- O vetor `blocos_do_arquivo[16]` dentro do FCB **é o índice**.
- O texto é cortado em pedaços de 64 bytes, e cada pedaço vai pro **primeiro bloco livre** (*first-fit*).
- Os blocos **não precisam ser vizinhos** (ex.: `[0, 5, 6]`), e um arquivo novo aproveita os "buracos" deixados por um `rm`.
- **Fragmentação interna:** o último bloco quase sempre sobra meio vazio.
- Arquivo máximo: **16 × 64 = 1024 bytes**.

O comando `disk` mostra o mapa (`#` ocupado, `.` livre).

---

## 8. Memória e erros

- **Memória:** a tabela de nós é criada com **1 `calloc`** e liberada com **1 `free`** no `exit`. O disco é um vetor estático. Foi testado com **0 vazamentos** (`leaks` no macOS, `make valgrind` no Linux).
- **Erros tratados:** não encontrado, é/não é diretório, nome inválido, já existe, permissão negada, `chmod` inválido, pasta não vazia, mover pasta para dentro dela mesma, disco cheio, arquivo grande demais, limite de 128 itens.

---

## 9. Perguntas prováveis (respostas curtas)

- **Onde está a árvore se é um array?** No `indice_do_pai`: cada nó sabe onde o pai está, e isso forma a árvore.
- **Qual a diferença entre `cp` e `mv`?** O `cp` cria um inode novo com blocos novos. O `mv` mantém o mesmo inode e só troca o pai ou o nome.
- **Por que indexada?** Não tem fragmentação externa e o acesso é direto (o bloco N está em `blocos[N]`).
- **Por que o root passa em tudo?** Igual ao Linux: o root ignora as permissões.
- **Por que `echo oi` dá 3 bytes?** O `o`, o `i` e a quebra de linha `\n`, igual ao Linux.
- **O que acontece com 129 arquivos?** Aparece "limite de 128 arquivos e pastas atingido".
- **Limitações?** Tudo some ao sair, arquivo máximo de 1024 bytes, `chmod` só numérico, sem links (`ln`).

---

## 10. Demo ao vivo (2 minutos)

```
mkdir home
cd home
echo ola professor > notas.txt
cat notas.txt
ls -l
stat notas.txt
disk
su alice
echo tentando > notas.txt      ← permissão negada
su root
chmod 666 notas.txt
su alice
echo agora foi > notas.txt     ← funciona
cat notas.txt
exit
```
