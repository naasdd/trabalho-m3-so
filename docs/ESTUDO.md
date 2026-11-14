# Mini Sistema de Arquivos em C — Material de Estudo

Trabalho M3 de Sistemas Operacionais (UNIVALI) — apresentação em sala.

Este material tem duas partes:

- **Parte 1 — Entendendo o trabalho (≈ 30 minutos de leitura).** Sem código. Explica o que o programa faz, por que faz e qual conceito da matéria cada coisa demonstra. No fim tem o roteiro da apresentação e as perguntas que o professor provavelmente vai fazer.
- **Parte 2 — Estudando o código.** Arquivo por arquivo, as linhas importantes: o que fazem, como fazem e por que foram escritas assim.

Leia a Parte 1 inteira antes de abrir o código. Depois que a ideia estiver clara, o código fica fácil de ler.

---

# PARTE 1 — ENTENDENDO O TRABALHO

## 1. O trabalho em uma frase

Fizemos, em C, **um terminal que imita o terminal do Linux**, onde dá para criar pastas e arquivos, escrever, ler, copiar, mover, apagar e controlar quem pode fazer o quê. A diferença é que **nada é salvo no disco de verdade**: tudo existe só na memória enquanto o programa está aberto.

É como um "Linux de brinquedo". Por fora, você digita `mkdir`, `ls -l`, `cat`, `chmod 754`, como no Linux. Por dentro, fomos nós que construímos todas as peças que o sistema operacional de verdade tem: a ficha de cada arquivo, a árvore de pastas, a checagem de permissões e o disco dividido em blocos.

O objetivo do professor não é ver se sabemos **usar** os comandos do Linux, e sim se entendemos **como eles funcionam por dentro**. Cada peça do programa existe para demonstrar um conceito da matéria.

## 2. O problema que um sistema de arquivos resolve

Imagine o disco do computador como um caderno gigante, com milhões de linhas numeradas, onde você pode escrever bytes. O disco em si não sabe o que é um "arquivo". Ele só sabe "guardar estes bytes na posição 4.000.123" e "me devolva o que está na posição 4.000.123".

Agora imagine pedir para o usuário trabalhar assim: "seu trabalho de SO está nas posições 4.000.123 até 4.002.000, e a foto da viagem está nas posições 9.100.000 até 9.300.000". Seria impossível.

O **sistema de arquivos** é a camada do sistema operacional que resolve isso. Ele oferece:

1. **Nomes** em vez de posições: você pede `trabalho.txt`, e ele sabe onde estão os bytes.
2. **Organização** em pastas, para não ficar tudo misturado.
3. **Informações** sobre cada arquivo: tamanho, quando foi criado, de quem é.
4. **Proteção**, para controlar quem pode ler, alterar ou executar.
5. **Gerenciamento do espaço**, decidindo em que pedaço do disco cada arquivo vai e reaproveitando o espaço quando algo é apagado.

O enunciado chama isso de "mapeamento lógico para físico": **lógico** é o nome que você vê (`/home/alice/notas.txt`), e **físico** é onde os bytes realmente estão (blocos 4, 9 e 10 do disco).

Nosso trabalho implementa essas cinco coisas em versão simplificada.

## 3. O que é um arquivo, para o sistema operacional

Para nós, um arquivo é "um documento". Para o sistema operacional, um arquivo é **um conjunto de bytes acompanhado de uma ficha de informações**.

Essas informações se chamam **atributos**. A matéria lista estes:

| Atributo | O que é | Exemplo |
|----------|---------|---------|
| **Nome** | como o humano chama o arquivo | `notas.txt` |
| **Identificador** | um número único que o sistema usa internamente | inode 5 |
| **Tipo** | que tipo de conteúdo é | texto, numérico, binário, programa |
| **Localização** | onde os bytes estão no disco | blocos 4, 9, 10 |
| **Tamanho** | quantos bytes tem | 150 bytes |
| **Proteção** | quem pode fazer o quê | `rwxr-xr--`, dono alice |
| **Datas** | criação, última modificação, último acesso | 14/11/2026 20:15 |

No nosso programa, **cada um desses atributos é um campo de uma struct em C**. Essa struct se chama **FCB**.

O nome e o identificador são coisas diferentes. O nome é um rótulo para humanos e pode mudar (quando você renomeia). O identificador é a "identidade" do arquivo para o sistema e nunca muda. Isso vai aparecer de novo quando falarmos do `mv`.

## 4. O FCB — a ficha de cada arquivo (e o tal "inode")

**FCB** significa *File Control Block*, ou "bloco de controle de arquivo". É a ficha onde o sistema operacional anota tudo sobre um arquivo. No Linux, essa ficha se chama **inode**. Para o nosso trabalho, FCB e inode são a mesma ideia.

Pense numa biblioteca antiga com fichário. Cada livro tem uma ficha de papel com título, autor, ano, número de páginas e em qual estante está. **A ficha não é o livro.** A ficha só diz onde o livro está.

O FCB funciona igual:
- O FCB guarda **todas as informações** do arquivo.
- O FCB **não guarda o conteúdo**, só a lista de **onde** o conteúdo está no disco (quais blocos).

Por que separar? Porque assim muitas operações nem precisam mexer no conteúdo:
- Para mostrar o `ls -l` (nome, tamanho, data, permissões), basta ler as fichas.
- Para renomear, basta trocar o nome na ficha.
- Para mudar permissões, basta trocar um número na ficha.

No nosso programa, cada arquivo recebe um **número de inode** quando é criado. Esse número vem de um contador que só cresce (1, 2, 3, ...), então nunca se repete. O comando `stat` mostra a ficha inteira de um arquivo:

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

Repare na última linha: **"Blocos: 0"**. É a localização: o conteúdo desse arquivo está no bloco 0 do disco simulado.

**Pastas também têm FCB.** Uma pasta é tratada como um arquivo especial, do tipo "diretório". Ela tem nome, inode, dono, permissões e datas, mas não tem conteúdo em blocos: o "conteúdo" de uma pasta é a lista de coisas que estão dentro dela.

## 5. Pastas organizadas em árvore

### 5.1 Por que árvore?

A matéria mostra uma evolução de como organizar os arquivos:

1. **Diretório de nível único:** todos os arquivos de todo mundo numa pasta só. Problemas: dois arquivos não podem ter o mesmo nome, e com milhares de arquivos fica impossível achar algo.
2. **Diretório de dois níveis:** uma pasta para cada usuário. Melhor, porque a alice e o bob podem ter um `notas.txt` cada. Mas cada usuário ainda tem tudo misturado dentro da sua pasta.
3. **Diretório em árvore:** pastas dentro de pastas, em quantos níveis quiser. É o que o Linux e o Windows usam, e é o que o nosso trabalho implementa.

As vantagens da árvore, que o enunciado pede para discutir:

- **Eficiência:** para achar `/home/alice/notas.txt`, o sistema não olha todos os arquivos do computador. Ele entra em `home`, depois em `alice`, depois procura `notas.txt`. Só olha o que está no caminho.
- **Nomeação:** o nome só precisa ser único **dentro da mesma pasta**. Pode existir `/home/alice/notas.txt` e `/home/bob/notas.txt` ao mesmo tempo.
- **Agrupamento:** coisas relacionadas ficam juntas (todos os arquivos do trabalho numa pasta, as fotos em outra).

### 5.2 Como desenhamos a árvore na memória

Uma árvore de pastas é assim:

```
            /
            |
          home
         /    \
     alice    bob
       |
   notas.txt
```

O problema é que uma pasta pode ter **qualquer quantidade de filhos**: 0, 2 ou 500. Como guardar isso em C?

A ideia mais óbvia seria cada pasta ter um vetor de filhos, por exemplo com espaço para 100. Mas aí uma pasta vazia desperdiça espaço para 100, e uma pasta com 101 itens não cabe.

A técnica que usamos chama **"primeiro filho / próximo irmão"**. Cada item da árvore (que chamamos de **nó**) guarda só três "setas" (ponteiros):

- uma seta para o **pai** (a pasta onde ele está),
- uma seta para o **primeiro filho** (se ele for uma pasta),
- uma seta para o **próximo irmão** (o próximo item da mesma pasta).

O mesmo desenho de antes fica assim na memória:

```
   /
   └─ primeiro filho ─▶ home
                         └─ primeiro filho ─▶ alice ── próximo irmão ──▶ bob
                                                └─ primeiro filho ─▶ notas.txt
```

Os filhos de uma pasta viram uma **fila encadeada**: a pasta aponta para o primeiro, o primeiro aponta para o segundo, e assim por diante. Para listar uma pasta (`ls`), é só começar no primeiro filho e ir seguindo as setas de "próximo irmão" até acabar.

Vantagens: não tem limite de itens por pasta, não desperdiça espaço e cada nó tem tamanho fixo.

Quando criamos um arquivo novo, ele entra **no começo** da fila de irmãos, porque é o jeito mais rápido (não precisa andar até o fim). É por isso que o nosso `ls` mostra primeiro o item mais recente.

### 5.3 Caminhos: absoluto, relativo, `.` e `..`

- **Caminho absoluto** começa com `/` e é contado a partir da raiz: `/home/alice/notas.txt`. Funciona de qualquer lugar.
- **Caminho relativo** é contado a partir da pasta onde você está agora: se você está em `/home`, então `alice/notas.txt` é o mesmo arquivo.
- **`.`** significa "a própria pasta", e **`..`** significa "a pasta de cima".

Um detalhe: **o pai da raiz é a própria raiz**. Então `cd ..` quando você já está em `/` continua em `/`, igual no Linux.

O programa sempre sabe "onde você está", porque guarda uma variável chamada **`diretorio_atual`**. O `cd` só troca o valor dessa variável, e o `pwd` sobe de pai em pai até a raiz para montar o caminho.

### 5.4 O que impedimos para a árvore não quebrar

- **Mover uma pasta para dentro dela mesma** (`mv /home /home/alice`). Isso criaria um laço: `home` estaria dentro de `alice`, que está dentro de `home`... A estrutura deixaria de ser uma árvore, e qualquer função que percorre a árvore ficaria em loop infinito. O programa detecta isso e bloqueia.
- **Apagar a pasta onde você está** (ou uma acima dela), porque você ficaria "flutuando" numa pasta que não existe mais.
- **Apagar pasta com coisas dentro** com `rmdir`: igual ao Linux, só apaga pasta vazia.

## 6. As operações com arquivos

### 6.1 As seis operações básicas da teoria

A matéria diz que as operações básicas com arquivos são **criar, escrever, ler, reposicionar, excluir e truncar**. Veja como cada uma aparece no programa:

| Operação | Comando | O que acontece |
|----------|---------|----------------|
| **Criar** | `touch a.txt` ou `echo oi > a.txt` | nasce uma ficha nova (FCB), com inode novo, ligada na pasta |
| **Escrever** | `echo texto > a.txt` | o texto é cortado em pedaços e guardado em blocos do disco |
| **Ler** | `cat a.txt` | os pedaços são buscados nos blocos, na ordem, e juntados |
| **Reposicionar** | `echo texto >> a.txt` | a escrita começa no **fim** do arquivo, não no começo |
| **Excluir** | `rm a.txt` | a ficha sai da pasta, os blocos voltam a ficar livres e a memória é liberada |
| **Truncar** | `echo novo > a.txt` (arquivo que já existia) | o conteúdo antigo é jogado fora e o arquivo recomeça só com o novo |

