/*
 * terminal.c
 *
 * O "prompt" do simulador: lê o que o usuário digita, descobre qual
 * comando é e chama a função certa.
 *
 * Os nomes dos COMANDOS continuam em inglês (mkdir, ls, cd...) porque
 * a ideia é imitar o terminal do Linux.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sistema_arquivos.h"


#define TAMANHO_MAXIMO_LINHA     512
#define MAXIMO_PALAVRAS_POR_LINHA 8


/* ---------------------------------------------------------------------
 * Usuários de teste
 *
 * Servem para demonstrar as 3 classes de permissão:
 *   - alice e bob estão no MESMO grupo (100)
 *   - guest está em outro grupo (200), então para ele vale "outros"
 * --------------------------------------------------------------------- */
static const Usuario usuarios_cadastrados[] = {
    /* id    grupo  nome   */
    {  0,    0,     "root"  },
    {  1000, 100,   "alice" },
    {  1001, 100,   "bob"   },
    {  2000, 200,   "guest" },
};
static const int quantidade_de_usuarios = 4;

/* Começamos como root */
Usuario usuario_atual = { 0, 0, "root" };


/* ---------------------------------------------------------------------
 * Comandos que só existem no terminal (não mexem na árvore)
 * --------------------------------------------------------------------- */

static void comando_help(void)
{
    printf("Comandos disponíveis:\n");
    printf("  mkdir <nome>             cria uma pasta\n");
    printf("  cd [caminho]             entra numa pasta (sem caminho volta para /)\n");
    printf("  ls [-l] [caminho]        lista o conteúdo (-l mostra detalhes)\n");
    printf("  pwd                      mostra onde você está\n");
    printf("  touch <arquivo>          cria um arquivo vazio\n");
    printf("  echo <texto> > <arquivo> escreve no arquivo (>> adiciona no final)\n");
    printf("  cat <arquivo>            mostra o conteúdo\n");
    printf("  cp <origem> <destino>    copia\n");
    printf("  mv <origem> <destino>    move ou renomeia\n");
    printf("  rm <arquivo>             apaga\n");
    printf("  stat <arquivo>           mostra o FCB (inode) do arquivo\n");
    printf("  chmod <modo> <arquivo>   muda permissões (ex.: chmod 754 a.txt)\n");
    printf("  disk                     mostra o mapa de blocos do disco\n");
    printf("  whoami                   mostra o usuário atual\n");
    printf("  su <usuario>             troca de usuário (root, alice, bob, guest)\n");
    printf("  help                     mostra esta ajuda\n");
    printf("  exit                     sai do simulador\n");
}

static void comando_whoami(void)
{
    printf("%s (id do usuário = %d, id do grupo = %d)\n",
           usuario_atual.nome,
           usuario_atual.id_do_usuario,
           usuario_atual.id_do_grupo);
}

static void comando_su(const char *nome_do_usuario)
{
    for (int i = 0; i < quantidade_de_usuarios; i++) {
        if (strcmp(usuarios_cadastrados[i].nome, nome_do_usuario) == 0) {
            usuario_atual = usuarios_cadastrados[i];
            return;
        }
    }
    printf("su: usuário '%s' não existe\n", nome_do_usuario);
}


/* ---------------------------------------------------------------------
 * echo (tratado separado porque o texto pode ter espaços)
 *
 *   echo ola mundo > arquivo.txt     -> sobrescreve
 *   echo mais texto >> arquivo.txt   -> adiciona no final
 *   echo ola mundo                   -> só imprime na tela
 * --------------------------------------------------------------------- */

/* Remove espaços e tabs do final de um texto. */
static void remover_espacos_do_final(char *texto)
{
    size_t tamanho = strlen(texto);
    while (tamanho > 0 && (texto[tamanho - 1] == ' ' || texto[tamanho - 1] == '\t')) {
        texto[tamanho - 1] = '\0';
        tamanho = tamanho - 1;
    }
}

