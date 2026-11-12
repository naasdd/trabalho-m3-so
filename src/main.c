/*
 * main.c - Ponto de entrada do mini-sistema de arquivos em memória.
 */
#include "fs.h"
#include "shell.h"

int main(void)
{
    fs_init();      /* cria a raiz "/" e zera o disco simulado */
    shell_run();    /* loop de comandos até 'exit' ou Ctrl+D   */
    fs_destroy();   /* libera toda a árvore (sem memory leak)  */
    return 0;
}
