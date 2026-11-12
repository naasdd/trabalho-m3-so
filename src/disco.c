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


/* TODO (3.4): quebrar o conteúdo em pedaços de 64 bytes,
 * reservar um bloco para cada pedaço e anotar os números no FCB. */
bool gravar_conteudo_no_disco(FCB *fcb, const char *conteudo, size_t tamanho)
{
    (void)fcb;
    (void)conteudo;
    (void)tamanho;
    return false;
}


/* TODO (3.4): percorrer os blocos do FCB e juntar o conteúdo em 'destino'. */
size_t ler_conteudo_do_disco(const FCB *fcb, char *destino, size_t tamanho_destino)
{
    (void)fcb;
    (void)destino;
    (void)tamanho_destino;
    return 0;
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
}
