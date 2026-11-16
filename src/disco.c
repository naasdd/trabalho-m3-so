/*
 * disco.c
 *
 * Objetivo 3.4 do enunciado: simular um disco dividido em blocos.
 *
 * O "disco" é só um vetor de bytes bem grande:
 *
 *   disco:   [ bloco 0 ][ bloco 1 ][ bloco 2 ] ... [ bloco 255 ]
 *              64 bytes   64 bytes   64 bytes        64 bytes
 *
 * Um segundo vetor (bloco_esta_ocupado) diz quais blocos já estão em uso.
 *
 * Método de alocação: INDEXADA.
 * Cada FCB guarda a lista dos números dos blocos que são dele.
 * Ex.: um arquivo de 150 bytes precisa de 3 blocos (64 + 64 + 22)
 *      e o FCB guarda algo como blocos_do_arquivo = {4, 9, 10}.
 */
#include <stdio.h>
#include <string.h>

#include "sistema_arquivos.h"


static char disco[QUANTIDADE_DE_BLOCOS_NO_DISCO * TAMANHO_DO_BLOCO];
static bool bloco_esta_ocupado[QUANTIDADE_DE_BLOCOS_NO_DISCO];


/* Zera o disco e marca todos os blocos como livres. */
void iniciar_disco(void)
{
    memset(disco, 0, sizeof(disco));

    for (int i = 0; i < QUANTIDADE_DE_BLOCOS_NO_DISCO; i++) {
        bloco_esta_ocupado[i] = false;
    }
}


/*
 * Procura o primeiro bloco livre, marca como ocupado e devolve o número dele.
 * Devolve -1 se o disco estiver cheio.
 */
int reservar_bloco_livre(void)
{
    for (int numero_do_bloco = 0; numero_do_bloco < QUANTIDADE_DE_BLOCOS_NO_DISCO; numero_do_bloco++) {
        if (!bloco_esta_ocupado[numero_do_bloco]) {
            bloco_esta_ocupado[numero_do_bloco] = true;
            return numero_do_bloco;
        }
    }
    return -1;
}


/* Marca o bloco como livre e apaga o que tinha nele. */
void liberar_bloco(int numero_do_bloco)
{
    bool numero_valido = (numero_do_bloco >= 0 && numero_do_bloco < QUANTIDADE_DE_BLOCOS_NO_DISCO);
    if (!numero_valido) {
        return;
    }

    bloco_esta_ocupado[numero_do_bloco] = false;

    char *inicio_do_bloco = &disco[numero_do_bloco * TAMANHO_DO_BLOCO];
    memset(inicio_do_bloco, 0, TAMANHO_DO_BLOCO);
}


/* Devolve ao disco todos os blocos que um arquivo estava usando. */
void liberar_todos_os_blocos_do_arquivo(FCB *fcb)
{
    for (int i = 0; i < fcb->quantidade_de_blocos; i++) {
        liberar_bloco(fcb->blocos_do_arquivo[i]);
    }
    fcb->quantidade_de_blocos = 0;
    fcb->tamanho_em_bytes = 0;
}


/* Conta quantos blocos ainda estão livres no disco. */
int contar_blocos_livres(void)
{
    int livres = 0;
    for (int numero_do_bloco = 0; numero_do_bloco < QUANTIDADE_DE_BLOCOS_NO_DISCO; numero_do_bloco++) {
        if (!bloco_esta_ocupado[numero_do_bloco]) {
            livres = livres + 1;
        }
    }
    return livres;
}


/* Quantos blocos são necessários para guardar 'tamanho' bytes?
 * É uma divisão arredondando para CIMA: 150 bytes / 64 = 2,3 -> 3 blocos. */
static int calcular_blocos_necessarios(size_t tamanho)
{
    int blocos_cheios = (int)(tamanho / TAMANHO_DO_BLOCO);
    int sobrou_um_pedaco = (tamanho % TAMANHO_DO_BLOCO) != 0;

    if (sobrou_um_pedaco) {
        return blocos_cheios + 1;
    }
    return blocos_cheios;
}


/*
 * Grava o conteúdo de um arquivo no disco (ALOCAÇÃO INDEXADA).
 *
 * Passo a passo:
 *   1. calcula quantos blocos vai precisar
 *   2. confere se cabe (limite por arquivo e espaço livre no disco)
 *   3. devolve os blocos antigos do arquivo (se ele já tinha conteúdo)
 *   4. para cada pedaço de 64 bytes: reserva um bloco livre,
 *      copia o pedaço para lá e anota o número do bloco no FCB
 *
 * Exemplo com 150 bytes:
 *   pedaço 1: bytes   0 a  63  -> bloco 4
 *   pedaço 2: bytes  64 a 127  -> bloco 9
 *   pedaço 3: bytes 128 a 149  -> bloco 10   (só 22 bytes usados)
 *   FCB.blocos_do_arquivo = {4, 9, 10}
 */