Também fizemos operações compostas:
- **`cp`** (copiar) = ler + criar + escrever.
- **`mv`** (mover/renomear).
- **`stat`** = ler a ficha inteira.
- **`./arquivo`** = executar (serve para testar a permissão de execução).

Cada operação também atualiza as **datas** certas. Ler atualiza a data de último acesso, e escrever atualiza a data de modificação.

### 6.2 O detalhe do `echo`: por que "oi" vira 3 bytes?

Quando você faz `echo oi > a.txt` no Linux, o arquivo fica com **3 bytes**: `o`, `i` e uma quebra de linha invisível (`\n`). O `echo` sempre coloca essa quebra no fim. Fizemos igual, para o simulador se comportar como o Linux. Se o professor perguntar por que o tamanho é 3 e não 2, a resposta é essa.

### 6.3 A diferença mais importante: `cp` × `mv`

Essa é uma ótima pergunta de prova e um ótimo momento da apresentação.

- **`cp a.txt b.txt`** cria um arquivo **novo**. O `b.txt` ganha **outro número de inode** e **outros blocos** no disco, com uma cópia do conteúdo. Depois disso são dois arquivos independentes: mudar um não muda o outro.
- **`mv a.txt b.txt`** **não copia nada**. O arquivo continua o mesmo, com **o mesmo inode e os mesmos blocos**. Só muda o nome na ficha, e talvez a pasta onde ele está pendurado.

Na demonstração, mostramos isso com o `stat`:
1. `cp notas.txt copia.txt` → o `stat` mostra inode **6** (o original era 5) e um bloco diferente.
2. `mv copia.txt renomeado.txt` → o `stat` continua mostrando inode **6** e o mesmo bloco.

É por isso que, no Linux, mover um arquivo de 10 GB dentro do mesmo disco é instantâneo, enquanto copiar demora.

### 6.4 Tipos de arquivo

O FCB tem um campo **tipo**. O enunciado pede pelo menos: numérico, caractere (texto), binário e programa. No nosso programa o tipo é decidido assim:
- nome termina com `.sh` → **programa**
- nome termina com `.bin` → **binário**
- conteúdo só tem números → **numérico**
- qualquer outra coisa → **texto**

As pastas têm o tipo **diretório**.

## 7. Permissões: quem pode fazer o quê

Essa é a parte mais "matemática" do trabalho, e a que o professor mais deve perguntar. Vale entender bem.

### 7.1 As três ações: r, w, x

- **r** (*read*, ler): ver o conteúdo.
- **w** (*write*, escrever): alterar o conteúdo.
- **x** (*execute*, executar): rodar como programa.

### 7.2 As três classes de pessoas

Cada arquivo tem um **dono** e um **grupo**. Em relação a um arquivo, qualquer usuário cai em **uma** de três classes:

- **dono** (*owner*): quem criou o arquivo,
- **grupo** (*group*): quem está no mesmo grupo do arquivo,
- **outros** (*others/public*): todo o resto.

Cada classe tem o seu próprio "rwx". Então cada arquivo tem **9 permissões**: 3 ações × 3 classes.

```
   dono    grupo   outros
   r w x   r w x   r w x
```

É isso que aparece no `ls -l`: `-rwxr-xr--` quer dizer que o dono pode tudo, o grupo pode ler e executar, e os outros só podem ler. O primeiro caractere é `d` se for pasta e `-` se for arquivo.

### 7.3 Os números: de onde vem o 754?

Cada ação vale um número:

```
   r = 4      w = 2      x = 1
```

Para cada classe, você soma as ações permitidas e chega num dígito de 0 a 7:

| Dígito | Conta | Permissão |
|--------|-------|-----------|
| 7 | 4+2+1 | `rwx` |
| 6 | 4+2 | `rw-` |
| 5 | 4+1 | `r-x` |
| 4 | 4 | `r--` |
| 0 | nada | `---` |

Então **`chmod 754`** significa: dono = 7 (`rwx`), grupo = 5 (`r-x`), outros = 4 (`r--`).

Por que 4, 2 e 1? Porque são as potências de 2 (2², 2¹, 2⁰). Cada ação é **um bit**, e um trio de bits forma um número de 0 a 7, que é exatamente um dígito em **base 8 (octal)**. Por isso permissões são escritas em octal: cada dígito octal representa exatamente um trio `rwx`.

```
   7    5    4         (octal)
  111  101  100        (binário)
  rwx  r-x  r--        (letras)
```

No programa, as 9 permissões ficam guardadas em **um único número inteiro**, e usamos os 9 bits mais baixos dele. É isso que o enunciado chama de **bitmask** (máscara de bits).

### 7.4 Como o programa decide se pode ou não

Quando alguém tenta fazer algo, o programa segue estes passos, igual ao Linux:

1. **É o root?** Se sim, pode tudo. Fim.
2. **Qual é a classe dessa pessoa em relação a esse arquivo?**
   - Se o id do usuário é o id do dono → classe **dono**.
   - Senão, se o grupo do usuário é o grupo do arquivo → classe **grupo**.
   - Senão → classe **outros**.
3. **Pega só o trio dessa classe** (os 3 bits dela).
4. **Olha se o bit da ação pedida está ligado** nesse trio. Se está, permite. Se não, mostra "permissão negada".

Atenção: vale **só uma** classe. Se você é o dono, valem as permissões do dono, mesmo que as dos "outros" sejam mais generosas.

O enunciado pede que **toda tentativa** de ler, escrever ou executar seja verificada, e chama isso de **bloqueio obrigatório**: o usuário não tem como "escapar" da verificação. No nosso programa, todo comando chama a mesma função de verificação antes de agir.

### 7.5 Usuários de teste

Para demonstrar as três classes, o programa já vem com quatro usuários, e o comando `su` troca entre eles:

| Usuário | id | grupo | Serve para mostrar... |
|---------|----|-------|------------------------|
| root | 0 | 0 | quem ignora permissões |
| alice | 1000 | 100 | a **dona** dos arquivos na demo |
| bob | 1001 | 100 | o **grupo** (mesmo grupo da alice) |
| guest | 2000 | 200 | os **outros** (grupo diferente) |

Na demo, a alice cria um `script.sh` e dá `chmod 754`. Então:
- a **alice** (dona, `rwx`) consegue executar,
- o **bob** (grupo, `r-x`) consegue executar, mas **não** consegue alterar,
- o **guest** (outros, `r--`) consegue ler, mas **não** consegue executar,
- o **root** consegue tudo.

### 7.6 Permissões em pastas (o detalhe que impressiona)

Em pastas, as letras têm outro significado:
- **r** na pasta = poder **listar** o que tem dentro (`ls`).
- **w** na pasta = poder **criar, apagar e renomear** coisas dentro dela.
- **x** na pasta = poder **entrar** nela (`cd`).

A consequência que surpreende muita gente: **para apagar um arquivo, o que importa é a permissão de escrita na PASTA, e não no arquivo.** Apagar é "tirar o nome da lista da pasta", ou seja, escrever na pasta. O nosso programa faz exatamente isso. Na demo, o guest não consegue apagar o `script.sh` porque não tem `w` na pasta da alice.

Outras duas regras do Linux que copiamos:
- **Só o dono (ou o root) pode dar `chmod`** num arquivo. Se não fosse assim, qualquer um daria `chmod 777` e a proteção não serviria para nada.
- Arquivos novos nascem com **`644`** (`rw-r--r--`), e pastas novas com **`755`** (`rwxr-xr-x`). São os padrões comuns no Linux. Por isso, na demo, o script nasce **sem** permissão de execução, nem para a dona: precisa de `chmod` primeiro, igual no Linux.

## 8. O disco de mentira e a divisão em blocos

### 8.1 Por que blocos?

O disco de verdade não é lido byte a byte: ele é lido em **blocos** de tamanho fixo (por exemplo 4096 bytes). Por isso o sistema de arquivos guarda cada arquivo como um conjunto de blocos.

No nosso trabalho, o "disco" é um vetor gigante de bytes na memória, dividido em **256 blocos de 64 bytes** (16 KB no total). Usamos blocos pequenos de propósito: com 64 bytes, até uma frase comum já ocupa 2 ou 3 blocos, e dá para **ver** a divisão acontecendo.

Além do disco, temos um **mapa de blocos livres**: uma lista de 256 posições dizendo, para cada bloco, se está ocupado ou livre. O comando `disk` desenha esse mapa (`#` = ocupado, `.` = livre):

```
   0 | #######.........................
  32 | ................................
  ...
7 de 256 blocos ocupados (64 bytes por bloco)
```

### 8.2 Os três jeitos de guardar um arquivo em blocos

A teoria mostra três métodos de **alocação**. Vale saber explicar os três, mesmo que só um esteja implementado.

**1. Alocação contígua.** O arquivo ocupa blocos **seguidos**, como uma família sentando lado a lado no cinema: blocos 10, 11, 12.
- Vantagem: muito rápido de ler, e para achar o pedaço N do arquivo basta somar.
- Problema: **fragmentação externa**. Depois de apagar e criar arquivos várias vezes, o espaço livre fica picado em buracos pequenos. Pode ter 30 blocos livres no total e mesmo assim não caber um arquivo de 5, porque não existem 5 blocos livres **seguidos**. Também é difícil aumentar o arquivo, porque o bloco seguinte pode estar ocupado.

**2. Alocação encadeada.** Cada bloco guarda um pedaço do arquivo **e o número do próximo bloco**, como numa caça ao tesouro em que cada pista diz onde está a próxima.
- Vantagem: sem fragmentação externa (qualquer bloco livre serve) e o arquivo cresce fácil.
- Problema: para chegar no pedaço 50, é preciso passar pelos 49 anteriores, então o acesso direto é lento. Um pouco de cada bloco é gasto com o "ponteiro", e se um bloco estragar, perde-se tudo o que vem depois.

**3. Alocação indexada.** Existe um **índice** (o "bloco de índice") que lista todos os blocos do arquivo, na ordem, como o sumário de um livro.
- Vantagem: sem fragmentação externa (os blocos podem estar espalhados) **e** com acesso direto (o pedaço N está em `índice[N]`).
- Problema: gasta espaço com o índice, e o tamanho do índice limita o tamanho máximo do arquivo.

**O nosso trabalho usa a alocação indexada.** O índice fica dentro da própria ficha (FCB): é a lista `blocos_do_arquivo`, com espaço para até 16 números de bloco. Por isso o maior arquivo possível tem 16 × 64 = **1024 bytes**.

### 8.3 Como um arquivo é gravado, passo a passo

Exemplo: gravar 150 bytes.

