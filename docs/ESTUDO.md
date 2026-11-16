# Mini Sistema de Arquivos em C — Guia de Estudo

Trabalho M3 de Sistemas Operacionais (UNIVALI), apresentado em sala.

- **Parte 1 — Entendendo o trabalho (≈ 20 min).** Sem código. O que o programa faz, por que faz e qual conceito da matéria cada parte demonstra. No fim estão o roteiro da apresentação e as perguntas prováveis do professor.
- **Parte 2 — Estudando o código (≈ 20 min).** Os trechos importantes de cada arquivo: o que fazem, como fazem e por que foram escritos assim.

Leia a Parte 1 inteira antes de abrir o código. Com a ideia clara, o código fica fácil de ler.

---

# PARTE 1 — ENTENDENDO O TRABALHO

## 1. O trabalho em uma frase

Fizemos, em C, **um terminal que imita o terminal do Linux**. Nele dá para criar pastas e arquivos, escrever, ler, copiar, mover, apagar e controlar quem pode fazer o quê. A diferença é que **nada é salvo no disco de verdade**: tudo existe só na memória enquanto o programa está aberto.

É um "Linux de brinquedo". Por fora, você digita `mkdir`, `ls -l`, `cat` e `chmod 754`, como no Linux. Por dentro, construímos as peças que um sistema operacional de verdade tem: **a ficha de cada arquivo, a árvore de pastas, a checagem de permissões e o disco dividido em blocos**.

O professor não quer ver se sabemos **usar** os comandos do Linux. Ele quer ver se entendemos **como eles funcionam por dentro**. Cada peça do programa existe para demonstrar um conceito da matéria.

## 2. O problema que um sistema de arquivos resolve

O disco, sozinho, não sabe o que é um "arquivo". Ele só sabe "guarde estes bytes na posição 4.000.123" e "me devolva o que está na posição 4.000.123". Ninguém consegue trabalhar decorando posições.

O **sistema de arquivos** é a camada do sistema operacional que resolve isso. Ele oferece:

1. **Nomes** em vez de posições: você pede `trabalho.txt`, e ele sabe onde estão os bytes.
2. **Organização** em pastas.
3. **Informações** sobre cada arquivo: tamanho, datas, dono.
4. **Proteção**: quem pode ler, alterar ou executar.
5. **Gerenciamento do espaço**: em que pedaço do disco cada arquivo fica, e o reaproveitamento do espaço quando algo é apagado.

O enunciado chama isso de **"mapeamento lógico para físico"**. O **lógico** é o nome que você vê (`/home/alice/notas.txt`), e o **físico** é onde os bytes realmente estão (por exemplo, os blocos 4, 9 e 10 do disco). Nosso trabalho implementa essas cinco coisas em versão simplificada.

## 3. O FCB — a ficha de cada arquivo (o "inode")

Para o sistema operacional, um arquivo é **um conjunto de bytes mais uma ficha de informações**. Essas informações são os **atributos**:

| Atributo | O que é | Exemplo |
|---|---|---|
| **Nome** | como o humano chama o arquivo | `notas.txt` |
| **Identificador** | número único usado internamente | inode 5 |
| **Tipo** | que tipo de conteúdo é | texto, numérico, binário, programa |
| **Localização** | onde os bytes estão no disco | blocos 4, 9, 10 |
| **Tamanho** | quantos bytes tem | 150 bytes |
| **Proteção** | quem pode fazer o quê | `rwxr-xr--`, dono alice |
| **Datas** | criação, modificação, último acesso | 14/11/2026 20:15 |

A ficha que guarda tudo isso se chama **FCB** (*File Control Block*). No Linux, ela se chama **inode**. No nosso programa, **cada atributo é um campo de uma struct em C**.

Pense numa biblioteca antiga com fichário. Cada livro tem uma ficha com título, autor e em qual estante ele está. **A ficha não é o livro**, ela só diz onde o livro está. O FCB funciona igual: guarda **todas as informações** do arquivo, mas **não guarda o conteúdo**, só a lista de **onde** o conteúdo está (quais blocos do disco).

Por que separar? Porque muitas operações nem precisam mexer no conteúdo:
- o `ls -l` só lê as fichas;
- renomear é trocar o nome na ficha;
- mudar permissões é trocar um número na ficha.

**Nome × identificador.** O nome é um rótulo para humanos e pode mudar. O **número do inode** é a identidade do arquivo e **nunca muda**. Ele vem de um contador que só cresce (1, 2, 3...), então nunca se repete, nem quando um arquivo é apagado.

O comando `stat` mostra a ficha inteira:

```
  Nome:          notas.txt
  Inode:         5
  Tipo:          texto
  Tamanho:       28 bytes
  Permissões:    0644 (-rw-r--r--)
  Dono (id):     1000
  Grupo (id):    100
  Criado:        14/11/2026 12:58:14
  Modificado:    14/11/2026 12:58:14
  Acessado:      14/11/2026 12:58:14
  Blocos:        0  (1 de no máximo 16)
```

A última linha é a **localização**: o conteúdo está no bloco 0 do disco simulado.

**Pastas também têm FCB**, do tipo "diretório". Elas têm nome, inode, dono, permissões e datas, mas não têm conteúdo em blocos. O "conteúdo" de uma pasta é a lista do que está dentro dela.

## 4. Pastas organizadas em árvore

### 4.1 Por que árvore?

A matéria mostra uma evolução:

1. **Nível único:** tudo de todo mundo numa pasta só. Ninguém pode repetir nome, e achar algo fica impossível.
2. **Dois níveis:** uma pasta por usuário. A alice e o bob podem ter um `notas.txt` cada, mas dentro da pasta de cada um continua tudo misturado.
3. **Árvore:** pastas dentro de pastas, em quantos níveis quiser. É o que o Linux usa, e é o que implementamos.

As vantagens da árvore (o enunciado pede para discutir):
- **Eficiência:** para achar `/home/alice/notas.txt`, o sistema entra em `home`, depois em `alice` e procura `notas.txt`. Só olha o caminho, e não o computador inteiro.
- **Nomeação:** o nome só precisa ser único **dentro da mesma pasta**. Podem existir `/home/alice/notas.txt` e `/home/bob/notas.txt` ao mesmo tempo.
- **Agrupamento:** coisas relacionadas ficam juntas.

### 4.2 Como a árvore fica na memória

Uma pasta pode ter **qualquer quantidade de filhos**: 0, 2 ou 500. Um vetor fixo de filhos desperdiçaria espaço numa pasta vazia e não caberia numa pasta grande.

Usamos a técnica **"primeiro filho / próximo irmão"**. Cada item da árvore (um **nó**) guarda só três "setas" (ponteiros):
- **pai**: a pasta onde ele está;
- **primeiro filho**: o primeiro item dentro dele (se for pasta);
- **próximo irmão**: o próximo item da mesma pasta.

