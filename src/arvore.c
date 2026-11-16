/*
 * arvore.c
 *
 * Funções que montam e percorrem a árvore de diretórios.
 * Aqui NÃO tem nenhum comando do terminal, só a estrutura em si.
 *
 * A árvore é guardada num ARRAY DE STRUCTS (tabela_de_nos): cada
 * posição é um arquivo ou pasta, e cada nó anota o índice da sua
 * pasta-mãe. Ver o desenho em sistema_arquivos.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sistema_arquivos.h"


/*
 * A TABELA DE NÓS: um array de structs com MAXIMO_DE_NOS posições.
 * Todo arquivo e toda pasta do sistema mora numa posição deste vetor.
 * Ela é alocada uma única vez (calloc) no começo e liberada (free) no fim.
 */
NoArvore *tabela_de_nos   = NULL;
NoArvore *raiz            = NULL;
NoArvore *diretorio_atual = NULL;

/* A raiz sempre ocupa a primeira posição do vetor. */
#define INDICE_DA_RAIZ 0

/* Contador para dar um número de inode diferente para cada arquivo novo. */
static int proximo_numero_inode = 1;


/* ---------------------------------------------------------------------
 * Criação e destruição do sistema de arquivos
 * --------------------------------------------------------------------- */

void iniciar_sistema_de_arquivos(void)
{
    iniciar_disco();

    /* calloc zera o vetor inteiro: todas as posições começam com em_uso = false */
    tabela_de_nos = calloc(MAXIMO_DE_NOS, sizeof(NoArvore));
    if (tabela_de_nos == NULL) {
        printf("Erro fatal: acabou a memória do computador.\n");
        exit(EXIT_FAILURE);
    }

    /* O primeiro nó criado cai na posição 0: essa é a raiz */
    raiz = criar_no("/", TIPO_DIRETORIO);

    /* O "pai" da raiz é a própria raiz. Assim "cd .." em "/" continua em "/". */
    raiz->indice_do_pai = INDICE_DA_RAIZ;

    diretorio_atual = raiz;
}

/*
 * Apaga um nó e, antes dele, todos os filhos (recursivamente).
 * "Apagar" aqui = devolver os blocos do disco e marcar a posição
 * do vetor como livre, para ser reaproveitada por outro arquivo.
 */
void liberar_no_e_todos_os_filhos(NoArvore *no)
{
    if (no == NULL || !no->em_uso) {
        return;
    }

    /* Procura no vetor todos os nós que são filhos deste */
    for (int i = 0; i < MAXIMO_DE_NOS; i++) {
        if (e_filho_de(&tabela_de_nos[i], no)) {
            liberar_no_e_todos_os_filhos(&tabela_de_nos[i]);
        }
    }

    liberar_todos_os_blocos_do_arquivo(&no->fcb);

    /* Zera a posição inteira: em_uso volta a ser false */
    memset(no, 0, sizeof(NoArvore));
}

void encerrar_sistema_de_arquivos(void)
{
    liberar_no_e_todos_os_filhos(raiz);

    /* Um único free para o único calloc do vetor */
    free(tabela_de_nos);
    tabela_de_nos   = NULL;
    raiz            = NULL;
    diretorio_atual = NULL;
}


/* ---------------------------------------------------------------------
 * Funções básicas do vetor
 * --------------------------------------------------------------------- */

/*
 * Em qual posição do vetor este nó está?
 * Subtrair dois ponteiros do mesmo vetor dá a distância entre eles
 * (em quantidade de elementos), que é justamente o índice.
 */
int indice_do_no(NoArvore *no)
{
    return (int)(no - tabela_de_nos);
}

/* Devolve a pasta onde o nó está (a "pasta de cima"). */
NoArvore *pasta_de_cima(NoArvore *no)
{
    return &tabela_de_nos[no->indice_do_pai];
}

/* Responde: 'no' é um filho direto de 'pasta'? */
bool e_filho_de(NoArvore *no, NoArvore *pasta)
{
    if (!no->em_uso) {
        return false;            /* posição livre não é filho de ninguém */
    }
    if (no == pasta) {
        return false;            /* a raiz é "pai de si mesma", mas não é filha */
    }
    return no->indice_do_pai == indice_do_no(pasta);
}

/* Uma pasta está vazia se nenhum nó do vetor aponta para ela como pai. */
bool pasta_esta_vazia(NoArvore *pasta)
{
    for (int i = 0; i < MAXIMO_DE_NOS; i++) {
        if (e_filho_de(&tabela_de_nos[i], pasta)) {
            return false;
        }
    }
    return true;
}

/* Quantas posições do vetor estão ocupadas (usado pelo comando disk). */
int contar_nos_em_uso(void)
{
    int quantidade = 0;
    for (int i = 0; i < MAXIMO_DE_NOS; i++) {
        if (tabela_de_nos[i].em_uso) {
            quantidade = quantidade + 1;
        }
    }
    return quantidade;
}


/* ---------------------------------------------------------------------
 * Criação de nós
 * --------------------------------------------------------------------- */