1. **Quantos blocos preciso?** 150 ÷ 64 = 2,3, arredondando para cima = **3 blocos**.
2. **Cabe?** Confere se 3 não passa do limite de 16 por arquivo e se existem 3 blocos livres no disco. Se não couber, dá erro **antes** de mexer em qualquer coisa.
3. **Devolve os blocos antigos** (se o arquivo já tinha conteúdo).
4. **Para cada pedaço:** pega o primeiro bloco livre, copia o pedaço para lá e anota o número do bloco na ficha.

```
  conteúdo:  [ bytes 0–63 ][ bytes 64–127 ][ bytes 128–149 ]
                   │              │                │
                   ▼              ▼                ▼
  disco:   ... [ bloco 4 ] ... [ bloco 9 ] [ bloco 10 ] ...

  ficha (FCB): tamanho = 150, blocos = {4, 9, 10}
```

Para **ler**, é o caminho inverso: percorre a lista de blocos da ficha, na ordem, e junta os pedaços.

A escolha "pegar o **primeiro** bloco livre" se chama **first-fit**.

### 8.4 O momento da demo que mostra a alocação indexada

Na demonstração fazemos isto:
1. Criamos um arquivo grande que ocupa os blocos **2, 3, 4**.
2. Apagamos um arquivo que estava no bloco **0**, o que abre um buraco no começo do disco.
3. Criamos outro arquivo que precisa de 3 blocos. O `stat` mostra **blocos 0, 5, 6**: ele usou o buraco **e** dois blocos lá no fim.

Esse arquivo está **espalhado** pelo disco, e funciona normalmente. Com alocação **contígua** isso seria impossível, porque ele precisaria de 3 blocos seguidos. É a prova visual de por que a alocação indexada evita a fragmentação externa.

### 8.5 Fragmentação interna

O último bloco de um arquivo quase nunca fica cheio. No exemplo de 150 bytes, o terceiro bloco usa só 22 dos 64 bytes, e os outros 42 ficam desperdiçados. Isso se chama **fragmentação interna**: espaço perdido **dentro** de um bloco. É o preço de usar blocos de tamanho fixo, e acontece em qualquer um dos três métodos.

Resumindo as duas fragmentações:
- **Interna:** sobra espaço dentro do último bloco do arquivo. Acontece em todos os métodos.
- **Externa:** o espaço livre fica picado em buracos que não servem para arquivos que precisam de blocos seguidos. É o problema da contígua, e a indexada resolve.

## 9. O que acontece quando você digita um comando

Juntando tudo, veja o caminho completo de `echo oi > notas.txt` digitado pela alice dentro de `/home/alice`:

1. **O terminal lê a linha** e percebe que começa com `echo`.
2. **Separa as partes:** texto = `oi`, arquivo = `notas.txt`, modo = substituir (um `>` só).
3. **Procura o arquivo** na pasta atual. Não existe, então vai criar.
4. **Verifica a permissão:** a alice pode escrever na pasta `/home/alice`? É a dona da pasta e a pasta é `755` (dono `rwx`), então sim.
5. **Cria a ficha (FCB):** inode novo, nome `notas.txt`, dona = alice, permissões `644`, datas = agora.
6. **Pendura a ficha** no começo da lista de filhos da pasta.
7. **Monta o conteúdo:** `oi` + quebra de linha = 3 bytes.
8. **Grava no disco:** 3 bytes cabem em 1 bloco, então pega o primeiro bloco livre (digamos o 0), copia os 3 bytes e anota `blocos = {0}` na ficha.
9. **Atualiza** o tamanho (3), o tipo (texto) e a data de modificação.
10. **Volta para o prompt.**

E `cat notas.txt` faz: acha o arquivo → confere se a alice pode **ler** → lê o bloco 0 → mostra `oi` → atualiza a data de acesso.

## 10. Memória: por que o professor fala de "memory leak"

Como tudo é feito em memória, cada pasta e cada arquivo criados ocupam um pedaço da memória do computador, pedido com uma função do C chamada **`calloc`** (prima do `malloc`). Em C, **ninguém devolve essa memória automaticamente**: se o programa pede e nunca devolve (com **`free`**), a memória fica presa até o programa fechar. Isso é um **vazamento de memória** (*memory leak*), e vale nota no critério "Funcionalidade e Robustez".

No nosso programa:
- **`rm` e `rmdir`** devolvem a memória do item apagado e liberam os blocos dele no disco.
- **Ao sair (`exit`)**, o programa percorre a árvore inteira, **de baixo para cima** (primeiro os filhos, depois o pai), devolvendo tudo.
- Testamos com uma ferramenta que procura vazamentos (`leaks` no Mac, `valgrind` no Linux) depois de uma sessão usando todos os comandos: **0 vazamentos**.

Por que de baixo para cima? Se apagássemos o pai primeiro, perderíamos as setas que levam aos filhos, e aí não teria mais como achá-los para devolver a memória deles.

## 11. Como o código está organizado

O código está dividido em arquivos por assunto, e cada um corresponde a um objetivo do enunciado:

| Arquivo | Assunto |
|---------|---------|
| `sistema_arquivos.h` | a "planta baixa": todas as structs (FCB, nó da árvore, usuário), constantes e a lista de funções |
| `arvore.c` | criar, procurar e apagar nós da árvore |
| `comandos_diretorio.c` | `mkdir`, `cd`, `ls`, `pwd`, `rmdir` — **objetivo 3.1** |
| `comandos_arquivo.c` | `touch`, `echo`, `cat`, `cp`, `mv`, `rm`, `stat`, `./` — **objetivo 3.2** |
| `permissoes.c` | verificação rwx, texto `rwxr-xr-x`, `chmod` — **objetivo 3.3** |
| `disco.c` | disco em blocos, gravar e ler — **objetivo 3.4** |
| `terminal.c` | lê o que você digita e chama o comando certo |
| `main.c` | liga tudo: inicia, roda o terminal, libera a memória |

Todos os nomes de funções e variáveis estão em português e dizem o que fazem: `usuario_tem_permissao`, `reservar_bloco_livre`, `diretorio_atual`... A ideia é que dê para ler o código quase como um texto.

## 12. Roteiro da apresentação

O roteiro de comandos está pronto em `demo/roteiro_apresentacao.txt`. Duas formas de usar:

- **Digitando ao vivo** (recomendado, porque passa mais segurança): abra o arquivo no celular ou impresso e vá digitando.
- **Plano B, se der nervoso ou faltar tempo:** `./minifs < demo/roteiro_apresentacao.txt` roda tudo de uma vez, e a saída aparece como se alguém estivesse digitando.

Antes de começar: `make` para compilar e `./minifs` para abrir.

**Parte 1 — Pastas (objetivo 3.1).** Comandos: `mkdir`, `cd`, `ls -l`, `pwd`.
> "A estrutura de pastas é uma árvore. Cada item é um nó com três ponteiros: pai, primeiro filho e próximo irmão. Assim uma pasta pode ter quantos itens quiser sem desperdiçar memória. O `cd` só muda a variável que guarda a pasta atual, e o `pwd` sobe de pai em pai até a raiz."

**Parte 2 — Arquivos e FCB (objetivo 3.2).** Comandos: `echo >`, `cat`, `echo >>`, `stat`, `cp`, `mv`.
> "Cada arquivo tem um FCB, que é o inode simulado: a ficha com nome, inode, tipo, tamanho, permissões, dono e as três datas. O `stat` mostra a ficha inteira. Reparem: o `cp` gerou um inode novo, o 6, com outro bloco. Depois o `mv` manteve o inode 6 e o mesmo bloco, porque mover não copia dados, só muda o nó de lugar na árvore."

**Parte 3 — Blocos (objetivo 3.4).** Comandos: `disk`, `echo` grande, `stat`, `rm`, `echo` grande, `stat`, `disk`.
> "O disco é um vetor de 256 blocos de 64 bytes. Esse texto de 131 bytes precisou de 3 blocos: 2, 3 e 4. Agora apago um arquivo que estava no bloco 0 e crio outro de 3 blocos: ele ficou nos blocos 0, 5 e 6, espalhados. Isso é a alocação indexada: o FCB guarda a lista dos blocos, então eles não precisam ser seguidos e não existe fragmentação externa. Com alocação contígua esse arquivo não caberia nesse espaço."

**Parte 4 — Permissões (objetivo 3.3).** Comandos: `./script.sh`, `chmod 754`, `ls -l`, e os `su bob` e `su guest` com cada tentativa.
> "As permissões ficam num inteiro como bitmask de 9 bits. O `chmod 754` monta esse número: 7 para o dono, 5 para o grupo, 4 para os outros. Para verificar, o programa escolhe a classe do usuário, isola os 3 bits dela com deslocamento e máscara e testa o bit da operação. O bob é do mesmo grupo, então vale r-x: executa mas não altera. O guest é dos outros, então vale r--: lê mas não executa. E não consegue apagar, porque apagar depende da permissão de escrita na pasta."

**Parte 5 — Erros.** Comandos com erro de propósito.
> "Todo comando confere os casos de erro antes de agir: não encontrado, nome inválido, mover pasta para dentro dela mesma, modo de chmod inválido..."

**Para fechar:**
> "Tudo é liberado ao sair, e testamos com ferramenta de detecção de vazamento: zero vazamentos."

**Dicas:**
- Se o professor pedir um comando diferente, não tem problema: tudo funciona com caminhos e tem mensagem de erro.
- Se travar, `help` lista todos os comandos.
- Se precisar recomeçar do zero, `exit` e `./minifs` de novo (tudo some ao sair, porque é em memória).

## 13. Perguntas que o professor provavelmente vai fazer

**"O que é o FCB e o que ele guarda?"**
É a ficha de controle de cada arquivo, o equivalente ao inode do Linux. Guarda nome, número do inode, tipo, tamanho, permissões, dono, grupo, data de criação, de modificação, de último acesso e a lista dos blocos onde está o conteúdo. Não guarda o conteúdo em si.

**"Por que vocês usaram essa estrutura de árvore?"**
Porque uma pasta pode ter qualquer número de filhos. Com "primeiro filho / próximo irmão", cada nó tem só 3 ponteiros de tamanho fixo, os filhos formam uma lista encadeada, não tem limite e não desperdiça espaço.

**"Quais as vantagens do diretório em árvore?"**
Eficiência (só percorre o caminho, não tudo), nomeação (o nome só precisa ser único dentro da pasta, então dá para ter arquivos com o mesmo nome em pastas diferentes) e agrupamento (arquivos relacionados juntos).

**"Como funciona o chmod 754 por dentro?"**
Cada dígito vira um trio de bits: 7 = 111, 5 = 101, 4 = 100. O dígito do dono é empurrado 6 bits para a esquerda, o do grupo 3 bits, e os três são juntados com OU bit a bit. Resultado: o número 0754, que é 111 101 100 em binário.

**"Como o programa sabe se o usuário pode ler?"**
Se for root, pode. Senão, escolhe a classe (dono se o id bate, grupo se o grupo bate, senão outros), desloca as permissões para trazer o trio dessa classe para o fim, aplica `& 7` para ficar só com esses 3 bits e testa o bit de leitura (valor 4) com `&`.