```
Árvore:                 Na memória:

      /                 /
      |                 └─ primeiro filho ─▶ home
    home                                     └─ primeiro filho ─▶ alice ── próximo irmão ──▶ bob
   /    \                                                          └─ primeiro filho ─▶ notas.txt
alice    bob
  |
notas.txt
```

Os filhos de uma pasta viram uma **lista encadeada**. Para listar uma pasta (`ls`), é só começar no primeiro filho e seguir as setas de "próximo irmão" até acabar. Não tem limite de itens, não desperdiça espaço e cada nó tem tamanho fixo.

Um item novo entra **no começo** da lista, porque é o jeito mais rápido (não precisa andar até o fim). Por isso o nosso `ls` mostra primeiro o item mais recente.

### 4.3 Caminhos, `.` e `..`

- **Absoluto:** começa com `/` e é contado a partir da raiz (`/home/alice/notas.txt`). Funciona de qualquer lugar.
- **Relativo:** é contado a partir da pasta atual. Estando em `/home`, `alice/notas.txt` é o mesmo arquivo.
- **`.`** é a própria pasta, e **`..`** é a pasta de cima.
- **O pai da raiz é a própria raiz**, então `cd ..` em `/` continua em `/`, igual no Linux.

O programa sabe "onde você está" por uma variável chamada `diretorio_atual`. O `cd` só troca essa variável. O `pwd` sobe de pai em pai até a raiz para montar o caminho.

### 4.4 O que o programa impede

- **Mover uma pasta para dentro dela mesma** (`mv /home /home/alice`). Isso criaria um laço, a estrutura deixaria de ser uma árvore e qualquer função que percorre a árvore entraria em loop infinito.
- **Apagar a pasta onde você está** (ou uma acima dela), porque você ficaria "flutuando" numa pasta que não existe mais.
- **`rmdir` em pasta com coisas dentro**: igual ao Linux, só apaga pasta vazia.

## 5. As operações com arquivos

### 5.1 As seis operações da teoria

| Operação | Comando | O que acontece |
|---|---|---|
| **Criar** | `touch a.txt` ou `echo oi > a.txt` | nasce uma ficha nova (FCB) com inode novo, ligada na pasta |
| **Escrever** | `echo texto > a.txt` | o texto é cortado em pedaços e guardado em blocos |
| **Ler** | `cat a.txt` | os pedaços são buscados nos blocos, na ordem, e juntados |
| **Reposicionar** | `echo texto >> a.txt` | a escrita começa no **fim** do arquivo |
| **Excluir** | `rm a.txt` | a ficha sai da pasta, os blocos são liberados e a memória é devolvida |
| **Truncar** | `echo novo > a.txt` (arquivo existente) | o conteúdo antigo é jogado fora e fica só o novo |

> **Sinceridade sobre o "reposicionar":** no Linux, reposicionar é o `lseek`, que move um "cursor" dentro do arquivo aberto. Nosso programa não tem "abrir arquivo" com cursor: cada comando lê ou grava o arquivo inteiro. O `>>` é o mais perto disso, porque escreve a partir do fim. Se o professor perguntar, responda exatamente isso. Admitir a limitação e explicar o motivo vale mais do que forçar.

Também temos `cp` (ler + criar + escrever), `mv` (mover/renomear), `stat` (ler a ficha) e `./arquivo` (executar, para testar a permissão `x`). Cada operação atualiza as datas: ler atualiza o último acesso, e escrever atualiza a modificação.

### 5.2 Por que `echo oi` vira 3 bytes?

No Linux, `echo oi > a.txt` cria um arquivo com **3 bytes**: `o`, `i` e uma quebra de linha invisível (`\n`), que o `echo` sempre coloca no fim. Fizemos igual.

### 5.3 A diferença mais importante: `cp` × `mv`

- **`cp a.txt b.txt`** cria um arquivo **novo**: o `b.txt` ganha **outro inode** e **outros blocos**, com uma cópia do conteúdo. São dois arquivos independentes.
- **`mv a.txt b.txt`** **não copia nada**. É o **mesmo inode e os mesmos blocos**. Só muda o nome na ficha e, talvez, a pasta onde o nó está pendurado.

Na demo, o `stat` prova isso: depois do `cp`, a cópia tem inode **6** (o original era 5) e outro bloco. Depois do `mv`, continua inode **6** e o mesmo bloco. É por isso que, no Linux, mover um arquivo enorme no mesmo disco é instantâneo, e copiar demora.

### 5.4 Tipos de arquivo

O enunciado pede os tipos numérico, caractere (texto), binário e programa. O tipo é decidido assim: nome terminado em `.sh` → **programa**; em `.bin` → **binário**; conteúdo só com números → **numérico**; qualquer outra coisa → **texto**. As pastas têm o tipo **diretório**.

## 6. Permissões: quem pode fazer o quê

É a parte mais "matemática", e a que o professor mais deve perguntar.

### 6.1 Ações e classes

- **r** (*read*): ler o conteúdo. **w** (*write*): alterar. **x** (*execute*): rodar como programa.
- Cada arquivo tem um **dono** e um **grupo**. Em relação a um arquivo, qualquer usuário cai em **uma** de três classes: **dono** (*owner*), **grupo** (*group*) ou **outros** (*others/public*).
- Cada classe tem o seu próprio rwx. São **9 permissões**: 3 ações × 3 classes.

No `ls -l`, `-rwxr-xr--` quer dizer: o dono pode tudo, o grupo lê e executa, e os outros só leem. O primeiro caractere é `d` para pasta e `-` para arquivo.

### 6.2 De onde vem o 754?

Cada ação vale um número: **r = 4, w = 2, x = 1**. Somando as ações permitidas, cada classe vira um dígito de 0 a 7:

| Dígito | Conta | Permissão |
|---|---|---|
| 7 | 4+2+1 | `rwx` |
| 6 | 4+2 | `rw-` |
| 5 | 4+1 | `r-x` |
| 4 | 4 | `r--` |
| 0 | nada | `---` |

**`chmod 754`** significa dono = 7 (`rwx`), grupo = 5 (`r-x`), outros = 4 (`r--`).

Por que 4, 2 e 1? São potências de 2: cada ação é **um bit**. Um trio de bits vai de 0 a 7, que é exatamente um dígito em **octal** (base 8). Por isso permissões são escritas em octal.

```
   7    5    4         (octal)
  111  101  100        (binário)
  rwx  r-x  r--        (letras)
```

No programa, as 9 permissões ficam guardadas num **único número inteiro**, usando os 9 bits mais baixos. É isso que o enunciado chama de **bitmask** (máscara de bits).

### 6.3 Como o programa decide

1. **É o root?** Pode tudo.
2. **Qual a classe da pessoa?** Se o id do usuário é o do dono → **dono**. Senão, se o grupo bate → **grupo**. Senão → **outros**.
3. **Pega só o trio dessa classe.**
4. **O bit da ação pedida está ligado?** Se estiver, permite. Se não, mostra "permissão negada".

