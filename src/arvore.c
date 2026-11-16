/*
 * arvore.c
 *
 * Funções que montam e percorrem a árvore de diretórios.
 * Aqui NÃO tem nenhum comando do terminal, só a estrutura em si.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sistema_arquivos.h"


NoArvore *raiz            = NULL;
NoArvore *diretorio_atual = NULL;

/* Contador para dar um número de inode diferente para cada arquivo novo. */
static int proximo_numero_inode = 1;


/* ---------------------------------------------------------------------
 * Criação e destruição do sistema de arquivos
 * --------------------------------------------------------------------- */

void iniciar_sistema_de_arquivos(void)
{
    iniciar_disco();

    raiz = criar_no("/", TIPO_DIRETORIO);

    /* O "pai" da raiz é a própria raiz. Assim "cd .." em "/" continua em "/". */
    raiz->pai = raiz;

    diretorio_atual = raiz;
}

/* Libera um nó e, antes dele, todos os filhos (recursivamente). */
void liberar_no_e_todos_os_filhos(NoArvore *no)
{
    if (no == NULL) {
        return;
    }

    NoArvore *filho = no->primeiro_filho;
    while (filho != NULL) {
        /* Guardamos o próximo ANTES de liberar o filho atual,
         * porque depois do free() não podemos mais ler filho->proximo_irmao. */
        NoArvore *proximo = filho->proximo_irmao;
        liberar_no_e_todos_os_filhos(filho);
        filho = proximo;
    }

    liberar_todos_os_blocos_do_arquivo(&no->fcb);
    free(no);
}

void encerrar_sistema_de_arquivos(void)
{
    liberar_no_e_todos_os_filhos(raiz);
    raiz = NULL;
    diretorio_atual = NULL;
}


/* ---------------------------------------------------------------------
 * Criação de nós
 * --------------------------------------------------------------------- */

/* Cria um nó novo (arquivo ou pasta) com o FCB preenchido. */
NoArvore *criar_no(const char *nome, TipoArquivo tipo)
{
    /* calloc já zera toda a memória, então todos os ponteiros começam NULL */
    NoArvore *novo = calloc(1, sizeof(NoArvore));
    if (novo == NULL) {
        printf("Erro fatal: acabou a memória do computador.\n");
        exit(EXIT_FAILURE);
    }

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

/* Coloca 'filho' dentro de 'pasta' (no começo da lista de filhos). */
void adicionar_filho(NoArvore *pasta, NoArvore *filho)
{
    filho->pai           = pasta;
    filho->proximo_irmao = pasta->primeiro_filho;
    pasta->primeiro_filho = filho;
}


/*
 * Tira 'filho' da lista de filhos de 'pasta' (sem liberar a memória).
 *
 * Como a lista só anda "para frente", precisamos achar quem vem
 * ANTES do filho e fazer ele pular o filho:
 *
 *   antes:   a -> filho -> c
 *   depois:  a ----------> c
 */
void remover_filho_da_pasta(NoArvore *pasta, NoArvore *filho)
{
    /* Caso 1: o filho é o primeiro da lista */
    if (pasta->primeiro_filho == filho) {
        pasta->primeiro_filho = filho->proximo_irmao;
    }
    /* Caso 2: o filho está no meio ou no fim */
    else {
        NoArvore *anterior = pasta->primeiro_filho;
        while (anterior != NULL && anterior->proximo_irmao != filho) {
            anterior = anterior->proximo_irmao;
        }
        if (anterior != NULL) {
            anterior->proximo_irmao = filho->proximo_irmao;
        }
    }

    filho->pai = NULL;
    filho->proximo_irmao = NULL;
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
        return pasta->pai;
    }

    /* Percorre a lista de filhos um por um */
    NoArvore *filho = pasta->primeiro_filho;
    while (filho != NULL) {
        if (strcmp(filho->fcb.nome, nome) == 0) {
            return filho;
        }
        filho = filho->proximo_irmao;
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
        atual = atual->pai;
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
        atual = atual->pai;
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