**"Por que o guest não conseguiu apagar o arquivo, se ele tinha permissão de leitura?"**
Porque apagar depende da permissão de **escrita na pasta**, e não do arquivo. Apagar é remover o nome da lista da pasta. O guest não tem `w` na pasta da alice.

**"Qual método de alocação vocês usaram e por quê?"**
Indexada. O FCB tem um vetor com os números dos blocos do arquivo. Escolhemos porque evita a fragmentação externa da contígua (os blocos podem ficar espalhados) e permite acesso direto, ao contrário da encadeada, onde é preciso seguir a corrente bloco a bloco.

**"Qual a diferença entre fragmentação interna e externa?"**
Interna é o espaço que sobra dentro do último bloco de um arquivo, e acontece sempre com blocos de tamanho fixo. Externa é o espaço livre picado em buracos que não servem para quem precisa de blocos seguidos, que é o problema da alocação contígua.

**"Qual o tamanho máximo de um arquivo no simulador?"**
1024 bytes: 16 blocos (o tamanho do índice no FCB) × 64 bytes por bloco.

**"O que acontece se o disco encher?"**
A gravação confere antes de começar se existem blocos livres suficientes. Se não existirem, mostra "disco cheio" e não mexe em nada, então o conteúdo antigo do arquivo não se perde.

**"Qual a diferença entre cp e mv por dentro?"**
O `cp` cria um FCB novo (inode novo) e grava o conteúdo em blocos novos. O `mv` só tira o nó de uma pasta e coloca em outra (ou troca o nome): mesmo inode, mesmos blocos.

**"Como vocês evitam memory leak?"**
Todo nó é criado com `calloc` e liberado com `free`. O `rm` libera o nó e devolve os blocos. Ao sair, uma função recursiva libera a árvore inteira, começando pelos filhos. Testamos com `leaks`/`valgrind`: zero vazamentos.

**"Por que o pai da raiz é ela mesma?"**
Para o `cd ..` na raiz funcionar sem caso especial e continuar na raiz, igual ao Linux.

**"Onde está o 'usuário atual'?"**
Numa variável global, `usuario_atual`, como o enunciado sugere. O `su` troca o valor dela, e todas as verificações usam ela.

## 14. Resumo em frases curtas (bom para flashcards)

- O trabalho é um terminal estilo Linux que simula um sistema de arquivos inteiro na memória.
- Um arquivo, para o SO, é um conjunto de bytes mais uma ficha de atributos.
- Os atributos de um arquivo são nome, identificador, tipo, localização, tamanho, proteção e datas.
- O FCB (File Control Block) é a ficha do arquivo; no Linux se chama inode.
- O FCB guarda onde está o conteúdo, mas não o conteúdo.
- O número do inode é a identidade do arquivo e nunca muda; o nome pode mudar.
- A árvore de pastas usa nós com três ponteiros: pai, primeiro filho e próximo irmão.
- Os filhos de uma pasta formam uma lista encadeada.
- O pai da raiz é a própria raiz.
- As vantagens do diretório em árvore são eficiência, nomeação e agrupamento.
- As operações básicas com arquivos são criar, escrever, ler, reposicionar, excluir e truncar.
- `echo >` trunca e escreve; `echo >>` escreve no final (reposiciona).
- O `cp` cria inode e blocos novos; o `mv` mantém o mesmo inode e os mesmos blocos.
- As permissões são 9 bits: rwx para dono, grupo e outros.
- r = 4, w = 2, x = 1; somando, cada classe vira um dígito de 0 a 7.
- `chmod 754` = dono rwx, grupo r-x, outros r--.
- Permissões são escritas em octal porque cada dígito octal é exatamente 3 bits.
- Para verificar, o programa escolhe uma classe, isola o trio com `>>` e `& 7` e testa o bit com `&`.
- O root ignora as permissões.
- Só o dono ou o root pode usar o chmod.
- Apagar um arquivo depende da permissão de escrita na pasta.
- `x` em pasta significa poder entrar nela; `r` em pasta significa poder listar.
- O disco simulado tem 256 blocos de 64 bytes.
- O mapa de blocos livres diz quais blocos estão ocupados.
- Os três métodos de alocação são contígua, encadeada e indexada.
- A contígua sofre com fragmentação externa.
- A encadeada tem acesso direto lento.
- A indexada usa um índice com a lista dos blocos; é a que o trabalho usa.
- O índice fica dentro do FCB e tem 16 posições, então o arquivo máximo tem 1024 bytes.
- First-fit significa usar o primeiro bloco livre encontrado.
- Fragmentação interna é a sobra dentro do último bloco.
- Memory leak é memória pedida e nunca devolvida.
- A árvore é liberada de baixo para cima para não perder os ponteiros dos filhos.

---

# PARTE 2 — ESTUDANDO O CÓDIGO

Agora que a ideia está clara, vamos ao código. A ordem segue o caminho natural: primeiro as definições, depois a estrutura, depois os comandos e por fim o terminal. Não é preciso decorar as linhas. O importante é conseguir apontar para uma parte e explicar **o que ela faz e por que está ali**.

## 15. Mapa geral: quem chama quem

```
main.c
  │
  ├─ iniciar_sistema_de_arquivos()      (arvore.c)
  │     ├─ iniciar_disco()              (disco.c)
  │     └─ criar_no("/")                (arvore.c)
  │
  ├─ rodar_terminal()                   (terminal.c)
  │     └─ loop: mostrar_prompt → fgets → executar_linha
  │                                          │
  │              ┌───────────────────────────┼──────────────────────────┐
  │              ▼                           ▼                          ▼
  │     comandos_diretorio.c        comandos_arquivo.c           permissoes.c
  │     (mkdir, cd, ls...)          (echo, cat, cp, mv...)       (chmod)
  │              │                           │                          │
  │              └──────────┬────────────────┴───────────┬──────────────┘
  │                         ▼                            ▼
  │                arvore.c (procurar,           permissoes.c
  │                criar, remover nós)           (usuario_tem_permissao)
  │                                                      │
  │                                  disco.c (gravar, ler, liberar blocos)
  │
  └─ encerrar_sistema_de_arquivos()     (arvore.c) → libera tudo
```

Todo arquivo `.c` começa com `#include "sistema_arquivos.h"`. É assim que um arquivo "conhece" as structs e as funções dos outros.

## 16. Makefile — como o programa é compilado

```make
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -pedantic -g
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:.c=.o)
BIN     = minifs
```

- `CC = gcc`: o compilador.
- `-std=c11`: usa o padrão C11 da linguagem.
- `-Wall -Wextra -pedantic`: liga **todos os avisos**. O código compila sem nenhum aviso, o que é um bom sinal de qualidade (critério de 20% da nota).
- `-g`: guarda informações de depuração, para o `valgrind` mostrar a linha exata de um problema.
- `$(wildcard src/*.c)`: pega todos os `.c` da pasta, sem precisar listar um por um.
- `$(SRC:.c=.o)`: para cada `.c`, gera um `.o` (arquivo objeto, código compilado ainda não ligado).

```make
src/%.o: src/%.c src/sistema_arquivos.h
	$(CC) $(CFLAGS) -c $< -o $@
```

Cada `.o` depende do seu `.c` **e do header**. Se você muda o header, tudo recompila; se muda só um `.c`, só ele recompila.

## 17. `sistema_arquivos.h` — a planta baixa

### 17.1 Constantes

```c
#define TAMANHO_DO_BLOCO              64
#define QUANTIDADE_DE_BLOCOS_NO_DISCO 256
#define MAXIMO_DE_BLOCOS_POR_ARQUIVO  16
#define TAMANHO_MAXIMO_DO_ARQUIVO     (MAXIMO_DE_BLOCOS_POR_ARQUIVO * TAMANHO_DO_BLOCO)
```

**Por que `#define` e não números soltos no código?** Se quisermos blocos de 128 bytes, mudamos **uma linha** e o programa inteiro se adapta. Números soltos no meio do código ("números mágicos") obrigariam a caçar cada `64` espalhado.

**Por que os parênteses em `TAMANHO_MAXIMO_DO_ARQUIVO`?** O `#define` é uma troca de texto. Sem parênteses, algo como `x / TAMANHO_MAXIMO_DO_ARQUIVO` viraria `x / 16 * 64`, que em C é `(x / 16) * 64`: conta errada. Com parênteses vira `x / (16 * 64)`.

```c
#define PERMISSAO_DE_LEITURA   4   /* r */
#define PERMISSAO_DE_ESCRITA   2   /* w */
#define PERMISSAO_DE_EXECUCAO  1   /* x */

#define POSICAO_DO_TRIO_DONO    6
#define POSICAO_DO_TRIO_GRUPO   3
#define POSICAO_DO_TRIO_OUTROS  0

#define PERMISSOES_PADRAO_DE_ARQUIVO   0644
#define PERMISSOES_PADRAO_DE_DIRETORIO 0755
```

- **4, 2, 1** são os valores de r, w e x dentro de um trio (100, 010 e 001 em binário).
- **6, 3, 0** são quantos bits é preciso andar para chegar no trio de cada classe. O trio do dono está nos bits 8-7-6, o do grupo nos bits 5-4-3, e o dos outros nos bits 2-1-0.
- **`0644` com zero na frente**: em C, **um número que começa com 0 é octal**. Então `0644` é o octal 644, e não o decimal 644. Sem o zero, as permissões ficariam completamente erradas. É um detalhe que o professor pode perguntar.

### 17.2 O enum de tipos

```c
typedef enum {
    TIPO_DIRETORIO,
    TIPO_TEXTO,
    TIPO_NUMERICO,
    TIPO_BINARIO,
    TIPO_PROGRAMA
} TipoArquivo;
```

Um `enum` dá nomes a números (`TIPO_DIRETORIO` = 0, `TIPO_TEXTO` = 1, ...). **Por que não usar o número direto?** Porque `if (tipo == TIPO_DIRETORIO)` se explica sozinho, e `if (tipo == 0)` não. O `typedef` permite escrever `TipoArquivo tipo;` em vez de `enum algo tipo;`. O enunciado cita "enum ou inteiro" para o tipo, e o critério de qualidade cita o uso de `typedef` e `enums`.

### 17.3 A struct FCB

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

Campo por campo, com o porquê:

- **`numero_inode`**: o identificador único. É `int` porque o enunciado pede "ID único (simulando um inode) (inteiro)".
- **`char nome[32]`**: o nome fica **dentro** da struct, num vetor fixo. Assim não precisa de `malloc` separado para o nome, nem de `free` depois, o que significa um vazamento possível a menos. O tamanho fixo é o motivo de o nome ter no máximo 31 caracteres (o 32º é o `'\0'` que termina toda string em C).
- **`size_t tamanho_em_bytes`**: `size_t` é o tipo que o C usa para tamanhos. Ele nunca é negativo e é o que funções como `strlen` e `memcpy` usam, então misturar não dá aviso.
- **`time_t` nas datas**: o tipo padrão do C para datas, que guarda os segundos desde 1970. O enunciado pede `time_t` explicitamente. `time(NULL)` devolve o "agora".
- **`int permissoes`**: o bitmask com os 9 bits de permissão.
- **`id_do_dono` e `id_do_grupo`**: guardamos o **número** do usuário, e não o nome, igual ao Linux (que guarda o UID e o GID).
- **`blocos_do_arquivo[16]` e `quantidade_de_blocos`**: o **bloco de índice** da alocação indexada. O vetor tem 16 posições, mas só as primeiras `quantidade_de_blocos` são válidas. Sem o contador, não daria para saber onde a lista acaba.