Vale **só uma** classe. Se você é o dono, valem as permissões do dono, mesmo que as dos outros sejam mais generosas.

O enunciado pede que **toda** tentativa de ler, escrever ou executar seja verificada, e chama isso de **bloqueio obrigatório**. No programa, todo comando passa pela mesma função de verificação antes de agir.

### 6.4 Usuários de teste

| Usuário | id | grupo | Mostra... |
|---|---|---|---|
| root | 0 | 0 | quem ignora permissões |
| alice | 1000 | 100 | a **dona** dos arquivos na demo |
| bob | 1001 | 100 | o **grupo** (mesmo grupo da alice) |
| guest | 2000 | 200 | os **outros** |

O comando `su` troca de usuário. Na demo, a alice cria `script.sh` e dá `chmod 754`. A alice (dona, `rwx`) executa. O bob (grupo, `r-x`) executa, mas não altera. O guest (outros, `r--`) lê, mas não executa. O root faz tudo.

### 6.5 Permissões em pastas (o detalhe que impressiona)

Em pastas, as letras mudam de sentido: **r** = listar (`ls`), **w** = criar, apagar e renomear coisas dentro, **x** = entrar (`cd`).

A consequência: **para apagar um arquivo, o que importa é a permissão de escrita na PASTA, e não no arquivo.** Apagar é tirar o nome da lista da pasta, ou seja, escrever na pasta. Na demo, o guest não consegue apagar o `script.sh` porque não tem `w` na pasta da alice.

Mais duas regras do Linux que copiamos:
- **Só o dono (ou o root) pode dar `chmod`.** Senão qualquer um daria `chmod 777` e a proteção não serviria para nada.
- Arquivos novos nascem com **644** (`rw-r--r--`) e pastas com **755** (`rwxr-xr-x`). Por isso o script nasce **sem** permissão de execução e precisa de `chmod` antes de rodar, igual no Linux.

## 7. O disco simulado e os blocos

### 7.1 Por que blocos?

O disco de verdade é lido em **blocos** de tamanho fixo, e não byte a byte. No nosso trabalho, o "disco" é um vetor de bytes na memória dividido em **256 blocos de 64 bytes** (16 KB). Os blocos são pequenos de propósito: com 64 bytes, uma frase comum já ocupa 2 ou 3 blocos, e dá para **ver** a divisão.

Existe também um **mapa de blocos livres**, que diz se cada bloco está ocupado ou livre. O comando `disk` desenha esse mapa (`#` = ocupado, `.` = livre):

```
   0 | #######.........................
  32 | ................................
7 de 256 blocos ocupados (64 bytes por bloco)
```

### 7.2 Os três métodos de alocação

**1. Contígua.** O arquivo ocupa blocos **seguidos**, como uma família sentada lado a lado no cinema.
- Vantagem: leitura rápida, e achar o pedaço N é só somar.
- Problema: **fragmentação externa**. Com o tempo, o espaço livre fica picado em buracos. Pode haver 30 blocos livres no total e mesmo assim não caber um arquivo de 5, porque não existem 5 livres **seguidos**.

**2. Encadeada.** Cada bloco guarda um pedaço e o **número do próximo**, como uma caça ao tesouro.
- Vantagem: qualquer bloco livre serve, e o arquivo cresce fácil.
- Problema: para chegar ao pedaço 50, é preciso passar pelos 49 anteriores (acesso direto lento). Se um bloco estragar, perde-se tudo o que vem depois.

**3. Indexada.** Um **índice** lista todos os blocos do arquivo, na ordem, como o sumário de um livro.
- Vantagem: os blocos podem ficar espalhados (sem fragmentação externa) **e** o acesso é direto (o pedaço N está em `índice[N]`).
- Problema: o índice ocupa espaço, e o tamanho dele limita o tamanho do arquivo.

**Usamos a indexada.** O índice fica dentro do próprio FCB: é a lista `blocos_do_arquivo`, com 16 posições. Por isso o maior arquivo possível tem 16 × 64 = **1024 bytes**.

### 7.3 Gravando um arquivo, passo a passo

Exemplo: gravar 150 bytes.

1. **Quantos blocos?** 150 ÷ 64 = 2,3, arredondando para cima = **3**.
2. **Cabe?** Confere o limite de 16 por arquivo e se há 3 blocos livres. Se não couber, dá erro **antes** de mexer em qualquer coisa.
3. **Devolve os blocos antigos**, se o arquivo já tinha conteúdo.
4. **Para cada pedaço:** pega o primeiro bloco livre, copia o pedaço e anota o número do bloco no FCB.

```
  conteúdo:  [ bytes 0–63 ][ bytes 64–127 ][ bytes 128–149 ]
                   │              │                │
                   ▼              ▼                ▼
  disco:   ... [ bloco 4 ] ... [ bloco 9 ] [ bloco 10 ] ...

  FCB: tamanho = 150, blocos = {4, 9, 10}
```

Ler é o caminho inverso: percorre a lista de blocos do FCB e junta os pedaços. Escolher sempre o **primeiro** bloco livre se chama **first-fit**.

### 7.4 O momento da demo que prova a indexada

1. Um arquivo grande ocupa os blocos **2, 3 e 4**.
2. Apagamos um arquivo que estava no bloco **0**, o que abre um buraco no começo.
3. Um novo arquivo de 3 blocos fica em **0, 5 e 6**: usou o buraco **e** dois blocos lá no fim.

Esse arquivo está **espalhado** e funciona normalmente. Com alocação contígua, isso seria impossível. É a prova visual de que a indexada evita a fragmentação externa.

### 7.5 Fragmentação interna

O último bloco quase nunca fica cheio. Nos 150 bytes do exemplo, o 3º bloco usa só 22 dos 64 bytes. Esse espaço perdido **dentro** do bloco é a **fragmentação interna**, e acontece em qualquer método.

- **Interna:** sobra dentro do último bloco. Acontece sempre.
- **Externa:** espaço livre picado em buracos. É o problema da contígua, e a indexada resolve.

## 8. O caminho de um comando, do começo ao fim

Veja o que acontece com `echo oi > notas.txt`, digitado pela alice em `/home/alice`:

1. O terminal lê a linha e vê que começa com `echo`.
2. Separa as partes: texto = `oi`, arquivo = `notas.txt`, modo = substituir (um `>` só).
3. Procura o arquivo. Ele não existe, então vai ser criado.
4. Verifica a permissão: a alice pode escrever na pasta? Ela é a dona e a pasta é `755`, então sim.
5. Cria o FCB: inode novo, dona = alice, permissões `644`, datas = agora.
6. Pendura a ficha no começo da lista de filhos da pasta.
7. Monta o conteúdo: `oi` + `\n` = 3 bytes.
8. Grava: 3 bytes cabem em 1 bloco. Pega o primeiro livre (digamos o 0) e anota `blocos = {0}`.
9. Atualiza o tamanho, o tipo e a data de modificação.