/*
 * Cria um nó novo (arquivo ou pasta) com o FCB preenchido.
 *
 * Procura a PRIMEIRA posição livre do vetor e usa ela.
 * Retorna NULL se o vetor estiver cheio (MAXIMO_DE_NOS itens).
 */
NoArvore *criar_no(const char *nome, TipoArquivo tipo)
{
    NoArvore *novo = NULL;
    for (int i = 0; i < MAXIMO_DE_NOS; i++) {
        if (!tabela_de_nos[i].em_uso) {
            novo = &tabela_de_nos[i];
            break;
        }
    }
    if (novo == NULL) {
        return NULL;   /* não sobrou nenhuma posição livre */
    }

    /* Começa do zero, para não sobrar lixo de um arquivo apagado antes */
    memset(novo, 0, sizeof(NoArvore));
    novo->em_uso = true;

    time_t agora = time(NULL);

    novo->fcb.numero_inode = proximo_numero_inode;
    proximo_numero_inode = proximo_numero_inode + 1;

    strncpy(novo->fcb.nome, nome, TAMANHO_MAXIMO_NOME - 1);
    novo->fcb.tipo             = tipo;
    novo->fcb.tamanho_em_bytes = 0;

    novo->fcb.data_de_criacao       = agora;
    novo->fcb.data_de_modificacao   = agora;
    novo->fcb.data_do_ultimo_acesso = agora;

    if (tipo == TIPO_DIRETORIO) {
        novo->fcb.permissoes = PERMISSOES_PADRAO_DE_DIRETORIO;
    } else {
        novo->fcb.permissoes = PERMISSOES_PADRAO_DE_ARQUIVO;
    }

    /* Quem cria o arquivo vira o dono dele */
    novo->fcb.id_do_dono  = usuario_atual.id_do_usuario;
    novo->fcb.id_do_grupo = usuario_atual.id_do_grupo;

    novo->fcb.quantidade_de_blocos = 0;

    return novo;
}

/*
 * Coloca 'filho' dentro de 'pasta'.
 * Basta anotar no filho o índice da pasta. Isso serve tanto para um nó
 * recém-criado quanto para MOVER um nó que já estava em outra pasta (mv).
 */
void adicionar_filho(NoArvore *pasta, NoArvore *filho)
{
    filho->indice_do_pai = indice_do_no(pasta);
}


/* ---------------------------------------------------------------------
 * Busca
 * --------------------------------------------------------------------- */

bool e_diretorio(NoArvore *no)
{
    return no->fcb.tipo == TIPO_DIRETORIO;
}

/* Procura um filho com esse nome dentro da pasta. Retorna NULL se não achar. */
NoArvore *procurar_filho_pelo_nome(NoArvore *pasta, const char *nome)
{
    if (pasta == NULL || !e_diretorio(pasta)) {
        return NULL;
    }

    /* "." significa "a própria pasta" */
    if (strcmp(nome, ".") == 0) {
        return pasta;
    }

    /* ".." significa "a pasta de cima" */
    if (strcmp(nome, "..") == 0) {
        return pasta_de_cima(pasta);
    }

    /* Percorre o vetor inteiro procurando um filho desta pasta com esse nome */
    for (int i = 0; i < MAXIMO_DE_NOS; i++) {
        NoArvore *candidato = &tabela_de_nos[i];
        if (e_filho_de(candidato, pasta) && strcmp(candidato->fcb.nome, nome) == 0) {
            return candidato;
        }
    }

    return NULL;
}

/*
 * Transforma um caminho em texto no nó correspondente.
 *
 * Funciona com:
 *   - caminho absoluto (começa com "/"):  "/home/alice"
 *   - caminho relativo (a partir de onde estou):  "alice/docs" ou "../bob"
 *
 * Retorna NULL se alguma parte do caminho não existir.
 */
NoArvore *encontrar_pelo_caminho(const char *caminho)
{
    /* Sem caminho = pasta atual */
    if (caminho == NULL || strlen(caminho) == 0) {
        return diretorio_atual;
    }

    /* Copiamos o caminho porque strtok() modifica o texto original */
    char copia_do_caminho[TAMANHO_MAXIMO_CAMINHO];
    strncpy(copia_do_caminho, caminho, TAMANHO_MAXIMO_CAMINHO - 1);
    copia_do_caminho[TAMANHO_MAXIMO_CAMINHO - 1] = '\0';

    /* De onde começamos a andar? */
    NoArvore *posicao;
    if (copia_do_caminho[0] == '/') {
        posicao = raiz;
    } else {
        posicao = diretorio_atual;
    }

    /* strtok separa o caminho em pedaços usando "/" como separador.
     * Ex.: "home/alice/docs" vira "home", depois "alice", depois "docs". */
    char *pedaco = strtok(copia_do_caminho, "/");
    while (pedaco != NULL) {
        posicao = procurar_filho_pelo_nome(posicao, pedaco);
        if (posicao == NULL) {
            return NULL;
        }
        pedaco = strtok(NULL, "/");
    }

    return posicao;
}

