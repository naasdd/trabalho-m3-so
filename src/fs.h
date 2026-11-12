/*
 * fs.h - Definições centrais do mini-sistema de arquivos em memória.
 *
 * Aqui ficam as constantes, os tipos (enums) e as estruturas que
 * representam os conceitos teóricos da disciplina:
 *   - FCB (File Control Block) / inode simulado  -> struct FCB
 *   - Diretório estruturado em árvore            -> struct Node
 *   - Usuário atual (para controle de acesso)    -> struct User
 *   - Disco simulado em blocos                   -> disk.c
 */
#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ---------------------------------------------------------------
 * Constantes
 * --------------------------------------------------------------- */
#define MAX_NAME        32      /* tamanho máximo de um nome de arquivo   */
#define MAX_PATH        256     /* tamanho máximo de um caminho           */

#define BLOCK_SIZE      64      /* bytes por bloco do disco simulado      */
#define NUM_BLOCKS      256     /* total de blocos -> disco de 16 KiB     */
#define MAX_FILE_BLOCKS 16      /* entradas no bloco de índice (indexada) */

/* ---------------------------------------------------------------
 * Permissões (bitmask estilo Unix: rwx rwx rwx = 9 bits)
 *
 *   owner  group  other
 *   r w x  r w x  r w x
 *   8 7 6  5 4 3  2 1 0   <- posição do bit
 *
 * Ex.: chmod 754 -> 0754 (octal) -> rwxr-xr--
 * --------------------------------------------------------------- */
#define PERM_R 04
#define PERM_W 02
#define PERM_X 01

#define SHIFT_OWNER 6
#define SHIFT_GROUP 3
#define SHIFT_OTHER 0

#define DEFAULT_FILE_PERMS 0644 /* rw-r--r-- */
#define DEFAULT_DIR_PERMS  0755 /* rwxr-xr-x */

/* ---------------------------------------------------------------
 * Tipos de arquivo (atributo "tipo" do FCB)
 * --------------------------------------------------------------- */
typedef enum {
    FT_DIRECTORY,
    FT_TEXT,      /* caractere */
    FT_NUMERIC,
    FT_BINARY,
    FT_PROGRAM
} FileType;

/* ---------------------------------------------------------------
 * File Control Block (FCB) - o "inode" simulado.
 * Guarda TODOS os metadados do arquivo, menos o conteúdo em si,
 * que fica nos blocos do disco referenciados por blocks[].
 * --------------------------------------------------------------- */
typedef struct {
    int      inode_id;               /* ID único (número do inode)       */
    char     name[MAX_NAME];         /* nome                             */
    FileType type;                   /* tipo                             */
    size_t   size;                   /* tamanho em bytes                 */
    time_t   created;                /* data de criação                  */
    time_t   modified;               /* data de modificação              */
    time_t   accessed;               /* data do último acesso            */
    uint16_t perms;                  /* bitmask rwxrwxrwx                */
    int      owner_uid;              /* dono                             */
    int      group_gid;              /* grupo                            */

    /* Alocação indexada: lista de blocos que guardam o conteúdo */
    int      blocks[MAX_FILE_BLOCKS];
    int      num_blocks;
} FCB;

/* ---------------------------------------------------------------
 * Nó da árvore de diretórios.
 * Representação "primeiro filho / próximo irmão": cada nó aponta
 * para o pai, para o primeiro filho e para o próximo irmão.
 * Assim uma árvore n-ária vira uma estrutura só com ponteiros.
 * --------------------------------------------------------------- */
typedef struct Node {
    FCB          fcb;
    struct Node *parent;
    struct Node *first_child;   /* só usado se for diretório */
    struct Node *next_sibling;
} Node;

/* Usuário que está "logado" no simulador */
typedef struct {
    int  uid;
    int  gid;
    char name[MAX_NAME];
} User;

/* ---------------------------------------------------------------
 * Estado global do sistema de arquivos
 * --------------------------------------------------------------- */
extern Node *fs_root;      /* raiz "/"                 */
extern Node *fs_cwd;       /* diretório corrente       */
extern User  current_user; /* usuário atual            */

/* ---------- fs.c : árvore e operações ---------- */
void  fs_init(void);
void  fs_destroy(void);
Node *fs_find_child(Node *dir, const char *name);
Node *fs_resolve(const char *path);           /* caminho -> nó, ou NULL  */
void  fs_get_path(Node *node, char *out, size_t out_size);
bool  fs_valid_name(const char *name);

int fs_mkdir(const char *name);
int fs_cd(const char *path);
int fs_ls(const char *path, bool long_format);
int fs_pwd(void);
int fs_touch(const char *name);
int fs_write(const char *name, const char *content, bool append);
int fs_cat(const char *name);
int fs_cp(const char *src, const char *dst);
int fs_mv(const char *src, const char *dst);
int fs_rm(const char *name);
int fs_stat(const char *name);

/* ---------- perm.c : controle de acesso ---------- */
bool perm_check(const FCB *fcb, const User *user, int wanted);
void perm_to_string(const FCB *fcb, char out[11]);
int  fs_chmod(const char *mode, const char *name);

/* ---------- disk.c : disco simulado ---------- */
void   disk_init(void);
int    disk_alloc_block(void);                 /* retorna nº do bloco ou -1 */
void   disk_free_block(int block);
int    disk_write_file(FCB *fcb, const char *data, size_t len);
size_t disk_read_file(const FCB *fcb, char *out, size_t out_size);
void   disk_free_file(FCB *fcb);
void   disk_print_map(void);

#endif /* FS_H */