E `cat notas.txt`: acha o arquivo → confere se a alice pode **ler** → lê o bloco 0 → mostra `oi` → atualiza a data de acesso.

## 9. Memória: o tal "memory leak"

Cada pasta e cada arquivo ocupam memória, pedida com **`calloc`** (primo do `malloc`). Em C, ninguém devolve essa memória automaticamente: se o programa pede e nunca devolve com **`free`**, a memória fica presa. Isso é o **vazamento de memória** (*memory leak*), e conta nota no critério de robustez.

- **`rm` e `rmdir`** devolvem a memória do item e liberam os blocos dele.
- **Ao sair**, o programa percorre a árvore **de baixo para cima** (filhos antes do pai) e devolve tudo. Se apagasse o pai primeiro, perderia as setas para os filhos e não teria mais como achá-los.
- Testamos com `leaks` (Mac) / `valgrind` (Linux), usando todos os comandos: **0 vazamentos**.

## 10. Roteiro da apresentação

Os comandos estão em `demo/roteiro_apresentacao.txt`. Compile com `make` e abra com `./minifs`. O ideal é **digitar ao vivo**, porque passa mais segurança. O plano B, se der nervoso ou faltar tempo, é `./minifs < demo/roteiro_apresentacao.txt`, que roda tudo como se alguém estivesse digitando.

**Parte 1 — Pastas (3.1).** `mkdir`, `cd`, `ls -l`, `pwd`.

> "As pastas são uma árvore. Cada item é um nó com três ponteiros: pai, primeiro filho e próximo irmão. Assim uma pasta pode ter quantos itens quiser, sem desperdiçar memória. O `cd` só muda a variável da pasta atual, e o `pwd` sobe de pai em pai até a raiz."

**Parte 2 — Arquivos e FCB (3.2).** `echo >`, `cat`, `echo >>`, `stat`, `cp`, `mv`.

> "Cada arquivo tem um FCB, o inode simulado, com nome, inode, tipo, tamanho, permissões, dono e as três datas. O `stat` mostra a ficha. Reparem: o `cp` gerou um inode novo, o 6, com outro bloco. O `mv` manteve o inode 6 e o mesmo bloco, porque mover não copia dados, só muda o nó de lugar."

**Parte 3 — Blocos (3.4).** `disk`, `echo` grande, `stat`, `rm`, `echo` grande, `stat`, `disk`.

> "O disco é um vetor de 256 blocos de 64 bytes. Esse texto de 131 bytes precisou de 3 blocos: 2, 3 e 4. Agora apago o arquivo do bloco 0 e crio outro de 3 blocos: ele ficou em 0, 5 e 6, espalhados. Isso é a alocação indexada: o FCB guarda a lista de blocos, então eles não precisam ser seguidos, e não há fragmentação externa."

**Parte 4 — Permissões (3.3).** `./script.sh`, `chmod 754`, `ls -l`, `su bob`, `su guest`.

> "As permissões ficam num inteiro, como bitmask de 9 bits. O `chmod 754` monta esse número: 7 para o dono, 5 para o grupo, 4 para os outros. Para verificar, o programa escolhe a classe do usuário, isola os 3 bits dela e testa o bit da operação. O bob é do grupo, então vale r-x: executa, mas não altera. O guest é dos outros, então vale r--: lê, mas não executa. E não apaga, porque apagar depende do `w` na pasta."

**Parte 5 — Erros.** Comandos errados de propósito.

> "Todo comando confere os erros antes de agir: não encontrado, nome inválido, permissão negada, mover pasta para dentro dela mesma, modo de chmod inválido..."

**Para fechar:** "Tudo é liberado ao sair, e testamos com ferramenta de vazamento: zero vazamentos."

**Dicas:** se o professor pedir outro comando, tudo funciona com caminhos e tem mensagem de erro. Se travar, `help` lista os comandos. Para recomeçar do zero, `exit` e `./minifs` de novo.

## 11. Perguntas que o professor provavelmente vai fazer

**"O que é o FCB e o que ele guarda?"**\
A ficha de controle do arquivo, o equivalente ao inode. Guarda nome, inode, tipo, tamanho, permissões, dono, grupo, as três datas e a lista dos blocos. Não guarda o conteúdo.

**"Por que essa estrutura de árvore?"**\
Uma pasta pode ter qualquer número de filhos. Com "primeiro filho / próximo irmão", cada nó tem só 3 ponteiros, os filhos formam uma lista encadeada, não há limite nem desperdício.

**"Quais as vantagens do diretório em árvore?"**\
Eficiência (só percorre o caminho), nomeação (nome único só dentro da pasta) e agrupamento.

**"Como funciona o `chmod 754` por dentro?"**\
Cada dígito é um trio de bits (7 = 111, 5 = 101, 4 = 100). O do dono é empurrado 6 bits para a esquerda, o do grupo 3, e os três são juntados com OU bit a bit, o que dá 0754.

**"Como o programa sabe se o usuário pode ler?"**\
Se for root, pode. Senão, escolhe a classe, desloca as permissões para trazer o trio dela para o fim, aplica `& 7` para ficar só com esses 3 bits e testa o bit de leitura (4) com `&`.

**"Por que o guest não apagou o arquivo, se podia ler?"**\
Apagar depende do `w` na **pasta**, e não no arquivo. Ele não tem `w` na pasta da alice.

**"Qual alocação vocês usaram e por quê?"**\
Indexada: o FCB tem um vetor com os números dos blocos. Evita a fragmentação externa da contígua e tem acesso direto, ao contrário da encadeada.

**"Fragmentação interna × externa?"**\
Interna é a sobra dentro do último bloco, e acontece sempre. Externa é o espaço livre picado em buracos, que é o problema da contígua.

**"Qual o tamanho máximo de um arquivo?"**\
1024 bytes: 16 blocos (o tamanho do índice) × 64 bytes.

**"E se o disco encher?"**\
A gravação confere antes de começar. Se não couber, mostra "disco cheio" e não mexe em nada, então o conteúdo antigo continua intacto.

**"`cp` × `mv` por dentro?"**\
O `cp` cria FCB novo (inode novo) e blocos novos. O `mv` só tira o nó de uma pasta e põe em outra: mesmo inode, mesmos blocos.

**"Como evitam memory leak?"**\
`calloc` para criar e `free` para liberar. O `rm` libera o nó e os blocos. Ao sair, uma função recursiva libera tudo, começando pelos filhos. Testado: 0 vazamentos.

**"Onde está o 'usuário atual'?"**\
Numa variável global, `usuario_atual`, como o enunciado sugere. O `su` troca o valor dela.

---

# PARTE 2 — ESTUDANDO O CÓDIGO

Não é preciso decorar linhas. O importante é conseguir apontar para um trecho e explicar **o que ele faz e por que está ali**. Alguns trechos abaixo foram encurtados com `...`.

## 12. Mapa dos arquivos