/*
 * Separa um caminho em "pasta onde fica" + "nome do item".
 * Serve para criar coisas em caminhos que AINDA NÃO existem.
 *
 *   "a.txt"            -> pasta atual,  nome "a.txt"
 *   "docs/a.txt"       -> pasta "docs", nome "a.txt"
 *   "/home/alice/a.txt"-> pasta "/home/alice", nome "a.txt"
 *
 * Retorna false se a pasta não existir ou não for um diretório.
 */
bool separar_pasta_e_nome(const char *caminho, NoArvore **pasta_encontrada, char *nome_encontrado)
{
    /* strrchr acha a ÚLTIMA "/" do caminho */
    const char *ultima_barra = strrchr(caminho, '/');

    NoArvore   *pasta;
    const char *trecho_do_nome;   /* aponta para onde o nome começa dentro do caminho */

    if (ultima_barra == NULL) {
        /* Não tem "/": é só um nome, dentro da pasta atual */
        pasta = diretorio_atual;
        trecho_do_nome = caminho;
    } else if (ultima_barra == caminho) {
        /* A única "/" é a do começo: ex. "/a.txt", fica na raiz */
        pasta = raiz;
        trecho_do_nome = ultima_barra + 1;
    } else {
        /* Copia tudo ANTES da última "/" para achar a pasta */
        char caminho_da_pasta[TAMANHO_MAXIMO_CAMINHO];
        size_t tamanho_do_trecho = (size_t)(ultima_barra - caminho);
        if (tamanho_do_trecho >= TAMANHO_MAXIMO_CAMINHO) {
            return false;
        }
        memcpy(caminho_da_pasta, caminho, tamanho_do_trecho);
        caminho_da_pasta[tamanho_do_trecho] = '\0';

        pasta = encontrar_pelo_caminho(caminho_da_pasta);
        trecho_do_nome = ultima_barra + 1;
    }

    /*
     * Nome comprido demais: NÃO cortamos escondido (o usuário ia criar
     * um arquivo com nome diferente do que digitou). Devolvemos um nome
     * vazio, e quem chamou vai recusar com "nome inválido".
     */
    if (strlen(trecho_do_nome) >= TAMANHO_MAXIMO_NOME) {
        nome_encontrado[0] = '\0';
    } else {
        strcpy(nome_encontrado, trecho_do_nome);
    }

    if (pasta == NULL || !e_diretorio(pasta)) {
        return false;
    }
    *pasta_encontrada = pasta;
    return true;
}

/*
 * Responde: 'possivel_descendente' está dentro de 'pasta' (em qualquer nível)?
 * Usado para impedir absurdos como mover uma pasta para dentro dela mesma.
 */
bool esta_dentro_de(NoArvore *possivel_descendente, NoArvore *pasta)
{
    NoArvore *atual = possivel_descendente;
    while (atual != NULL) {
        if (atual == pasta) {
            return true;
        }
        if (atual == raiz) {
            return false;   /* chegou no topo sem encontrar */
        }
        atual = pasta_de_cima(atual);
    }
    return false;
}

/*
 * Monta o caminho completo de um nó, ex.: "/home/alice".
 *
 * Ideia: subimos do nó até a raiz guardando cada nó numa lista,
 * depois escrevemos os nomes de trás para frente.
 */
void montar_caminho_completo(NoArvore *no, char *destino, size_t tamanho_destino)
{
    if (no == raiz) {
        snprintf(destino, tamanho_destino, "/");
        return;
    }

    /* Passo 1: subir até a raiz guardando o caminho */
    NoArvore *nos_do_caminho[TAMANHO_MAXIMO_CAMINHO];
    int quantidade = 0;

    NoArvore *atual = no;
    while (atual != raiz && quantidade < TAMANHO_MAXIMO_CAMINHO) {
        nos_do_caminho[quantidade] = atual;
        quantidade = quantidade + 1;
        atual = pasta_de_cima(atual);
    }

    /* Passo 2: escrever do mais alto (fim da lista) até o mais baixo */
    destino[0] = '\0';
    for (int i = quantidade - 1; i >= 0; i--) {
        strncat(destino, "/", tamanho_destino - strlen(destino) - 1);
        strncat(destino, nos_do_caminho[i]->fcb.nome, tamanho_destino - strlen(destino) - 1);
    }
}

/*
 * Um nome é válido se:
 *   - não está vazio
 *   - cabe no tamanho máximo
 *   - não tem "/" (porque "/" separa pastas)
 *   - não é "." nem ".." (são nomes reservados)
 */
bool nome_e_valido(const char *nome)
{
    if (nome == NULL || strlen(nome) == 0) {
        return false;
    }
    if (strlen(nome) >= TAMANHO_MAXIMO_NOME) {
        return false;
    }
    if (strchr(nome, '/') != NULL) {
        return false;
    }
    if (strcmp(nome, ".") == 0 || strcmp(nome, "..") == 0) {
        return false;
    }
    return true;
}