ResultadoDaGravacao gravar_conteudo_no_disco(FCB *fcb, const char *conteudo, size_t tamanho)
{
    /* Passo 1 */
    int blocos_necessarios = calcular_blocos_necessarios(tamanho);

    /* Passo 2: os blocos que o arquivo já tem também contam como
     * "disponíveis", porque vão ser devolvidos no passo 3. */
    if (blocos_necessarios > MAXIMO_DE_BLOCOS_POR_ARQUIVO) {
        return ERRO_ARQUIVO_GRANDE_DEMAIS;
    }
    int blocos_disponiveis = contar_blocos_livres() + fcb->quantidade_de_blocos;
    if (blocos_necessarios > blocos_disponiveis) {
        return ERRO_DISCO_CHEIO;
    }

    /* Passo 3 */
    liberar_todos_os_blocos_do_arquivo(fcb);

    /* Passo 4 */
    size_t bytes_ja_gravados = 0;
    for (int i = 0; i < blocos_necessarios; i++) {
        int numero_do_bloco = reservar_bloco_livre();

        /* O último pedaço pode ser menor que um bloco inteiro */
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

    fcb->quantidade_de_blocos = blocos_necessarios;
    fcb->tamanho_em_bytes = tamanho;
    return GRAVOU_COM_SUCESSO;
}


/*
 * Lê o conteúdo de um arquivo: o caminho inverso da gravação.
 * Percorre a lista de blocos do FCB, na ordem, e junta os pedaços.
 *
 * Coloca um '\0' no final para o resultado poder ser usado como texto.
 * Retorna quantos bytes foram lidos.
 */
size_t ler_conteudo_do_disco(const FCB *fcb, char *destino, size_t tamanho_destino)
{
    size_t bytes_ja_lidos = 0;

    for (int i = 0; i < fcb->quantidade_de_blocos; i++) {
        int numero_do_bloco = fcb->blocos_do_arquivo[i];

        size_t bytes_que_faltam = fcb->tamanho_em_bytes - bytes_ja_lidos;
        size_t bytes_deste_pedaco = TAMANHO_DO_BLOCO;
        if (bytes_que_faltam < TAMANHO_DO_BLOCO) {
            bytes_deste_pedaco = bytes_que_faltam;
        }

        /* Proteção: nunca escrever além do espaço que o destino tem
         * (o -1 guarda lugar para o '\0' do final) */
        if (bytes_ja_lidos + bytes_deste_pedaco > tamanho_destino - 1) {
            break;
        }

        char *inicio_do_bloco = &disco[numero_do_bloco * TAMANHO_DO_BLOCO];
        memcpy(destino + bytes_ja_lidos, inicio_do_bloco, bytes_deste_pedaco);
        bytes_ja_lidos = bytes_ja_lidos + bytes_deste_pedaco;
    }

    destino[bytes_ja_lidos] = '\0';
    return bytes_ja_lidos;
}


/*
 * disk : mostra o mapa do disco.
 *   '#' = bloco ocupado
 *   '.' = bloco livre
 * 32 blocos por linha; o número à esquerda é o primeiro bloco da linha.
 */
void comando_disk(void)
{
    int blocos_por_linha = 32;
    int total_ocupados = 0;

    for (int numero_do_bloco = 0; numero_do_bloco < QUANTIDADE_DE_BLOCOS_NO_DISCO; numero_do_bloco++) {

        bool comeco_de_linha = (numero_do_bloco % blocos_por_linha == 0);
        if (comeco_de_linha) {
            if (numero_do_bloco > 0) {
                printf("\n");
            }
            printf("%4d | ", numero_do_bloco);
        }

        if (bloco_esta_ocupado[numero_do_bloco]) {
            printf("#");
            total_ocupados = total_ocupados + 1;
        } else {
            printf(".");
        }
    }

    printf("\n%d de %d blocos ocupados (%d bytes por bloco)\n",
           total_ocupados, QUANTIDADE_DE_BLOCOS_NO_DISCO, TAMANHO_DO_BLOCO);
    printf("%d de %d posições da tabela de nós em uso (arquivos + pastas)\n",
           contar_nos_em_uso(), MAXIMO_DE_NOS);
}