| Arquivo | O que tem |
|---|---|
| `sistema_arquivos.h` | a "planta baixa": structs, constantes e a lista de funções |
| `main.c` | liga tudo: inicia, roda o terminal, libera a memória |
| `arvore.c` | criar, procurar e apagar nós da árvore |
| `comandos_diretorio.c` | `mkdir`, `cd`, `ls`, `pwd`, `rmdir` — **objetivo 3.1** |
| `comandos_arquivo.c` | `touch`, `echo`, `cat`, `cp`, `mv`, `rm`, `stat`, `./` — **objetivo 3.2** |
| `permissoes.c` | verificação rwx, texto `rwxr-xr-x`, `chmod` — **objetivo 3.3** |
| `disco.c` | disco em blocos, gravar e ler — **objetivo 3.4** |
| `terminal.c` | lê o que você digita e chama o comando certo |

Todo `.c` começa com `#include "sistema_arquivos.h"`. É assim que um arquivo "conhece" as structs e as funções dos outros. O `main` tem três linhas, de propósito: **montar, usar, desmontar**.

```c
iniciar_sistema_de_arquivos();
rodar_terminal();
encerrar_sistema_de_arquivos();
```

O `Makefile` compila com `gcc -std=c11 -Wall -Wextra -pedantic -g`. Os `-W...` ligam todos os avisos, e o código compila **sem nenhum aviso**. O `-g` guarda informações para o `valgrind` apontar a linha exata de um problema.

## 13. `sistema_arquivos.h` — a planta baixa

### 13.1 Constantes

```c
#define TAMANHO_DO_BLOCO              64
#define QUANTIDADE_DE_BLOCOS_NO_DISCO 256
#define MAXIMO_DE_BLOCOS_POR_ARQUIVO  16
#define TAMANHO_MAXIMO_DO_ARQUIVO     (MAXIMO_DE_BLOCOS_POR_ARQUIVO * TAMANHO_DO_BLOCO)

#define PERMISSAO_DE_LEITURA   4
#define PERMISSAO_DE_ESCRITA   2
#define PERMISSAO_DE_EXECUCAO  1

#define POSICAO_DO_TRIO_DONO    6
#define POSICAO_DO_TRIO_GRUPO   3
#define POSICAO_DO_TRIO_OUTROS  0

#define PERMISSOES_PADRAO_DE_ARQUIVO   0644
#define PERMISSOES_PADRAO_DE_DIRETORIO 0755
```

- **Por que `#define`?** Para mudar o tamanho do bloco, basta mudar **uma linha**. Números soltos pelo código ("números mágicos") obrigariam a caçar cada `64`.
- **Os parênteses** em `TAMANHO_MAXIMO_DO_ARQUIVO` existem porque o `#define` é troca de texto. Sem eles, `x / TAMANHO_MAXIMO_DO_ARQUIVO` viraria `x / 16 * 64`, uma conta errada.
- **6, 3, 0** são quantos bits é preciso andar para chegar ao trio de cada classe. O dono está nos bits 8-7-6, o grupo nos 5-4-3 e os outros nos 2-1-0.
- **`0644` com zero na frente:** em C, número que começa com 0 é **octal**. Sem o zero, seria o decimal 644, e as permissões ficariam erradas.

### 13.2 Enum e structs

```c
typedef enum {
    TIPO_DIRETORIO, TIPO_TEXTO, TIPO_NUMERICO, TIPO_BINARIO, TIPO_PROGRAMA
} TipoArquivo;
```

Um `enum` dá nomes a números. `if (tipo == TIPO_DIRETORIO)` se explica sozinho, e `if (tipo == 0)` não. O `typedef` permite escrever só `TipoArquivo`. O critério de qualidade cita `typedef` e `enums`.

```c
typedef struct {
    int          numero_inode;
    char         nome[TAMANHO_MAXIMO_NOME];
    TipoArquivo  tipo;
    size_t       tamanho_em_bytes;

    time_t       data_de_criacao;
    time_t       data_de_modificacao;
    time_t       data_do_ultimo_acesso;

    int          permissoes;
    int          id_do_dono;
    int          id_do_grupo;

    int          blocos_do_arquivo[MAXIMO_DE_BLOCOS_POR_ARQUIVO];
    int          quantidade_de_blocos;
} FCB;
```

- **`char nome[32]`** fica **dentro** da struct, então não precisa de `malloc` nem de `free` separados. O nome tem no máximo 31 caracteres, porque o 32º é o `'\0'` que termina toda string em C.
- **`size_t`** é o tipo do C para tamanhos: nunca é negativo, e é o que `strlen` e `memcpy` usam.
- **`time_t`** guarda os segundos desde 1970, e o enunciado pede esse tipo. `time(NULL)` devolve o "agora".
- **`permissoes`** é o bitmask de 9 bits. **`id_do_dono`/`id_do_grupo`** guardam números, e não nomes, igual ao UID/GID do Linux.
- **`blocos_do_arquivo[16]` + `quantidade_de_blocos`** são o **índice** da alocação indexada. Só as primeiras `quantidade_de_blocos` posições valem, e sem o contador não daria para saber onde a lista acaba.

```c
typedef struct NoArvore {
    FCB              fcb;
    struct NoArvore *pai;
    struct NoArvore *primeiro_filho;
    struct NoArvore *proximo_irmao;
} NoArvore;
```

- **`FCB fcb`** sem asterisco: a ficha mora **dentro** do nó, então um `calloc` cria os dois e um `free` libera os dois.
- **Os ponteiros** guardam **endereços** de outros nós ("setas"). `NULL` significa "não aponta para ninguém".
- **`struct NoArvore` com nome antes do `{`:** a struct aponta para ela mesma, e dentro das chaves o apelido `NoArvore` ainda não existe.

### 13.3 Globais com `extern`

```c
extern NoArvore *raiz;
extern NoArvore *diretorio_atual;
extern Usuario   usuario_atual;
```

`extern` quer dizer "esta variável existe, mas foi criada em outro arquivo". Assim todos os `.c` usam **as mesmas** variáveis.

## 14. `arvore.c` — a estrutura da árvore

### 14.1 Iniciar

```c
iniciar_disco();
raiz = criar_no("/", TIPO_DIRETORIO);
raiz->pai = raiz;
diretorio_atual = raiz;
```

**`raiz->pai = raiz`**: fazendo a raiz apontar para si mesma, o `cd ..` na raiz funciona sem `if` especial. O `->` acessa um campo **através de um ponteiro**.

### 14.2 `criar_no`

```c
NoArvore *novo = calloc(1, sizeof(NoArvore));
if (novo == NULL) {
    printf("Erro fatal: acabou a memória do computador.\n");
    exit(EXIT_FAILURE);
}
novo->fcb.numero_inode = proximo_numero_inode;
proximo_numero_inode = proximo_numero_inode + 1;
strncpy(novo->fcb.nome, nome, TAMANHO_MAXIMO_NOME - 1);
...
novo->fcb.id_do_dono  = usuario_atual.id_do_usuario;
novo->fcb.id_do_grupo = usuario_atual.id_do_grupo;
```