### 17.4 A struct NoArvore

```c
typedef struct NoArvore {
    FCB              fcb;
    struct NoArvore *pai;
    struct NoArvore *primeiro_filho;
    struct NoArvore *proximo_irmao;
} NoArvore;
```

- **`FCB fcb`** (sem asterisco): a ficha fica **dentro** do nó, e não num lugar separado. Um `calloc` cria o nó e a ficha juntos, e um `free` libera os dois.
- **Os três ponteiros** (com asterisco) guardam **endereços de memória** de outros nós. Não são os nós em si, são "setas" para eles. Um ponteiro `NULL` significa "não aponta para ninguém": sem filhos, sem próximo irmão.
- **Por que `struct NoArvore` tem nome antes do `{`?** Porque a struct aponta para ela mesma. Dentro das chaves o apelido `NoArvore` (que vem do `typedef`) ainda não existe, então é preciso usar o nome completo `struct NoArvore *`.
- **Por que ter o `pai`?** Sem ele, o `cd ..` e o `pwd` seriam muito difíceis: teríamos que procurar o pai a partir da raiz. Com ele, é só seguir a seta.

### 17.5 Variáveis globais com `extern`

```c
extern NoArvore *raiz;
extern NoArvore *diretorio_atual;
extern Usuario   usuario_atual;
```

**`extern`** quer dizer: "esta variável existe, mas foi criada em outro arquivo". As variáveis são realmente criadas em `arvore.c` (`raiz`, `diretorio_atual`) e em `terminal.c` (`usuario_atual`). O `extern` no header permite que todos os `.c` usem as mesmas variáveis. Sem o `extern`, cada arquivo criaria a sua própria cópia, e o linker daria erro de variável definida duas vezes.

## 18. `main.c` — o ponto de partida

```c
int main(void)
{
    iniciar_sistema_de_arquivos();
    rodar_terminal();
    encerrar_sistema_de_arquivos();
    return 0;
}
```

São três passos, de propósito: **montar, usar, desmontar**. Lendo o `main`, você entende o programa inteiro em 3 linhas. O `encerrar` só roda quando o `rodar_terminal` termina (o usuário digitou `exit` ou apertou Ctrl+D), e é ele que garante zero vazamentos.

## 19. `arvore.c` — a estrutura da árvore

### 19.1 `iniciar_sistema_de_arquivos`

```c
iniciar_disco();
raiz = criar_no("/", TIPO_DIRETORIO);
raiz->pai = raiz;
diretorio_atual = raiz;
```

- Zera o disco, cria a pasta `/` e começa com o usuário dentro dela.
- **`raiz->pai = raiz`**: a raiz é o único nó sem pai de verdade. Fazendo ela apontar para si mesma, o `cd ..` na raiz não precisa de um `if` especial: `procurar_filho_pelo_nome(raiz, "..")` devolve `raiz->pai`, que é a própria raiz.
- **`->`** é como se acessa um campo **através de um ponteiro**. `raiz` é um ponteiro, então usamos `raiz->pai`. Se fosse a struct direto, seria `raiz.pai`.

### 19.2 `criar_no`

```c
NoArvore *novo = calloc(1, sizeof(NoArvore));
if (novo == NULL) {
    printf("Erro fatal: acabou a memória do computador.\n");
    exit(EXIT_FAILURE);
}
```

- **`calloc(1, sizeof(NoArvore))`** pede memória para 1 nó **e zera tudo**. **Por que `calloc` e não `malloc`?** O `malloc` devolve memória com lixo. Com `calloc`, todos os ponteiros já começam como `NULL` (zero) e todos os números como 0, então não esquecemos de inicializar `primeiro_filho`, `proximo_irmao` etc. Um ponteiro com lixo, quando seguido, faz o programa travar (*segmentation fault*).
- **`sizeof(NoArvore)`**: o tamanho em bytes de um nó. O compilador calcula, não precisamos saber o número.
- **Checar `NULL`**: se o computador não tiver memória, o `calloc` devolve `NULL`. Usar esse ponteiro travaria o programa, então paramos com uma mensagem.

```c
novo->fcb.numero_inode = proximo_numero_inode;
proximo_numero_inode = proximo_numero_inode + 1;
```

- `proximo_numero_inode` é uma variável `static` do arquivo que começa em 1 e só cresce, então **dois arquivos nunca têm o mesmo inode**, nem se um for apagado.
- **`static` fora de função** = a variável só é visível dentro de `arvore.c`. Ninguém de fora consegue mexer no contador por acidente.
- `novo->fcb.numero_inode`: `novo` é ponteiro (então `->`), e `fcb` é uma struct dentro do nó (então `.`).

```c
strncpy(novo->fcb.nome, nome, TAMANHO_MAXIMO_NOME - 1);
```

- **Por que `strncpy` e não `strcpy`?** O `strcpy` copia até o fim do texto, sem olhar o tamanho do destino. Um nome gigante passaria do vetor de 32 posições e corromperia a memória vizinha (*buffer overflow*). O `strncpy` copia **no máximo** 31 caracteres.
- **Por que `- 1`?** Para sobrar a última posição para o `'\0'`. Como a memória veio zerada do `calloc`, essa posição já é `'\0'`.

```c
if (tipo == TIPO_DIRETORIO) {
    novo->fcb.permissoes = PERMISSOES_PADRAO_DE_DIRETORIO;
} else {
    novo->fcb.permissoes = PERMISSOES_PADRAO_DE_ARQUIVO;
}
novo->fcb.id_do_dono  = usuario_atual.id_do_usuario;
novo->fcb.id_do_grupo = usuario_atual.id_do_grupo;
```

Pasta nasce com 755, arquivo com 644, e **quem cria vira o dono**. Por isso, na demo, a alice é dona do que ela cria.

### 19.3 `adicionar_filho`

```c
filho->pai           = pasta;
filho->proximo_irmao = pasta->primeiro_filho;
pasta->primeiro_filho = filho;
```

Insere o filho **no começo** da lista. **A ordem das duas últimas linhas importa:**

```
antes:     pasta ─▶ A ─▶ B

linha 2:   filho ─▶ A          (o novo aponta para quem era o primeiro)
linha 3:   pasta ─▶ filho      (a pasta passa a apontar para o novo)

depois:    pasta ─▶ filho ─▶ A ─▶ B
```

Se trocássemos a ordem (primeiro `pasta->primeiro_filho = filho`), perderíamos a seta para `A`, e a lista inteira de filhos antigos sumiria (vazamento e arquivos perdidos). Inserir no começo é **O(1)**: não importa quantos filhos a pasta tem, são sempre 3 linhas.

### 19.4 `remover_filho_da_pasta`

```c
if (pasta->primeiro_filho == filho) {
    pasta->primeiro_filho = filho->proximo_irmao;
}
else {
    NoArvore *anterior = pasta->primeiro_filho;
    while (anterior != NULL && anterior->proximo_irmao != filho) {
        anterior = anterior->proximo_irmao;
    }
    if (anterior != NULL) {
        anterior->proximo_irmao = filho->proximo_irmao;
    }
}
```

Tirar um item de uma lista encadeada é "fazer o anterior pular ele":

```
antes:    A ─▶ filho ─▶ C
depois:   A ──────────▶ C
```

- **Caso 1:** o filho é o primeiro, então a pasta passa a apontar para o segundo.
- **Caso 2:** é preciso **achar o anterior**, porque a lista só anda para frente (não tem seta "voltar"). O `while` caminha até achar o nó cujo `proximo_irmao` é o filho.
- **`anterior != NULL &&` vem primeiro no `while`**: o `&&` em C para no primeiro falso. Se `anterior` for `NULL`, ele nem tenta ler `anterior->proximo_irmao`, o que travaria o programa.

Essa função **não libera memória**: só desliga o nó da árvore. Assim o `mv` pode reaproveitá-la (tira de uma pasta e coloca em outra), e o `rm` chama o `free` depois.

### 19.5 `procurar_filho_pelo_nome`

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

- **`strcmp`** compara dois textos e devolve **0 quando são iguais**. Isso confunde muita gente: `== 0` quer dizer "iguais". **Por que não usar `nome == "."`?** Porque em C isso compara os **endereços** dos textos, e não as letras.
- `.` e `..` são tratados aqui, num lugar só, e por isso funcionam em qualquer comando (`cd ..`, `ls ..`, `cp a.txt ..`).
- O `while` percorre a lista de irmãos: é a busca **O(n)** dentro da pasta. Devolver `NULL` é a forma de dizer "não achei".

### 19.6 `encontrar_pelo_caminho`

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

Esta é a função que transforma `/home/alice/notas.txt` no nó do arquivo.

- **Por que copiar o caminho?** Porque o `strtok` **modifica o texto** que recebe: ele troca cada `/` por `'\0'`. O caminho original veio de quem chamou e é `const` (não pode ser alterado), então trabalhamos numa cópia. A linha que coloca `'\0'` no fim garante que a cópia termina, mesmo que o caminho seja enorme (o `strncpy` não coloca o `'\0'` quando corta o texto).
- **Absoluto ou relativo:** se começa com `/`, parte da raiz; senão, da pasta atual. É só isso que diferencia os dois.
- **`strtok`** corta o texto em pedaços. A primeira chamada recebe o texto, e as seguintes recebem `NULL` para continuar de onde parou:

```
"home/alice/notas.txt"
 strtok(texto, "/") → "home"
 strtok(NULL,  "/") → "alice"
 strtok(NULL,  "/") → "notas.txt"
 strtok(NULL,  "/") → NULL (acabou)
```

- A cada pedaço, **desce um nível** com `procurar_filho_pelo_nome`. Se algum pedaço não existe, devolve `NULL` na hora.
- Um efeito bom: barras repetidas (`/home//alice/`) funcionam, porque o `strtok` pula separadores seguidos.

### 19.7 `separar_pasta_e_nome`

Usada para **criar** coisas: `echo oi > docs/novo.txt` precisa achar a pasta `docs` e saber que o nome novo é `novo.txt`. Não dá para usar `encontrar_pelo_caminho("docs/novo.txt")`, porque o arquivo ainda não existe.

```c
const char *ultima_barra = strrchr(caminho, '/');
```

- **`strrchr`** acha a **última** ocorrência de um caractere (o `r` vem de *reverse*). Tudo antes dela é a pasta, e tudo depois é o nome.
- Três casos: sem barra (`novo.txt` → pasta atual), barra só no começo (`/novo.txt` → raiz), e barra no meio (`docs/novo.txt` → pasta `docs`).