static void tratar_echo(char *texto_depois_do_echo)
{
    bool adicionar_no_final;
    char *posicao_do_sinal;
    int tamanho_do_sinal;

    /* Procuramos primeiro ">>" porque ">" também apareceria dentro de ">>" */
    posicao_do_sinal = strstr(texto_depois_do_echo, ">>");
    if (posicao_do_sinal != NULL) {
        adicionar_no_final = true;
        tamanho_do_sinal = 2;
    } else {
        posicao_do_sinal = strchr(texto_depois_do_echo, '>');
        adicionar_no_final = false;
        tamanho_do_sinal = 1;
    }

    /* Sem ">" : igual ao Linux, só imprime o texto */
    if (posicao_do_sinal == NULL) {
        printf("%s\n", texto_depois_do_echo);
        return;
    }

    /* Cortamos a linha no sinal:  "ola mundo " | "> arquivo.txt" */
    *posicao_do_sinal = '\0';
    char *texto = texto_depois_do_echo;
    char *parte_do_arquivo = posicao_do_sinal + tamanho_do_sinal;

    char *nome_do_arquivo = strtok(parte_do_arquivo, " \t");
    if (nome_do_arquivo == NULL) {
        printf("echo: faltou o nome do arquivo depois do '>'\n");
        return;
    }

    remover_espacos_do_final(texto);
    comando_echo_para_arquivo(nome_do_arquivo, texto, adicionar_no_final);
}


/* ---------------------------------------------------------------------
 * Interpretação da linha digitada
 * --------------------------------------------------------------------- */

/*
 * Quebra a linha em palavras separadas por espaço.
 * Ex.: "cp a.txt b.txt" -> palavras = {"cp", "a.txt", "b.txt"}, retorna 3.
 */
static int separar_em_palavras(char *linha, char *palavras[])
{
    int quantidade = 0;
    char *palavra = strtok(linha, " \t");

    while (palavra != NULL && quantidade < MAXIMO_PALAVRAS_POR_LINHA) {
        palavras[quantidade] = palavra;
        quantidade = quantidade + 1;
        palavra = strtok(NULL, " \t");
    }
    return quantidade;
}

/*
 * Confere se o usuário passou argumentos suficientes.
 * Se não passou, mostra como se usa o comando.
 * (quantidade_palavras inclui o próprio nome do comando)
 */
static bool tem_argumentos_suficientes(int quantidade_palavras, int argumentos_necessarios, const char *como_usar)
{
    int argumentos_recebidos = quantidade_palavras - 1;
    if (argumentos_recebidos < argumentos_necessarios) {
        printf("uso: %s\n", como_usar);
        return false;
    }
    return true;
}

/* Trata o "ls", que tem argumentos opcionais: ls, ls -l, ls pasta, ls -l pasta */
static void tratar_ls(char *palavras[], int quantidade_palavras)
{
    bool formato_detalhado = false;
    const char *caminho = NULL;

    for (int i = 1; i < quantidade_palavras; i++) {
        if (strcmp(palavras[i], "-l") == 0) {
            formato_detalhado = true;
        } else {
            caminho = palavras[i];
        }
    }

    comando_ls(caminho, formato_detalhado);
}

/*
 * Executa uma linha digitada.
 * Retorna false quando o usuário quer sair (exit), true caso contrário.
 */