- **`calloc` e não `malloc`:** o `calloc` **zera** a memória, então todos os ponteiros já nascem `NULL`. Com `malloc`, eles teriam lixo, e seguir um ponteiro com lixo trava o programa (*segmentation fault*).
- **Checar `NULL`:** se faltar memória, o `calloc` devolve `NULL`, e usar esse ponteiro travaria tudo.
- **`proximo_numero_inode`** é uma variável `static` que só cresce, então o inode nunca se repete. `static` fora de função = só `arvore.c` enxerga.
- **`strncpy` com `- 1`:** copia no máximo 31 caracteres, para nunca passar do vetor (*buffer overflow*). A última posição já é `'\0'` porque o `calloc` zerou tudo.
- **Quem cria vira o dono**, e as permissões padrão são 755 para pasta e 644 para arquivo.

### 14.3 `adicionar_filho`

```c
filho->pai            = pasta;
filho->proximo_irmao  = pasta->primeiro_filho;
pasta->primeiro_filho = filho;
```

```
antes:    pasta ─▶ A ─▶ B
depois:   pasta ─▶ filho ─▶ A ─▶ B
```

**A ordem das duas últimas linhas importa.** Se a pasta apontasse para o filho primeiro, a seta para `A` se perderia, e todos os filhos antigos sumiriam. Inserir no começo é **O(1)**: são sempre 3 linhas, não importa o tamanho da pasta.

### 14.4 `remover_filho_da_pasta`

```c
if (pasta->primeiro_filho == filho) {
    pasta->primeiro_filho = filho->proximo_irmao;
} else {
    NoArvore *anterior = pasta->primeiro_filho;
    while (anterior != NULL && anterior->proximo_irmao != filho) {
        anterior = anterior->proximo_irmao;
    }
    if (anterior != NULL) {
        anterior->proximo_irmao = filho->proximo_irmao;
    }
}
```

Tirar da lista é "fazer o anterior pular o filho" (`A ─▶ filho ─▶ C` vira `A ─▶ C`). Se o filho é o primeiro, a pasta aponta para o segundo. Senão, é preciso **achar o anterior**, porque a lista só anda para a frente. O `anterior != NULL &&` vem primeiro porque o `&&` para no primeiro falso, e assim nunca lê um ponteiro `NULL`.

Essa função **não libera memória**, só desliga o nó. O `mv` usa isso para mudar o nó de pasta, e o `rm` chama o `free` depois.

### 14.5 `procurar_filho_pelo_nome`

```c
if (strcmp(nome, ".") == 0)  { return pasta; }
if (strcmp(nome, "..") == 0) { return pasta->pai; }

NoArvore *filho = pasta->primeiro_filho;
while (filho != NULL) {
    if (strcmp(filho->fcb.nome, nome) == 0) {
        return filho;
    }
    filho = filho->proximo_irmao;
}
return NULL;
```

- **`strcmp` devolve 0 quando os textos são iguais.** Em C, `nome == "."` compararia **endereços**, e não letras.
- Como `.` e `..` são tratados aqui, num lugar só, eles funcionam em qualquer comando.

### 14.6 `encontrar_pelo_caminho`

```c
char copia_do_caminho[TAMANHO_MAXIMO_CAMINHO];
strncpy(copia_do_caminho, caminho, TAMANHO_MAXIMO_CAMINHO - 1);
copia_do_caminho[TAMANHO_MAXIMO_CAMINHO - 1] = '\0';

NoArvore *posicao;
if (copia_do_caminho[0] == '/') {
    posicao = raiz;
} else {
    posicao = diretorio_atual;
}

char *pedaco = strtok(copia_do_caminho, "/");
while (pedaco != NULL) {
    posicao = procurar_filho_pelo_nome(posicao, pedaco);
    if (posicao == NULL) {
        return NULL;
    }
    pedaco = strtok(NULL, "/");
}
return posicao;
```

- **Por que copiar?** O `strtok` **modifica** o texto, trocando cada `/` por `'\0'`, e o caminho recebido é `const`.
- **Absoluto ou relativo** é só o primeiro `if`: começa com `/`, parte da raiz; senão, da pasta atual.
- **`strtok`** corta em pedaços (`"home"`, `"alice"`, `"notas.txt"`). A primeira chamada recebe o texto, e as próximas recebem `NULL` para continuar. A cada pedaço, desce um nível.

### 14.7 `separar_pasta_e_nome`

Usada para **criar** coisas: em `echo oi > docs/novo.txt`, o arquivo ainda não existe, então é preciso achar a pasta `docs` e separar o nome `novo.txt`.

- **`strrchr(caminho, '/')`** acha a **última** barra. O que vem antes é a pasta, e o que vem depois é o nome.
- **`ultima_barra - caminho`**: subtrair dois ponteiros do mesmo texto dá a distância entre eles (aritmética de ponteiros).
- **Nome com 32 caracteres ou mais** não é cortado escondido: a função devolve um nome vazio, e quem chamou recusa com "nome inválido".
- **`NoArvore **pasta_encontrada`** (ponteiro para ponteiro): o `return` já diz se deu certo. Para devolver também a pasta, quem chama passa `&pasta`, e a função escreve `*pasta_encontrada = pasta`.

### 14.8 `liberar_no_e_todos_os_filhos`

```c
NoArvore *filho = no->primeiro_filho;
while (filho != NULL) {
    NoArvore *proximo = filho->proximo_irmao;
    liberar_no_e_todos_os_filhos(filho);
    filho = proximo;
}
liberar_todos_os_blocos_do_arquivo(&no->fcb);
free(no);
```

- **Recursiva:** libera cada filho (que libera os filhos dele...) e só no fim o próprio nó. É o "de baixo para cima".
- **A linha mais importante é guardar o `proximo` ANTES.** Depois do `free(filho)`, ler `filho->proximo_irmao` seria usar memória já devolvida (*use-after-free*).
- Os blocos são devolvidos **antes** do `free`, porque depois não haveria mais a lista deles.

## 15. `permissoes.c` — a parte com bits

### 15.1 Escolher a classe e extrair o trio

```c
if (usuario->id_do_usuario == fcb->id_do_dono)  { return POSICAO_DO_TRIO_DONO; }
if (usuario->id_do_grupo   == fcb->id_do_grupo) { return POSICAO_DO_TRIO_GRUPO; }
return POSICAO_DO_TRIO_OUTROS;
```

A ordem dos `if` é a regra do Linux: dono, depois grupo, depois outros. Vale o primeiro que bater.

```c
int deslocado = permissoes >> posicao_do_trio;
int so_os_tres_ultimos_bits = deslocado & 7;
```

- **`>>`** empurra os bits para a direita, e os da ponta caem fora. `>> 3` traz o trio do grupo para a ponta.
- **`& 7`**: 7 é `111`. Um bit do resultado só é 1 se for 1 **nos dois lados**, então o `& 7` apaga tudo menos os 3 últimos bits. Por isso o 7 é chamado de **máscara**.

