/*
 * disk.c - Disco simulado em blocos + alocação indexada.
 *
 * O "disco" é um único array de bytes dividido em NUM_BLOCKS blocos
 * de BLOCK_SIZE bytes. Um bitmap marca quais blocos estão livres.
 * Cada FCB guarda a lista dos blocos que contêm seu conteúdo
 * (FCB.blocks[]), que faz o papel do bloco de índice.
 */
#include <stdio.h>
#include <string.h>

#include "fs.h"

static char disk[NUM_BLOCKS * BLOCK_SIZE];  /* o "disco" */
static bool block_used[NUM_BLOCKS];         /* bitmap de blocos livres */

void disk_init(void)
{
    memset(disk, 0, sizeof disk);
    memset(block_used, 0, sizeof block_used);
}

/* Procura o primeiro bloco livre (first-fit). Retorna -1 se o disco encheu. */
int disk_alloc_block(void)
{
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (!block_used[i]) {
            block_used[i] = true;
            return i;
        }
    }
    return -1;
}

void disk_free_block(int block)
{
    if (block >= 0 && block < NUM_BLOCKS) {
        block_used[block] = false;
        memset(&disk[block * BLOCK_SIZE], 0, BLOCK_SIZE);
    }
}

/* Devolve ao disco todos os blocos de um arquivo. */
void disk_free_file(FCB *fcb)
{
    for (int i = 0; i < fcb->num_blocks; i++)
        disk_free_block(fcb->blocks[i]);
    fcb->num_blocks = 0;
    fcb->size = 0;
}

/* TODO (3.4): dividir 'data' em blocos, alocar e registrar no FCB. */
int disk_write_file(FCB *fcb, const char *data, size_t len)
{
    (void)fcb; (void)data; (void)len;
    return -1;
}

/* TODO (3.4): juntar os blocos do FCB de volta em 'out'. */
size_t disk_read_file(const FCB *fcb, char *out, size_t out_size)
{
    (void)fcb; (void)out; (void)out_size;
    return 0;
}

/* Mostra o mapa do disco: '#' = ocupado, '.' = livre. */
void disk_print_map(void)
{
    int used = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (i % 32 == 0) printf("%s%4d | ", i ? "\n" : "", i);
        putchar(block_used[i] ? '#' : '.');
        used += block_used[i];
    }
    printf("\n%d/%d blocos usados (%d bytes por bloco)\n",
           used, NUM_BLOCKS, BLOCK_SIZE);
}