```c
size_t tamanho_do_trecho = (size_t)(ultima_barra - caminho);
memcpy(caminho_da_pasta, caminho, tamanho_do_trecho);
caminho_da_pasta[tamanho_do_trecho] = '\0';
```

- **`ultima_barra - caminho`**: subtrair dois ponteiros do mesmo texto dá a **distância** entre eles, ou seja, quantos caracteres existem antes da barra. Isso se chama aritmética de ponteiros.
- `memcpy` copia exatamente esse trecho, e o `'\0'` termina o texto.
- A função devolve a pasta "por fora", usando um **ponteiro para ponteiro** (`NoArvore **pasta_encontrada`). **Por que `**`?** Uma função em C só devolve um valor com `return`, e o nosso `return` já é usado para dizer se deu certo (`true`/`false`). Para devolver também a pasta, quem chama passa o **endereço** da sua variável (`&pasta`), e a função escreve nesse endereço com `*pasta_encontrada = pasta`.

### 19.8 `esta_dentro_de`

```c
NoArvore *atual = possivel_descendente;
while (atual != NULL) {
    if (atual == pasta) { return true; }
    if (atual == raiz)  { return false; }
    atual = atual->pai;
}
```

Sobe de pai em pai, a partir de um nó: se encontrar a `pasta` no caminho, é porque está dentro dela. **Por que parar na raiz?** Porque o pai da raiz é ela mesma: sem esse `if`, o loop nunca terminaria. É usada no `mv` (para não mover uma pasta para dentro dela mesma) e no `rmdir` (para não apagar a pasta onde você está).

### 19.9 `montar_caminho_completo`

```c
NoArvore *atual = no;
while (atual != raiz && quantidade < TAMANHO_MAXIMO_CAMINHO) {
    nos_do_caminho[quantidade] = atual;
    quantidade = quantidade + 1;
    atual = atual->pai;
}

destino[0] = '\0';
for (int i = quantidade - 1; i >= 0; i--) {
    strncat(destino, "/", tamanho_destino - strlen(destino) - 1);
    strncat(destino, nos_do_caminho[i]->fcb.nome, tamanho_destino - strlen(destino) - 1);
}
```

- Subindo pelos pais, os nomes aparecem **ao contrário** (`notas.txt`, `alice`, `home`). Então guardamos os nós num vetor e depois escrevemos **de trás para frente**.
- **`strncat`** gruda um texto no fim de outro. A conta `tamanho_destino - strlen(destino) - 1` é "quanto espaço ainda sobra", para nunca escrever além do vetor.
- `destino[0] = '\0'` começa com um texto vazio, porque o `strncat` precisa de um texto válido para grudar.

### 19.10 `liberar_no_e_todos_os_filhos`

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

- É **recursiva**: para liberar uma pasta, primeiro libera cada filho (e cada filho libera os filhos dele...). Só no fim libera o próprio nó. Isso é o "de baixo para cima".
- **A linha mais importante é `NoArvore *proximo = filho->proximo_irmao;`**: guardamos o próximo **antes** de liberar o filho. Depois do `free(filho)`, aquela memória não é mais nossa, e ler `filho->proximo_irmao` seria usar memória já liberada (*use-after-free*), um erro grave que o valgrind acusa.
- `liberar_todos_os_blocos_do_arquivo` devolve os blocos ao disco **antes** do `free`, porque depois não teríamos mais a lista dos blocos.
- `&no->fcb`: o `&` pega o **endereço** do FCB, porque a função espera um ponteiro (ela modifica o FCB).

## 20. `permissoes.c` — a parte com bits

### 20.1 `descobrir_posicao_do_trio`

```c
if (usuario->id_do_usuario == fcb->id_do_dono) {
    return POSICAO_DO_TRIO_DONO;
}
if (usuario->id_do_grupo == fcb->id_do_grupo) {
    return POSICAO_DO_TRIO_GRUPO;
}
return POSICAO_DO_TRIO_OUTROS;
```

A ordem dos `if` é a regra do Linux: **dono primeiro, depois grupo, depois outros**, e vale só o primeiro que bater.

### 20.2 `extrair_trio`

```c
int deslocado = permissoes >> posicao_do_trio;
int so_os_tres_ultimos_bits = deslocado & 7;
return so_os_tres_ultimos_bits;
```

São as duas operações de bits mais importantes do trabalho:

- **`>>` (deslocar para a direita)**: empurra todos os bits para a direita, e os da ponta direita caem fora. `>> 3` traz o trio do grupo para a ponta.
- **`& 7` (E bit a bit com 7)**: 7 em binário é `111`. Um bit do resultado só é 1 quando é 1 **nos dois lados**, então o `& 7` apaga todos os bits menos os 3 últimos. Por isso o 7 é chamado de **máscara**: ele "tapa" o que não interessa.

```
permissoes = 0754 = 111 101 100
>> 3              =     111 101
& 7  (000 111)    =         101   → 5 = r-x  (trio do grupo)
```

### 20.3 `usuario_tem_permissao`

```c
if (usuario->id_do_usuario == 0) {
    return true;
}
int posicao = descobrir_posicao_do_trio(fcb, usuario);
int trio    = extrair_trio(fcb->permissoes, posicao);
bool tem_o_bit = (trio & permissao_desejada) != 0;
return tem_o_bit;
```

- O root (id 0) passa direto.
- **`trio & permissao_desejada`**: o mesmo `&`, agora para testar **um** bit. Se o bit pedido está ligado no trio, o resultado é diferente de zero.

```
trio = 101 (r-x)
pedindo escrita (2 = 010):   101 & 010 = 000 → 0 → NEGADO
pedindo leitura (4 = 100):   101 & 100 = 100 → 4 → PERMITIDO
```

- **Os parâmetros são `const FCB *` e `const Usuario *`**: o `const` promete que a função só **lê**, sem modificar. O compilador dá erro se alguém tentar alterar sem querer. **Por que passar ponteiro e não a struct?** Uma struct FCB é grande (tem vetores dentro), e passá-la por valor copiaria tudo a cada chamada. O ponteiro é só um endereço.

Essa é **a** função do bloqueio obrigatório: todo comando que precisa de permissão passa por ela.

### 20.4 `montar_texto_de_permissoes` e `escrever_trio_como_texto`

```c
if (trio & PERMISSAO_DE_LEITURA) {
    destino[0] = 'r';
} else {
    destino[0] = '-';
}
```

Para cada bit do trio, escreve a letra ou `-`. A função principal chama isso três vezes, escrevendo nas posições 1, 4 e 7 do texto (`&destino[1]` é o endereço da posição 1). A posição 0 recebe `d` ou `-`, e a posição 10 recebe o `'\0'`. **Por que o vetor tem 11 posições?** São 10 caracteres visíveis (`drwxr-xr-x`) + 1 do `'\0'`.

### 20.5 `comando_chmod`

```c
int digito_dono   = modo[0] - '0';
int digito_grupo  = modo[1] - '0';
int digito_outros = modo[2] - '0';

int permissoes = (digito_dono   << POSICAO_DO_TRIO_DONO)
               | (digito_grupo  << POSICAO_DO_TRIO_GRUPO)
               | (digito_outros << POSICAO_DO_TRIO_OUTROS);
```

- **`modo[0] - '0'`**: o texto `"754"` é feito de **caracteres**, e o caractere `'7'` não é o número 7 (na tabela ASCII ele vale 55). Como os dígitos são seguidos na tabela (`'0'` = 48, `'1'` = 49, ..., `'7'` = 55), subtrair `'0'` dá o número certo: 55 − 48 = 7.
- **`<<` (deslocar para a esquerda)**: o contrário do `>>`, coloca cada dígito na posição do seu trio.
- **`|` (OU bit a bit)**: junta os três, porque um bit é 1 se for 1 em **qualquer** lado. Como os trios não se sobrepõem, o OU funciona como uma "colagem".

```
7 << 6 = 111 000 000
5 << 3 =     101 000
4 << 0 =         100
          ───────────  |
          111 101 100 = 0754
```

Antes disso, a função faz duas verificações:
- **`modo_e_valido`**: exatamente 3 caracteres, cada um de `'0'` a `'7'`. Sem isso, `chmod 9 a.txt` ou `chmod 888 a.txt` gerariam permissões malucas.
- **Dono ou root:** `if (!e_root && !e_dono)` bloqueia. Lendo em português: "se não é root **e** não é dono, nega".

## 21. `disco.c` — o disco em blocos

### 21.1 As duas variáveis

```c
static char disco[QUANTIDADE_DE_BLOCOS_NO_DISCO * TAMANHO_DO_BLOCO];
static bool bloco_esta_ocupado[QUANTIDADE_DE_BLOCOS_NO_DISCO];
```

- **`disco`** é um vetor de 16.384 bytes (`char` = 1 byte), e é o "char[] para representar o disco" que o enunciado sugere. O bloco N começa na posição `N * 64`.
- **`bloco_esta_ocupado`** é o mapa de livres: `true` = ocupado.
- **`static`** de novo: só o `disco.c` mexe no disco diretamente. Os outros arquivos usam as funções (`gravar_conteudo_no_disco`, `ler_conteudo_do_disco`). Isso é **encapsulamento**: se um dia mudarmos o jeito de guardar, só este arquivo muda.
- **Por que não é `malloc`?** O disco tem tamanho fixo e existe do começo ao fim do programa. Um vetor estático é mais simples e não tem como vazar.

### 21.2 `reservar_bloco_livre` (o first-fit)

```c
for (int numero_do_bloco = 0; numero_do_bloco < QUANTIDADE_DE_BLOCOS_NO_DISCO; numero_do_bloco++) {
    if (!bloco_esta_ocupado[numero_do_bloco]) {
        bloco_esta_ocupado[numero_do_bloco] = true;
        return numero_do_bloco;
    }
}
return -1;
```

Anda do bloco 0 em diante e pega o **primeiro** livre: isso é o **first-fit**. Marca como ocupado **antes** de devolver, para que a próxima chamada não pegue o mesmo bloco. O `-1` significa "disco cheio" (nenhum bloco válido é negativo).

### 21.3 `calcular_blocos_necessarios` (divisão arredondando para cima)

```c
int blocos_cheios = (int)(tamanho / TAMANHO_DO_BLOCO);
int sobrou_um_pedaco = (tamanho % TAMANHO_DO_BLOCO) != 0;
if (sobrou_um_pedaco) {
    return blocos_cheios + 1;
}
return blocos_cheios;
```

- A divisão de inteiros em C **corta** a parte decimal: 150 / 64 = 2, e não 2,34.
- **`%`** é o **resto** da divisão: 150 % 64 = 22. Se sobrou resto, os 22 bytes precisam de mais um bloco.
- Resultado: 150 bytes → 2 + 1 = **3 blocos**. 128 bytes → 2 blocos (resto 0, cabe certinho).

### 21.4 `gravar_conteudo_no_disco` (o coração do objetivo 3.4)

