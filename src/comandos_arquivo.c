/*
 * comandos_arquivo.c
 *
 * Objetivo 3.2 do enunciado: criar, escrever, ler, copiar, mover e apagar
 * arquivos, tudo dentro da árvore simulada (nada no disco real).
 *
 * TODO: implementar juntos. Por enquanto cada comando só avisa que falta.
 */
#include <stdio.h>

#include "sistema_arquivos.h"


static void avisar_que_falta_implementar(const char *nome_do_comando)
{
    printf("%s: ainda não implementado\n", nome_do_comando);
}


/* touch <nome> : cria um arquivo vazio */
void comando_touch(const char *nome)
{
    (void)nome;  /* "(void)" só evita o aviso de variável não usada */
    avisar_que_falta_implementar("touch");
}

/* echo <texto> > <nome>  ou  echo <texto> >> <nome> */
void comando_echo_para_arquivo(const char *nome, const char *texto, bool adicionar_no_final)
{
    (void)nome;
    (void)texto;
    (void)adicionar_no_final;
    avisar_que_falta_implementar("echo");
}

/* cat <nome> : mostra o conteúdo do arquivo */
void comando_cat(const char *nome)
{
    (void)nome;
    avisar_que_falta_implementar("cat");
}

/* cp <origem> <destino> : copia um arquivo */
void comando_cp(const char *origem, const char *destino)
{
    (void)origem;
    (void)destino;
    avisar_que_falta_implementar("cp");
}

/* mv <origem> <destino> : move ou renomeia */
void comando_mv(const char *origem, const char *destino)
{
    (void)origem;
    (void)destino;
    avisar_que_falta_implementar("mv");
}

/* rm <nome> : apaga um arquivo */
void comando_rm(const char *nome)
{
    (void)nome;
    avisar_que_falta_implementar("rm");
}

/* stat <nome> : mostra todos os campos do FCB */
void comando_stat(const char *nome)
{
    (void)nome;
    avisar_que_falta_implementar("stat");
}
