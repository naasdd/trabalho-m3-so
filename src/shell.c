/*
 * shell.c - Interpretador de comandos interativo (REPL) do simulador.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"
#include "shell.h"

#define MAX_LINE 512
#define MAX_ARGS 8

/* Usuários pré-cadastrados para testar permissões com 'su'. */
static const User users[] = {
    { 0,    0,   "root"  },
    { 1000, 100, "alice" },
    { 1001, 100, "bob"   },   /* mesmo grupo da alice */
    { 2000, 200, "guest" },   /* "outros" */
};
#define NUM_USERS (int)(sizeof users / sizeof users[0])

static void cmd_help(void)
{
    puts("Comandos disponíveis:\n"
         "  mkdir <nome>            cria diretório\n"
         "  cd [caminho]            muda de diretório\n"
         "  ls [-l] [caminho]       lista conteúdo\n"
         "  pwd                     mostra diretório atual\n"
         "  touch <arq>             cria arquivo vazio\n"
         "  echo <texto> > <arq>    escreve (>> para anexar)\n"
         "  cat <arq>               lê conteúdo\n"
         "  cp <orig> <dest>        copia\n"
         "  mv <orig> <dest>        move/renomeia\n"
         "  rm <arq>                remove\n"
         "  stat <arq>              mostra o FCB/inode\n"
         "  chmod <modo> <arq>      altera permissões (ex.: 754)\n"
         "  disk                    mapa de blocos do disco\n"
         "  whoami / su <usuario>   usuário atual (root, alice, bob, guest)\n"
         "  help / exit");
}

static void cmd_su(const char *name)
{
    for (int i = 0; i < NUM_USERS; i++) {
        if (strcmp(users[i].name, name) == 0) {
            current_user = users[i];
            return;
        }
    }
    printf("su: usuário '%s' não existe\n", name);
}

/*
 * echo é tratado à parte porque o texto pode ter espaços:
 *   echo ola mundo > arq.txt
 *   echo mais texto >> arq.txt
 */
static void cmd_echo(char *rest)
{
    bool append = false;
    char *redir = strstr(rest, ">>");
    if (redir) append = true;
    else redir = strchr(rest, '>');

    if (!redir) {                 /* sem redirecionamento: só imprime */
        printf("%s\n", rest);
        return;
    }
    *redir = '\0';
    char *file = redir + (append ? 2 : 1);
    file = strtok(file, " \t");
    if (!file) {
        printf("echo: faltou o nome do arquivo\n");
        return;
    }
    /* tira espaços do fim do texto */
    size_t len = strlen(rest);
    while (len && (rest[len - 1] == ' ' || rest[len - 1] == '\t'))
        rest[--len] = '\0';

    fs_write(file, rest, append);
}

/* Confere a quantidade de argumentos; imprime o uso se estiver errado. */
static bool need(int argc, int n, const char *usage)
{
    if (argc - 1 < n) {
        printf("uso: %s\n", usage);
        return false;
    }
    return true;
}

/* Executa uma linha. Retorna false quando o usuário pede para sair. */
static bool execute(char *line)
{
    line[strcspn(line, "\n")] = '\0';

    /* echo precisa da linha "crua" (antes do strtok) */
    while (*line == ' ' || *line == '\t') line++;
    if (strncmp(line, "echo", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
        cmd_echo(line + 4 + (line[4] == ' '));
        return true;
    }

    char *argv[MAX_ARGS];
    int argc = 0;
    for (char *t = strtok(line, " \t"); t && argc < MAX_ARGS; t = strtok(NULL, " \t"))
        argv[argc++] = t;
    if (argc == 0) return true;

    const char *cmd = argv[0];

    if      (!strcmp(cmd, "exit") || !strcmp(cmd, "quit")) return false;
    else if (!strcmp(cmd, "help"))   cmd_help();
    else if (!strcmp(cmd, "pwd"))    fs_pwd();
    else if (!strcmp(cmd, "whoami")) printf("%s (uid=%d gid=%d)\n", current_user.name,
                                            current_user.uid, current_user.gid);
    else if (!strcmp(cmd, "disk"))   disk_print_map();
    else if (!strcmp(cmd, "cd"))     fs_cd(argc > 1 ? argv[1] : "/");
    else if (!strcmp(cmd, "ls")) {
        bool lf = argc > 1 && !strcmp(argv[1], "-l");
        const char *path = argc > 1 + lf ? argv[1 + lf] : NULL;
        fs_ls(path, lf);
    }
    else if (!strcmp(cmd, "mkdir")) { if (need(argc, 1, "mkdir <nome>"))        fs_mkdir(argv[1]); }
    else if (!strcmp(cmd, "touch")) { if (need(argc, 1, "touch <arq>"))         fs_touch(argv[1]); }
    else if (!strcmp(cmd, "cat"))   { if (need(argc, 1, "cat <arq>"))           fs_cat(argv[1]); }
    else if (!strcmp(cmd, "rm"))    { if (need(argc, 1, "rm <arq>"))            fs_rm(argv[1]); }
    else if (!strcmp(cmd, "stat"))  { if (need(argc, 1, "stat <arq>"))          fs_stat(argv[1]); }
    else if (!strcmp(cmd, "su"))    { if (need(argc, 1, "su <usuario>"))        cmd_su(argv[1]); }
    else if (!strcmp(cmd, "cp"))    { if (need(argc, 2, "cp <orig> <dest>"))    fs_cp(argv[1], argv[2]); }
    else if (!strcmp(cmd, "mv"))    { if (need(argc, 2, "mv <orig> <dest>"))    fs_mv(argv[1], argv[2]); }
    else if (!strcmp(cmd, "chmod")) { if (need(argc, 2, "chmod <modo> <arq>"))  fs_chmod(argv[1], argv[2]); }
    else printf("%s: comando não encontrado (digite 'help')\n", cmd);

    return true;
}

void shell_run(void)
{
    char line[MAX_LINE];
    char path[MAX_PATH];

    printf("Mini-FS em memória. Digite 'help' para ver os comandos.\n");
    for (;;) {
        fs_get_path(fs_cwd, path, sizeof path);
        printf("%s@minifs:%s%c ", current_user.name, path,
               current_user.uid == 0 ? '#' : '$');
        fflush(stdout);

        if (!fgets(line, sizeof line, stdin)) {  /* Ctrl+D ou fim do arquivo */
            putchar('\n');
            break;
        }
        if (!execute(line)) break;
    }
}