```c
int blocos_necessarios = calcular_blocos_necessarios(tamanho);

if (blocos_necessarios > MAXIMO_DE_BLOCOS_POR_ARQUIVO) {
    return ERRO_ARQUIVO_GRANDE_DEMAIS;
}
int blocos_disponiveis = contar_blocos_livres() + fcb->quantidade_de_blocos;
if (blocos_necessarios > blocos_disponiveis) {
    return ERRO_DISCO_CHEIO;
}

liberar_todos_os_blocos_do_arquivo(fcb);
```

- **As verificações vêm ANTES de liberar os blocos antigos.** Se liberássemos primeiro e depois descobríssemos que não cabe, o arquivo perderia o conteúdo antigo **e** não ganharia o novo. Assim, ou a gravação dá certo inteira, ou nada muda.
- **`+ fcb->quantidade_de_blocos`**: os blocos que o arquivo já tem vão ser devolvidos, então contam como disponíveis. Sem isso, reescrever um arquivo num disco quase cheio daria "disco cheio" à toa.
- O retorno é um **enum** (`ResultadoDaGravacao`) e não `true`/`false`, porque existem **dois** erros diferentes e cada um tem a sua mensagem.

```c
size_t bytes_ja_gravados = 0;
for (int i = 0; i < blocos_necessarios; i++) {
    int numero_do_bloco = reservar_bloco_livre();

    size_t bytes_que_faltam = tamanho - bytes_ja_gravados;
    size_t bytes_deste_pedaco = TAMANHO_DO_BLOCO;
    if (bytes_que_faltam < TAMANHO_DO_BLOCO) {
        bytes_deste_pedaco = bytes_que_faltam;
    }

    char *inicio_do_bloco = &disco[numero_do_bloco * TAMANHO_DO_BLOCO];
    memcpy(inicio_do_bloco, conteudo + bytes_ja_gravados, bytes_deste_pedaco);

    fcb->blocos_do_arquivo[i] = numero_do_bloco;
    bytes_ja_gravados = bytes_ja_gravados + bytes_deste_pedaco;
}
```

- **`bytes_deste_pedaco`**: normalmente 64, mas o último pedaço pode ser menor (no exemplo de 150 bytes, o último tem 22).
- **`&disco[numero_do_bloco * TAMANHO_DO_BLOCO]`**: o endereço onde começa o bloco. O bloco 9 começa no byte 9 × 64 = 576 do vetor.
- **`conteudo + bytes_ja_gravados`**: aritmética de ponteiros de novo. É o endereço do pedaço atual dentro do texto (o 1º pedaço começa no byte 0, o 2º no byte 64...).
- **`memcpy(destino, origem, quantidade)`** copia bytes de um lugar para outro. **Por que não `strcpy`?** Porque `strcpy` para no primeiro `'\0'` e não respeita o limite de 64. O `memcpy` copia exatamente a quantidade pedida.
- **`fcb->blocos_do_arquivo[i] = numero_do_bloco`**: é aqui que o índice é preenchido, e é a **linha que materializa a alocação indexada**.

### 21.5 `ler_conteudo_do_disco`

É o mesmo laço ao contrário: para cada bloco da lista, copia do disco para o `destino`. Duas coisas diferentes:

```c
if (bytes_ja_lidos + bytes_deste_pedaco > tamanho_destino - 1) {
    break;
}
...
destino[bytes_ja_lidos] = '\0';
```

- **Proteção:** nunca escreve além do tamanho do vetor de destino, guardando 1 posição para o `'\0'`.
- **O `'\0'` no final** transforma o conteúdo num texto C válido, para o `printf("%s", ...)` saber onde parar. O disco não guarda esse `'\0'`: o tamanho real está no FCB.

### 21.6 `liberar_bloco` e `liberar_todos_os_blocos_do_arquivo`

```c
bloco_esta_ocupado[numero_do_bloco] = false;
memset(inicio_do_bloco, 0, TAMANHO_DO_BLOCO);
```

Marca como livre e **zera** o bloco com `memset` (preenche N bytes com um valor). Zerar não é obrigatório (o Linux não zera), mas deixa o disco limpo e evita "restos" de arquivos apagados. A função do arquivo inteiro chama `liberar_bloco` para cada número do índice e zera o contador e o tamanho.

## 22. `comandos_arquivo.c` — os comandos de arquivo

### 22.1 O padrão que todo comando segue

Todos os comandos têm a mesma forma, e isso facilita muito a leitura:

```
1. procurar o alvo         → se não achar: mensagem e return
2. checar se é do tipo certo (arquivo ou pasta) → senão: mensagem e return
3. checar a permissão      → se não puder: "permissão negada" e return
4. fazer a operação
5. atualizar as datas
```

Esse estilo se chama **retorno antecipado**: cada erro sai da função na hora, então o "caminho feliz" fica no fim, sem `if` dentro de `if`. É o que atende o requisito 5.3 do enunciado (tratamento de erros com mensagens claras).

### 22.2 `encontrar_arquivo` (função de apoio)

Procura pelo caminho e já trata os dois erros mais comuns ("não encontrado" e "é um diretório"). Recebe o nome do comando só para a mensagem sair certinha (`cat: 'x': arquivo não encontrado`). Sem ela, essas 8 linhas estariam repetidas em `cat`, `cp` e `./`.

### 22.3 `comando_echo_para_arquivo`

```c
NoArvore *arquivo = encontrar_pelo_caminho(caminho);
if (arquivo == NULL) {
    arquivo = criar_arquivo_vazio("echo", caminho);
    if (arquivo == NULL) {
        return;
    }
} else {
    ... é diretório? tem permissão de escrita? ...
}
```

Se o arquivo não existe, é criado (igual ao Linux). Se já existe, precisa ter `w` **no arquivo**.

```c
char conteudo_novo[TAMANHO_MAXIMO_DO_ARQUIVO + 1];
size_t tamanho_atual = 0;

if (adicionar_no_final) {
    tamanho_atual = ler_conteudo_do_disco(&arquivo->fcb, conteudo_novo, sizeof(conteudo_novo));
}
```

- Um vetor local com espaço para o maior arquivo possível + 1 (para o `'\0'`). **Por que não `malloc`?** Porque o tamanho máximo é conhecido e pequeno (1025 bytes). Um vetor local some sozinho quando a função termina, então não tem como vazar.
- **Com `>>`**, o vetor começa com o conteúdo antigo. **Com `>`**, começa vazio (tamanho 0), e é assim que o `>` **trunca** o arquivo.

```c
size_t tamanho_final = tamanho_atual + tamanho_do_texto + 1;   /* +1 do "\n" */
if (tamanho_final > TAMANHO_MAXIMO_DO_ARQUIVO) { ... erro ... }

memcpy(conteudo_novo + tamanho_atual, texto, tamanho_do_texto);
conteudo_novo[tamanho_atual + tamanho_do_texto] = '\n';
conteudo_novo[tamanho_final] = '\0';
```

- O tamanho é conferido **antes** de copiar, para não estourar o vetor.
- O texto novo é colado **depois** do que já tinha (`conteudo_novo + tamanho_atual`), e depois vem o `'\n'` que o echo sempre põe.

Por fim: grava no disco, descobre o tipo (`.sh` = programa, só números = numérico...) e atualiza as datas.

### 22.4 `comando_rm`

```c
NoArvore *pasta = arquivo->pai;
if (!usuario_tem_permissao(&pasta->fcb, &usuario_atual, PERMISSAO_DE_ESCRITA)) { ... }

remover_filho_da_pasta(pasta, arquivo);
liberar_no_e_todos_os_filhos(arquivo);
```

- A permissão é checada **na pasta** (`arquivo->pai`), e não no arquivo: a regra do Linux explicada na Parte 1.
- **A ordem importa:** primeiro desliga da árvore, depois libera. Se liberasse antes, a pasta continuaria apontando para uma memória que não existe mais (um "ponteiro pendurado"), e o próximo `ls` travaria o programa.

### 22.5 `comando_cp`

O ponto mais interessante é a variável `criou_arquivo_novo`:

```c
if (resultado != GRAVOU_COM_SUCESSO) {
    mostrar_erro_de_gravacao("cp", resultado);
    if (criou_arquivo_novo) {
        remover_filho_da_pasta(pasta_destino, arquivo_destino);
        liberar_no_e_todos_os_filhos(arquivo_destino);
    }
    return;
}
```

Se a cópia criou um arquivo novo e a gravação falhou (disco cheio), o arquivo vazio recém-criado é apagado, para não deixar "lixo" para trás. Se o destino já existia, ele é mantido. A variável lembra qual dos dois casos aconteceu.

Outra coisa importante: a cópia chama `criar_no` (**inode novo**) e `gravar_conteudo_no_disco` (**blocos novos**). É isso que o `stat` mostra na demo.

### 22.6 `decidir_destino` (para cp e mv)

Se o destino é uma pasta que já existe (`cp a.txt docs`), o item vai para dentro dela com o mesmo nome. Senão, o destino é tratado como "caminho + nome novo" (`cp a.txt docs/b.txt`). É o mesmo comportamento do Linux.

### 22.7 `comando_mv`

```c
if (esta_dentro_de(pasta_destino, item)) {
    printf("mv: não é possível mover uma pasta para dentro dela mesma\n");
    return;
}
...
NoArvore *pasta_origem = item->pai;
bool pode_tirar   = usuario_tem_permissao(&pasta_origem->fcb,  &usuario_atual, PERMISSAO_DE_ESCRITA);
bool pode_colocar = usuario_tem_permissao(&pasta_destino->fcb, &usuario_atual, PERMISSAO_DE_ESCRITA);
...
remover_filho_da_pasta(pasta_origem, item);
strncpy(item->fcb.nome, nome_destino, TAMANHO_MAXIMO_NOME - 1);
adicionar_filho(pasta_destino, item);
```

- **A checagem de laço** usa `esta_dentro_de`: se a pasta de destino está dentro do item (ou é o próprio item), bloqueia.
- **Duas permissões:** escrever na pasta de onde sai **e** na pasta para onde vai.
- **`pasta_origem` é guardada antes** porque `remover_filho_da_pasta` zera o `item->pai`. Depois dessa chamada não daria mais para saber de onde ele veio.
- **As três últimas linhas são o `mv` inteiro:** desliga, troca o nome, religa. Nenhum byte de conteúdo é copiado, e o inode e os blocos continuam os mesmos.

### 22.8 `comando_executar`

Só confere a permissão de **execução** (`x`) e, se tiver, mostra o conteúdo como "saída" do programa. Não roda nada de verdade: existe para demonstrar que o `x` é verificado, como o enunciado pede ("ler, escrever, executar").

## 23. `comandos_diretorio.c` — os detalhes que valem lembrar

