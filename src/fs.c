/*
 * fs.c - Árvore de diretórios e operações sobre arquivos.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"

Node *fs_root = NULL;
Node *fs_cwd  = NULL;
User  current_user = { 0, 0, "root" };

static int next_inode_id = 1;  /* gerador de IDs únicos (inodes) */

/* ---------------------------------------------------------------
 * Helpers internos
 * --------------------------------------------------------------- */

/* Aloca e inicializa um nó novo (FCB com metadados padrão). */
static Node *node_new(const char *name, FileType type)
{
    Node *n = calloc(1, sizeof(Node));
    if (!n) {
        fprintf(stderr, "erro: sem memória\n");
        exit(EXIT_FAILURE);
    }
    time_t now = time(NULL);

    n->fcb.inode_id  = next_inode_id++;
    strncpy(n->fcb.name, name, MAX_NAME - 1);
    n->fcb.type      = type;
    n->fcb.size      = 0;
    n->fcb.created   = now;
    n->fcb.modified  = now;
    n->fcb.accessed  = now;
    n->fcb.perms     = (type == FT_DIRECTORY) ? DEFAULT_DIR_PERMS
                                              : DEFAULT_FILE_PERMS;
    n->fcb.owner_uid = current_user.uid;
    n->fcb.group_gid = current_user.gid;
    n->fcb.num_blocks = 0;
    return n;
}

/* Insere 'child' como filho de 'dir' (no início da lista de irmãos). */
static void node_add_child(Node *dir, Node *child)
{
    child->parent       = dir;
    child->next_sibling = dir->first_child;
    dir->first_child    = child;
}

/* Libera recursivamente um nó e toda a subárvore abaixo dele. */
static void node_free_recursive(Node *n)
{
    if (!n) return;
    Node *c = n->first_child;
    while (c) {
        Node *next = c->next_sibling;
        node_free_recursive(c);
        c = next;
    }
    disk_free_file(&n->fcb);   /* devolve os blocos do disco */
    free(n);
}

/* ---------------------------------------------------------------
 * Inicialização / destruição
 * --------------------------------------------------------------- */
void fs_init(void)
{
    disk_init();
    fs_root = node_new("/", FT_DIRECTORY);
    fs_root->parent = fs_root;   /* ".." da raiz é a própria raiz */
    fs_cwd = fs_root;
}

void fs_destroy(void)
{
    node_free_recursive(fs_root);
    fs_root = fs_cwd = NULL;
}

/* ---------------------------------------------------------------
 * Navegação
 * --------------------------------------------------------------- */
Node *fs_find_child(Node *dir, const char *name)
{
    if (!dir || dir->fcb.type != FT_DIRECTORY) return NULL;
    if (strcmp(name, ".") == 0)  return dir;
    if (strcmp(name, "..") == 0) return dir->parent;

    for (Node *c = dir->first_child; c; c = c->next_sibling)
        if (strcmp(c->fcb.name, name) == 0)
            return c;
    return NULL;
}

/* Resolve um caminho absoluto ("/a/b") ou relativo ("a/../b"). */
Node *fs_resolve(const char *path)
{
    if (!path || !*path) return fs_cwd;

    char buf[MAX_PATH];
    strncpy(buf, path, MAX_PATH - 1);
    buf[MAX_PATH - 1] = '\0';

    Node *cur = (buf[0] == '/') ? fs_root : fs_cwd;
    for (char *tok = strtok(buf, "/"); tok; tok = strtok(NULL, "/")) {
        cur = fs_find_child(cur, tok);
        if (!cur) return NULL;
    }
    return cur;
}

/* Monta o caminho absoluto de um nó subindo até a raiz. */
void fs_get_path(Node *node, char *out, size_t out_size)
{
    if (node == fs_root) {
        snprintf(out, out_size, "/");
        return;
    }
    char tmp[MAX_PATH] = "";
    for (Node *n = node; n != fs_root; n = n->parent) {
        char part[MAX_PATH];
        snprintf(part, sizeof part, "/%s%s", n->fcb.name, tmp);
        strncpy(tmp, part, sizeof tmp - 1);
    }
    snprintf(out, out_size, "%s", tmp);
}