```
permissoes = 0754 = 111 101 100
>> 3              =     111 101
& 7               =         101   → 5 = r-x  (grupo)
```

### 15.2 `usuario_tem_permissao` — o bloqueio obrigatório

```c
if (usuario->id_do_usuario == 0) {
    return true;
}
int posicao = descobrir_posicao_do_trio(fcb, usuario);
int trio    = extrair_trio(fcb->permissoes, posicao);
bool tem_o_bit = (trio & permissao_desejada) != 0;
return tem_o_bit;
```

```
trio = 101 (r-x)
pedindo escrita (010):  101 & 010 = 000 → NEGADO
pedindo leitura (100):  101 & 100 = 100 → PERMITIDO
```

- O root (id 0) passa direto.
- Os parâmetros são **`const FCB *`** e **`const Usuario *`**: o ponteiro evita copiar a struct inteira, e o `const` garante que a função só lê.
- Todo comando que precisa de permissão passa por esta função.

### 15.3 `comando_chmod`

```c
int digito_dono   = modo[0] - '0';
int digito_grupo  = modo[1] - '0';
int digito_outros = modo[2] - '0';

int permissoes = (digito_dono   << POSICAO_DO_TRIO_DONO)
               | (digito_grupo  << POSICAO_DO_TRIO_GRUPO)
               | (digito_outros << POSICAO_DO_TRIO_OUTROS);
```

- **`modo[0] - '0'`**: o caractere `'7'` não é o número 7 (na tabela ASCII ele vale 55). Como os dígitos são seguidos na tabela, `'7' - '0'` = 55 − 48 = **7**.
- **`<<`** coloca cada dígito no seu trio, e **`|`** (OU) junta os três, porque um bit é 1 se for 1 em qualquer lado.

```
7 << 6 = 111 000 000
5 << 3 =     101 000
4 << 0 =         100
          ───────────  |
          111 101 100 = 0754
```

Antes disso, `modo_e_valido` confere se são exatamente 3 dígitos de 0 a 7, e `if (!e_root && !e_dono)` bloqueia quem não é dono nem root.

## 16. `disco.c` — o disco em blocos

### 16.1 As variáveis

```c
static char disco[QUANTIDADE_DE_BLOCOS_NO_DISCO * TAMANHO_DO_BLOCO];
static bool bloco_esta_ocupado[QUANTIDADE_DE_BLOCOS_NO_DISCO];
```

- `disco` é o "char[] que representa o disco" sugerido pelo enunciado. O bloco N começa na posição `N * 64`.
- `bloco_esta_ocupado` é o mapa de livres.
- **`static`**: só `disco.c` mexe nos vetores, e os outros arquivos usam as funções. Isso é **encapsulamento**.

### 16.2 First-fit e quantos blocos

```c
for (int numero_do_bloco = 0; numero_do_bloco < QUANTIDADE_DE_BLOCOS_NO_DISCO; numero_do_bloco++) {
    if (!bloco_esta_ocupado[numero_do_bloco]) {
        bloco_esta_ocupado[numero_do_bloco] = true;
        return numero_do_bloco;
    }
}
return -1;
```

Pega o **primeiro** bloco livre (first-fit) e marca como ocupado **antes** de devolver. O `-1` significa "disco cheio".

Para saber quantos blocos são necessários, divide pelo tamanho do bloco e arredonda para cima: `150 / 64` = 2 (a divisão de inteiros corta a parte decimal), e `150 % 64` = 22 de resto. Como sobrou resto, são 2 + 1 = **3 blocos**.

### 16.3 `gravar_conteudo_no_disco` — o coração do objetivo 3.4

```c
if (blocos_necessarios > MAXIMO_DE_BLOCOS_POR_ARQUIVO) {
    return ERRO_ARQUIVO_GRANDE_DEMAIS;
}
int blocos_disponiveis = contar_blocos_livres() + fcb->quantidade_de_blocos;
if (blocos_necessarios > blocos_disponiveis) {
    return ERRO_DISCO_CHEIO;
}
liberar_todos_os_blocos_do_arquivo(fcb);

size_t bytes_ja_gravados = 0;
for (int i = 0; i < blocos_necessarios; i++) {
    int numero_do_bloco = reservar_bloco_livre();
    ...
    char *inicio_do_bloco = &disco[numero_do_bloco * TAMANHO_DO_BLOCO];
    memcpy(inicio_do_bloco, conteudo + bytes_ja_gravados, bytes_deste_pedaco);

    fcb->blocos_do_arquivo[i] = numero_do_bloco;
    bytes_ja_gravados = bytes_ja_gravados + bytes_deste_pedaco;
}
```

- **As verificações vêm ANTES de liberar os blocos antigos.** Se liberasse primeiro e não coubesse, o arquivo perderia o conteúdo antigo e não ganharia o novo. Assim, ou grava tudo, ou nada muda.
- **`+ fcb->quantidade_de_blocos`**: os blocos que o arquivo já tem vão voltar, então contam como disponíveis.
- O retorno é um **enum**, e não `true`/`false`, porque existem dois erros diferentes, cada um com sua mensagem.
- **`bytes_deste_pedaco`** normalmente é 64, mas o último pedaço pode ser menor (22, no exemplo).
- **`conteudo + bytes_ja_gravados`** é aritmética de ponteiros: o endereço do pedaço atual dentro do texto.
- **`memcpy`** copia exatamente N bytes. O `strcpy` pararia no primeiro `'\0'` e não respeitaria o limite de 64.
- **`fcb->blocos_do_arquivo[i] = numero_do_bloco`** é **a linha que materializa a alocação indexada**.

**Ler** é o mesmo laço ao contrário, com uma proteção para não passar do tamanho do destino e um `'\0'` no fim, para o `printf` saber onde o texto acaba. **Liberar um bloco** marca como livre e zera com `memset`.

## 17. `comandos_arquivo.c` — os comandos

### 17.1 O padrão de todo comando

```
1. procurar o alvo            → se não achar: mensagem e return
2. conferir o tipo (arquivo ou pasta)
3. conferir a permissão       → se não puder: "permissão negada" e return
4. fazer a operação
5. atualizar as datas
```

Esse estilo se chama **retorno antecipado**: cada erro sai da função na hora, e o caminho feliz fica no fim, sem `if` dentro de `if`. É o que atende o requisito 5.3 (tratamento de erros com mensagens claras).

### 17.2 `echo`

```c
char conteudo_novo[TAMANHO_MAXIMO_DO_ARQUIVO + 1];
size_t tamanho_atual = 0;
if (adicionar_no_final) {
    tamanho_atual = ler_conteudo_do_disco(&arquivo->fcb, conteudo_novo, sizeof(conteudo_novo));
}
size_t tamanho_final = tamanho_atual + tamanho_do_texto + 1;   /* +1 do "\n" */
...
memcpy(conteudo_novo + tamanho_atual, texto, tamanho_do_texto);
conteudo_novo[tamanho_atual + tamanho_do_texto] = '\n';
```