- **`comando_cd`** exige permissão `x` na pasta de destino (`PERMISSAO_DE_EXECUCAO`). No Linux, `x` em pasta significa "poder entrar".
- **`comando_ls`** exige `r` na pasta. Se você der `ls` num arquivo, ele mostra só aquele arquivo, igual ao Linux.
- **`comando_mkdir`** exige `w` na pasta atual e confere se o nome é válido e se não existe.
- **`comando_rmdir`** confere quatro coisas antes de apagar: existe? é pasta? não é a raiz nem a pasta atual (ou uma acima dela)? está vazia? E ainda exige `w` na pasta de cima.
- **`imprimir_linha_detalhada`** (o `ls -l`) usa `localtime` (transforma o `time_t` em dia, mês, hora...) e `strftime` (formata como texto, `"%d/%m %H:%M"` → `14/11 20:36`). O `%zu` no `printf` é o formato certo para `size_t`.

## 24. `terminal.c` — de texto digitado a comando

### 24.1 O loop principal

```c
bool entrada_e_o_teclado = isatty(fileno(stdin));

while (true) {
    mostrar_prompt();
    if (fgets(linha, sizeof(linha), stdin) == NULL) {
        printf("\n");
        break;
    }
    if (!entrada_e_o_teclado) {
        printf("%s", linha);
        ...
    }
    bool continuar = executar_linha(linha);
    if (!continuar) {
        break;
    }
}
```

- **`fgets(linha, tamanho, stdin)`** lê uma linha do teclado com **limite de tamanho** (512), então nunca estoura o vetor. Devolve `NULL` quando a entrada acaba (Ctrl+D, ou fim do arquivo de roteiro), e aí saímos do loop normalmente, liberando a memória.
- **`isatty(fileno(stdin))`** pergunta "a entrada é um teclado de verdade?". Quando rodamos `./minifs < roteiro.txt`, a resposta é não, e então imprimimos o comando depois do prompt, para a saída ficar igual a alguém digitando. Para usar o `isatty` com `-std=c11`, o arquivo começa com `#define _POSIX_C_SOURCE 200809L`, que libera as funções do padrão POSIX (Linux/Mac).
- `executar_linha` devolve `false` só no `exit`.

### 24.2 `executar_linha`

```c
linha[strcspn(linha, "\n")] = '\0';
```

O `fgets` guarda o Enter (`'\n'`) no fim da linha. O **`strcspn`** devolve a posição do primeiro `'\n'`, e colocamos `'\0'` ali, cortando o Enter. Sem isso, `cd home` procuraria uma pasta chamada `"home\n"`.

```c
bool comeca_com_echo_espaco = (strncmp(linha, "echo ", 5) == 0);
if (comeca_com_echo_espaco) {
    tratar_echo(linha + 5);
    return true;
}
```

**Por que o `echo` é tratado ANTES de separar as palavras?** Porque o texto do echo tem espaços (`echo ola mundo > a.txt`). Se separássemos tudo em palavras primeiro, perderíamos os espaços originais do texto. Então o echo recebe a linha "crua". O `strncmp(..., 5)` compara só os 5 primeiros caracteres, e `linha + 5` é o texto logo depois de `"echo "`.

```c
bool comeca_com_ponto_barra = (strncmp(comando, "./", 2) == 0);
if (comeca_com_ponto_barra) {
    comando_executar(comando + 2);
    return true;
}
```

`./script.sh` → `comando + 2` é `"script.sh"`.

O resto é uma sequência de `if / else if` comparando o nome do comando com `strcmp` e chamando a função certa. Para comandos que exigem argumentos, `tem_argumentos_suficientes` mostra o "uso:" quando falta algo (por exemplo, `cp` sozinho mostra `uso: cp <origem> <destino>`).

### 24.3 `tratar_echo`

```c
posicao_do_sinal = strstr(texto_depois_do_echo, ">>");
if (posicao_do_sinal != NULL) {
    adicionar_no_final = true;
    tamanho_do_sinal = 2;
} else {
    posicao_do_sinal = strchr(texto_depois_do_echo, '>');
    ...
}
```

- **Procura `>>` primeiro** porque o `>` aparece dentro do `>>`. Se procurasse `>` primeiro, todo `>>` seria confundido com `>`.
- **`strstr`** acha um texto dentro de outro, e **`strchr`** acha um caractere.
- Depois, `*posicao_do_sinal = '\0'` corta a linha em duas: o que vem antes do sinal é o texto, e o que vem depois é o nome do arquivo. Um `'\0'` no meio de uma string faz o C achar que ela acabou ali, o que é um truque clássico.

### 24.4 Usuários de teste e `su`

```c
static const Usuario usuarios_cadastrados[] = {
    {  0,    0,     "root"  },
    {  1000, 100,   "alice" },
    {  1001, 100,   "bob"   },
    {  2000, 200,   "guest" },
};
```

Um vetor de structs inicializado direto no código. `static const` = só este arquivo vê, e ninguém altera. O `su` percorre o vetor e, se achar o nome, copia a struct inteira para `usuario_atual`. Em C, atribuir uma struct a outra (`usuario_atual = usuarios_cadastrados[i];`) copia todos os campos de uma vez.

## 25. Fluxo completo de `echo oi > notas.txt` (diagrama)

```
rodar_terminal()
   │  fgets → "echo oi > notas.txt\n"
   ▼
executar_linha()
   │  corta o "\n"
   │  começa com "echo "? sim
   ▼
tratar_echo("oi > notas.txt")
   │  procura ">>": não tem
   │  procura ">": achou → adicionar_no_final = false
   │  corta: texto = "oi", arquivo = "notas.txt"
   ▼
comando_echo_para_arquivo("notas.txt", "oi", false)
   │
   ├─ encontrar_pelo_caminho("notas.txt") → NULL (não existe)
   │     └─ criar_arquivo_vazio()
   │           ├─ separar_pasta_e_nome → pasta atual, "notas.txt"
   │           ├─ nome_e_valido? sim
   │           ├─ já existe? não
   │           ├─ usuario_tem_permissao(pasta, ESCRITA)? sim
   │           └─ criar_no() + adicionar_filho()
   │
   ├─ monta conteúdo: "oi" + "\n" = 3 bytes
   │
   ├─ gravar_conteudo_no_disco(fcb, "oi\n", 3)
   │     ├─ calcular_blocos_necessarios(3) = 1
   │     ├─ cabe? (1 ≤ 16, há blocos livres) sim
   │     ├─ liberar blocos antigos (nenhum)
   │     └─ reservar_bloco_livre() → 0
   │           memcpy → disco[0..2] = 'o','i','\n'
   │           fcb.blocos_do_arquivo = {0}
   │
   └─ tipo = texto, datas = agora
```

## 26. Glossário rápido das funções do C usadas

| Função | O que faz | Onde aparece |
|--------|-----------|--------------|
| `calloc(n, tam)` | pede memória para n itens e zera tudo | `criar_no` |
| `free(p)` | devolve a memória | `liberar_no_e_todos_os_filhos` |
| `strcmp(a, b)` | compara textos; **0 = iguais** | buscas e comandos |
| `strncmp(a, b, n)` | compara só os n primeiros caracteres | detectar `echo ` e `./` |
| `strncpy(dest, orig, n)` | copia texto com limite | nomes |
| `strncat(dest, orig, n)` | gruda um texto no fim de outro, com limite | `montar_caminho_completo` |
| `strlen(s)` | tamanho do texto (sem o `'\0'`) | validações |
| `strchr(s, c)` / `strrchr(s, c)` | primeira / última ocorrência de um caractere | `>` no echo / última `/` |
| `strstr(s, t)` | acha um texto dentro de outro | `>>` no echo |
| `strtok(s, sep)` | corta o texto em pedaços (modifica o original) | caminhos e palavras |
| `strcspn(s, set)` | posição do primeiro caractere do conjunto | cortar o `\n` |
| `memcpy(dest, orig, n)` | copia exatamente n bytes | gravar/ler blocos |
| `memset(p, v, n)` | preenche n bytes com o valor v | zerar disco e blocos |
| `time(NULL)` | data/hora atual em `time_t` | datas do FCB |
| `localtime` + `strftime` | converte `time_t` em texto formatado | `ls -l`, `stat` |
| `fgets(buf, n, stdin)` | lê uma linha com limite | terminal |
| `isatty(fileno(stdin))` | a entrada é o teclado? | terminal |
| `printf("%zu")` / `("%04o")` | imprime `size_t` / octal com 4 dígitos | `ls -l` / `stat` |

## 27. Operadores de bits (resumo para revisar antes da apresentação)

| Operador | Nome | Exemplo | Para que usamos |
|----------|------|---------|-----------------|
| `>>` | desloca para a direita | `0754 >> 3` = `0075` | trazer um trio para a ponta |
| `<<` | desloca para a esquerda | `7 << 6` = `0700` | colocar um dígito no seu trio (chmod) |
| `&` | E bit a bit | `0075 & 7` = `5` | isolar um trio / testar um bit |
| `\|` | OU bit a bit | `0700 \| 0050 \| 0004` = `0754` | juntar os três trios (chmod) |

## 28. Perguntas técnicas que podem aparecer

**"Por que `calloc` e não `malloc`?"**
Porque o `calloc` zera a memória: todos os ponteiros começam como `NULL` e os números como 0. Com `malloc`, os ponteiros teriam lixo e o programa poderia travar ao segui-los.

**"Por que o `0` na frente de `0644`?"**
Em C, um número que começa com zero é octal. Permissões são octais (cada dígito = 3 bits). Sem o zero, 644 seria decimal, e os bits ficariam errados.

**"Por que guardar o `proximo` antes do `free` na liberação da árvore?"**
Porque depois do `free` aquela memória não pertence mais ao programa, e ler `filho->proximo_irmao` seria *use-after-free*.

**"Por que o `rm` desliga o nó da pasta antes de liberar?"**
Se liberasse primeiro, a pasta ficaria apontando para memória liberada (ponteiro pendurado), e o próximo `ls` travaria.

**"Por que a gravação confere o espaço antes de liberar os blocos antigos?"**
Para que, se não couber, o arquivo mantenha o conteúdo antigo. Ou dá certo inteiro, ou nada muda.

**"Por que copiar o caminho antes do `strtok`?"**
Porque o `strtok` troca as `/` por `'\0'` no texto original, e o caminho recebido é `const` (não pode ser alterado).

**"Para que serve o ponteiro duplo em `separar_pasta_e_nome`?"**
Para a função devolver a pasta "por fora", já que o `return` é usado para dizer se deu certo. Quem chama passa `&pasta`, e a função escreve `*pasta_encontrada = pasta`.

**"Por que `modo[0] - '0'`?"**
Para converter o caractere `'7'` no número 7. Os dígitos são seguidos na tabela ASCII, então subtrair o código do `'0'` dá o valor numérico.

**"Por que o echo é tratado antes de separar as palavras?"**
Porque o texto do echo tem espaços, e separar em palavras os perderia.

**"Por que `static` nas variáveis do disco e no contador de inode?"**
Para que só o próprio arquivo `.c` consiga mexer nelas. Os outros usam as funções. Isso evita alterações acidentais (encapsulamento).

**"Por que passar `const FCB *` em vez de `FCB`?"**
O ponteiro evita copiar a struct inteira (que é grande), e o `const` garante que a função só lê, sem modificar.