/* Nome válido: não vazio, cabe em MAX_NAME, sem '/', não é "." nem "..". */
bool fs_valid_name(const char *name)
{
    if (!name || !*name) return false;
    if (strlen(name) >= MAX_NAME) return false;
    if (strchr(name, '/')) return false;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return false;
    return true;
}

/* ---------------------------------------------------------------
 * 3.1 - Diretórios
 * --------------------------------------------------------------- */
int fs_mkdir(const char *name)
{
    if (!fs_valid_name(name)) {
        printf("mkdir: nome inválido: '%s'\n", name ? name : "");
        return -1;
    }
    if (!perm_check(&fs_cwd->fcb, &current_user, PERM_W)) {
        printf("mkdir: permissão negada\n");
        return -1;
    }
    if (fs_find_child(fs_cwd, name)) {
        printf("mkdir: '%s' já existe\n", name);
        return -1;
    }
    node_add_child(fs_cwd, node_new(name, FT_DIRECTORY));
    fs_cwd->fcb.modified = time(NULL);
    return 0;
}

int fs_cd(const char *path)
{
    Node *target = fs_resolve(path ? path : "/");
    if (!target) {
        printf("cd: '%s': diretório não encontrado\n", path);
        return -1;
    }
    if (target->fcb.type != FT_DIRECTORY) {
        printf("cd: '%s': não é um diretório\n", path);
        return -1;
    }
    /* Em Unix, "entrar" num diretório exige permissão de execução (x). */
    if (!perm_check(&target->fcb, &current_user, PERM_X)) {
        printf("cd: '%s': permissão negada\n", path);
        return -1;
    }
    fs_cwd = target;
    target->fcb.accessed = time(NULL);
    return 0;
}

static void print_long(const Node *n)
{
    char perms[11];
    char date[20];
    perm_to_string(&n->fcb, perms);
    strftime(date, sizeof date, "%d/%m %H:%M", localtime(&n->fcb.modified));
    printf("%s %4d %4d %6zu %s %s%s\n",
           perms, n->fcb.owner_uid, n->fcb.group_gid,
           n->fcb.size, date, n->fcb.name,
           n->fcb.type == FT_DIRECTORY ? "/" : "");
}

int fs_ls(const char *path, bool long_format)
{
    Node *dir = fs_resolve(path);
    if (!dir) {
        printf("ls: '%s': não encontrado\n", path);
        return -1;
    }
    if (dir->fcb.type != FT_DIRECTORY) {   /* ls num arquivo: mostra ele */
        if (long_format) print_long(dir);
        else printf("%s\n", dir->fcb.name);
        return 0;
    }
    if (!perm_check(&dir->fcb, &current_user, PERM_R)) {
        printf("ls: permissão negada\n");
        return -1;
    }
    for (Node *c = dir->first_child; c; c = c->next_sibling) {
        if (long_format) print_long(c);
        else printf("%s%s  ", c->fcb.name,
                    c->fcb.type == FT_DIRECTORY ? "/" : "");
    }
    if (!long_format && dir->first_child) printf("\n");
    dir->fcb.accessed = time(NULL);
    return 0;
}

int fs_pwd(void)
{
    char path[MAX_PATH];
    fs_get_path(fs_cwd, path, sizeof path);
    printf("%s\n", path);
    return 0;
}

/* ---------------------------------------------------------------
 * 3.2 - Arquivos e metadados   (TODO: implementar juntos)
 * --------------------------------------------------------------- */
#define NOT_YET(cmd) do { printf("%s: ainda não implementado\n", cmd); return -1; } while (0)

int fs_touch(const char *name)                         { (void)name; NOT_YET("touch"); }
int fs_write(const char *n, const char *c, bool app)   { (void)n; (void)c; (void)app; NOT_YET("echo"); }
int fs_cat(const char *name)                           { (void)name; NOT_YET("cat"); }
int fs_cp(const char *src, const char *dst)            { (void)src; (void)dst; NOT_YET("cp"); }
int fs_mv(const char *src, const char *dst)            { (void)src; (void)dst; NOT_YET("mv"); }
int fs_rm(const char *name)                            { (void)name; NOT_YET("rm"); }
int fs_stat(const char *name)                          { (void)name; NOT_YET("stat"); }
