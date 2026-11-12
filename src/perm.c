/*
 * perm.c - Controle de acesso RWX (owner / group / other) com bitmasks.
 */
#include <stdio.h>

#include "fs.h"

/*
 * Verifica se 'user' tem a permissão 'wanted' (PERM_R, PERM_W ou PERM_X)
 * sobre o arquivo descrito por 'fcb'.
 *
 * Igual ao Unix: escolhe UMA classe (dono, senão grupo, senão outros)
 * e testa só os 3 bits dessa classe.
 */
bool perm_check(const FCB *fcb, const User *user, int wanted)
{
    if (user->uid == 0)          /* root pode tudo */
        return true;

    int shift;
    if (user->uid == fcb->owner_uid)       shift = SHIFT_OWNER;
    else if (user->gid == fcb->group_gid)  shift = SHIFT_GROUP;
    else                                   shift = SHIFT_OTHER;

    int class_bits = (fcb->perms >> shift) & 07;  /* isola rwx da classe */
    return (class_bits & wanted) == wanted;
}

/* Converte o FCB em "drwxr-xr-x" (10 caracteres + '\0'). */
void perm_to_string(const FCB *fcb, char out[11])
{
    const char *rwx = "rwxrwxrwx";
    out[0] = (fcb->type == FT_DIRECTORY) ? 'd' : '-';
    for (int i = 0; i < 9; i++) {
        int bit = 8 - i;                      /* bit 8 = r do dono ... */
        out[i + 1] = (fcb->perms & (1 << bit)) ? rwx[i] : '-';
    }
    out[10] = '\0';
}

/* TODO (3.3): chmod numérico, ex.: chmod 754 arquivo */
int fs_chmod(const char *mode, const char *name)
{
    (void)mode; (void)name;
    printf("chmod: ainda não implementado\n");
    return -1;
}
