/*
 * main.c
 *
 * Ponto de entrada do programa. Só faz três coisas, em ordem:
 *   1. monta o sistema de arquivos (pasta "/" e disco vazio)
 *   2. abre o terminal e espera os comandos
 *   3. quando o usuário digita "exit", libera toda a memória
 */
#include "sistema_arquivos.h"

int main(void)
{
    iniciar_sistema_de_arquivos();
    rodar_terminal();
    encerrar_sistema_de_arquivos();
    return 0;
}