- Se o arquivo não existe, é criado. Se existe, precisa de `w` **no arquivo**.
- O vetor local tem o tamanho do maior arquivo + 1. Não precisa de `malloc`, e some sozinho no fim da função.
- **Com `>>`**, o vetor começa com o conteúdo antigo. **Com `>`**, começa vazio: é assim que o `>` **trunca**.
- O tamanho é conferido antes de copiar, e o `'\n'` do echo entra no fim.

### 17.3 `rm`

```c
NoArvore *pasta = arquivo->pai;
if (!usuario_tem_permissao(&pasta->fcb, &usuario_atual, PERMISSAO_DE_ESCRITA)) { ... }
remover_filho_da_pasta(pasta, arquivo);
liberar_no_e_todos_os_filhos(arquivo);
```

- A permissão é checada **na pasta** (`arquivo->pai`).
- **Primeiro desliga, depois libera.** Ao contrário, a pasta ficaria apontando para memória que não existe mais (ponteiro pendurado), e o próximo `ls` travaria.

### 17.4 `cp`

Chama `criar_no` (**inode novo**) e `gravar_conteudo_no_disco` (**blocos novos**). Se a gravação falhar (disco cheio) e o arquivo tiver acabado de ser criado, a variável `criou_arquivo_novo` faz o programa apagá-lo, para não deixar um arquivo vazio de lixo para trás.

### 17.5 `mv`

```c
if (esta_dentro_de(pasta_destino, item)) {
    printf("mv: não é possível mover uma pasta para dentro dela mesma\n");
    return;
}
...
NoArvore *pasta_origem = item->pai;
...
remover_filho_da_pasta(pasta_origem, item);
strncpy(item->fcb.nome, nome_destino, TAMANHO_MAXIMO_NOME - 1);
adicionar_filho(pasta_destino, item);
```

- **`esta_dentro_de`** sobe de pai em pai a partir do destino. Se encontrar o item no caminho, o movimento criaria um laço, então bloqueia. Ela para na raiz, porque o pai da raiz é ela mesma e o loop nunca acabaria.
- Exige `w` na pasta de onde sai **e** na pasta para onde vai.
- **`pasta_origem` é guardada antes**, porque `remover_filho_da_pasta` zera o `pai`.
- **As três últimas linhas são o `mv` inteiro:** desliga, troca o nome, religa. Nenhum byte é copiado, e o inode e os blocos continuam os mesmos.

## 18. `comandos_diretorio.c`

- **`mkdir`** aceita nome ou caminho (`/home/alice`), usando `separar_pasta_e_nome`. Exige `w` na pasta de cima.
- **`cd`** exige `x` na pasta de destino. **`ls`** exige `r`.
- **`rmdir`** confere: existe? é pasta? não é a raiz nem a pasta atual (ou uma acima dela)? está vazia? E exige `w` na pasta de cima.
- O **`ls -l`** usa `localtime` + `strftime` para transformar o `time_t` em texto (`"%d/%m %H:%M"`).

## 19. `terminal.c` — do texto digitado ao comando

```c
while (true) {
    mostrar_prompt();
    if (fgets(linha, sizeof(linha), stdin) == NULL) {
        break;
    }
    ...
    if (!executar_linha(linha)) {
        break;
    }
}
```

- **`fgets`** lê uma linha **com limite de tamanho**, então nunca estoura o vetor. Devolve `NULL` no fim da entrada (Ctrl+D ou fim do roteiro), e o programa sai normalmente, liberando a memória.
- **`isatty(fileno(stdin))`** pergunta se a entrada é um teclado. Com `./minifs < roteiro.txt`, não é, e o programa imprime o comando depois do prompt, como se alguém digitasse.

```c
linha[strcspn(linha, "\n")] = '\0';
```

O `fgets` guarda o Enter (`'\n'`). O `strcspn` acha a posição dele, e o `'\0'` corta ali. Sem isso, `cd home` procuraria a pasta `"home\n"`.

- **O `echo` é tratado ANTES de separar as palavras**, porque o texto dele tem espaços que se perderiam.
- No echo, o programa procura **`>>` antes de `>`**, porque o `>` está dentro do `>>`. Depois coloca um `'\0'` no lugar do sinal, cortando a linha em "texto" e "nome do arquivo".
- O resto é uma sequência de `if / else if` com `strcmp`, chamando a função certa.
- Os usuários de teste ficam num vetor `static const Usuario usuarios_cadastrados[]`. O `su` copia a struct escolhida para `usuario_atual`, e em C atribuir uma struct copia todos os campos de uma vez.

## 20. Operadores de bits (revisar antes da apresentação)

| Operador | Nome | Exemplo | Para que usamos |
|---|---|---|---|
| `>>` | desloca para a direita | `0754 >> 3` = `0075` | trazer um trio para a ponta |
| `<<` | desloca para a esquerda | `7 << 6` = `0700` | colocar um dígito no seu trio (chmod) |
| `&` | E bit a bit | `0075 & 7` = `5` | isolar um trio / testar um bit |
| `\|` | OU bit a bit | `0700 \| 0050 \| 0004` = `0754` | juntar os três trios (chmod) |

## 21. Perguntas técnicas que podem aparecer

**"Por que `calloc` e não `malloc`?"**\
O `calloc` zera a memória, então os ponteiros nascem `NULL`. Com `malloc`, teriam lixo.

**"Por que o `0` na frente de `0644`?"**\
Em C, número com zero na frente é octal, e permissões são octais.

**"Por que guardar o `proximo` antes do `free`?"**\
Depois do `free`, aquela memória não é mais do programa. Ler dela seria *use-after-free*.

**"Por que o `rm` desliga o nó antes de liberar?"**\
Senão a pasta ficaria apontando para memória liberada, e o próximo `ls` travaria.

**"Por que a gravação confere o espaço antes de liberar os blocos antigos?"**\
Para, se não couber, o arquivo manter o conteúdo antigo. Ou dá certo inteiro, ou nada muda.

**"Por que copiar o caminho antes do `strtok`?"**\
Porque o `strtok` troca as `/` por `'\0'` no texto original, e o caminho é `const`.

**"Para que serve o ponteiro duplo em `separar_pasta_e_nome`?"**\
Para devolver a pasta "por fora", já que o `return` diz se deu certo.

**"Por que `modo[0] - '0'`?"**\
Para converter o caractere `'7'` no número 7, já que os dígitos são seguidos na tabela ASCII.

**"Por que `static` no disco e no contador de inode?"**\
Para só o próprio `.c` mexer neles (encapsulamento).

**"Por que passar `const FCB *` em vez de `FCB`?"**\
O ponteiro evita copiar a struct inteira, e o `const` garante que a função só lê.