static bool executar_linha(char *linha)
{
    /* fgets deixa o "\n" (Enter) no fim da linha; tiramos ele */
    linha[strcspn(linha, "\n")] = '\0';

    /* Pula espaços no começo */
    while (*linha == ' ' || *linha == '\t') {
        linha++;
    }

    /* O echo precisa da linha inteira (com espaços), então tratamos antes */
    bool comeca_com_echo_espaco = (strncmp(linha, "echo ", 5) == 0);
    if (comeca_com_echo_espaco) {
        tratar_echo(linha + 5);
        return true;
    }
    if (strcmp(linha, "echo") == 0) {
        printf("\n");
        return true;
    }

    char *palavras[MAXIMO_PALAVRAS_POR_LINHA];
    int quantidade_palavras = separar_em_palavras(linha, palavras);

    /* Linha vazia: só mostra o prompt de novo */
    if (quantidade_palavras == 0) {
        return true;
    }

    const char *comando = palavras[0];

    if (strcmp(comando, "exit") == 0) {
        return false;
    }
    else if (strcmp(comando, "help") == 0) {
        comando_help();
    }
    else if (strcmp(comando, "pwd") == 0) {
        comando_pwd();
    }
    else if (strcmp(comando, "whoami") == 0) {
        comando_whoami();
    }
    else if (strcmp(comando, "disk") == 0) {
        comando_disk();
    }
    else if (strcmp(comando, "ls") == 0) {
        tratar_ls(palavras, quantidade_palavras);
    }
    else if (strcmp(comando, "cd") == 0) {
        if (quantidade_palavras >= 2) {
            comando_cd(palavras[1]);
        } else {
            comando_cd("/");
        }
    }
    else if (strcmp(comando, "mkdir") == 0) {
        if (tem_argumentos_suficientes(quantidade_palavras, 1, "mkdir <nome>")) {
            comando_mkdir(palavras[1]);
        }
    }
    else if (strcmp(comando, "touch") == 0) {
        if (tem_argumentos_suficientes(quantidade_palavras, 1, "touch <arquivo>")) {
            comando_touch(palavras[1]);
        }
    }
    else if (strcmp(comando, "cat") == 0) {
        if (tem_argumentos_suficientes(quantidade_palavras, 1, "cat <arquivo>")) {
            comando_cat(palavras[1]);
        }
    }
    else if (strcmp(comando, "rm") == 0) {
        if (tem_argumentos_suficientes(quantidade_palavras, 1, "rm <arquivo>")) {
            comando_rm(palavras[1]);
        }
    }
    else if (strcmp(comando, "stat") == 0) {
        if (tem_argumentos_suficientes(quantidade_palavras, 1, "stat <arquivo>")) {
            comando_stat(palavras[1]);
        }
    }
    else if (strcmp(comando, "su") == 0) {
        if (tem_argumentos_suficientes(quantidade_palavras, 1, "su <usuario>")) {
            comando_su(palavras[1]);
        }
    }
    else if (strcmp(comando, "cp") == 0) {
        if (tem_argumentos_suficientes(quantidade_palavras, 2, "cp <origem> <destino>")) {
            comando_cp(palavras[1], palavras[2]);
        }
    }
    else if (strcmp(comando, "mv") == 0) {
        if (tem_argumentos_suficientes(quantidade_palavras, 2, "mv <origem> <destino>")) {
            comando_mv(palavras[1], palavras[2]);
        }
    }
    else if (strcmp(comando, "chmod") == 0) {
        if (tem_argumentos_suficientes(quantidade_palavras, 2, "chmod <modo> <arquivo>")) {
            comando_chmod(palavras[1], palavras[2]);
        }
    }
    else {
        printf("%s: comando não encontrado (digite 'help')\n", comando);
    }

    return true;
}


/* ---------------------------------------------------------------------
 * Loop principal
 * --------------------------------------------------------------------- */

/* Mostra o prompt, ex.: "alice@minifs:/home/alice$ " */
static void mostrar_prompt(void)
{
    char caminho_atual[TAMANHO_MAXIMO_CAMINHO];
    montar_caminho_completo(diretorio_atual, caminho_atual, sizeof(caminho_atual));

    /* Igual ao Linux: root usa "#", usuário comum usa "$" */
    char simbolo;
    if (usuario_atual.id_do_usuario == 0) {
        simbolo = '#';
    } else {
        simbolo = '$';
    }

    printf("%s@minifs:%s%c ", usuario_atual.nome, caminho_atual, simbolo);
    fflush(stdout);
}

void rodar_terminal(void)
{
    char linha[TAMANHO_MAXIMO_LINHA];

    printf("Mini sistema de arquivos em memória. Digite 'help' para ver os comandos.\n");

    while (true) {
        mostrar_prompt();

        /* fgets retorna NULL quando a entrada acaba (Ctrl+D) */
        if (fgets(linha, sizeof(linha), stdin) == NULL) {
            printf("\n");
            break;
        }

        bool continuar = executar_linha(linha);
        if (!continuar) {
            break;
        }
    }
}
