/*
 * permissoes.c
 *
 * Objetivo 3.3 do enunciado: controle de acesso RWX
 * para dono, grupo e outros, usando operações com bits.
 */
#include <stdio.h>
#include <string.h>

#include "sistema_arquivos.h"


/*
 * Descobre qual "trio" de permissões vale para esse usuário.
 *
 * Igual ao Linux, vale só UM trio, nesta ordem:
 *   1. se o usuário é o dono  -> trio do dono
 *   2. se é do mesmo grupo    -> trio do grupo
 *   3. senão                  -> trio dos outros
 */
static int descobrir_posicao_do_trio(const FCB *fcb, const Usuario *usuario)
{
    if (usuario->id_do_usuario == fcb->id_do_dono) {
        return POSICAO_DO_TRIO_DONO;
    }
    if (usuario->id_do_grupo == fcb->id_do_grupo) {
        return POSICAO_DO_TRIO_GRUPO;
    }
    return POSICAO_DO_TRIO_OUTROS;
}

/*
 * Pega só os 3 bits (rwx) de uma classe.
 *
 * Exemplo com permissoes = 0754 (rwx r-x r--) e o trio do GRUPO (posição 3):
 *
 *   0754 em binário:      111 101 100
 *   ">> 3" anda 3 bits:       111 101     (o trio do grupo ficou no fim)
 *   "& 7" (7 = 111):              101     (apaga tudo menos os 3 últimos)
 *
 *   Resultado: 101 = 5 = r-x
 */
static int extrair_trio(int permissoes, int posicao_do_trio)
{
    int deslocado = permissoes >> posicao_do_trio;
    int so_os_tres_ultimos_bits = deslocado & 7;
    return so_os_tres_ultimos_bits;
}


/*
 * Responde: esse usuário pode fazer essa operação nesse arquivo?
 *
 * permissao_desejada é PERMISSAO_DE_LEITURA, PERMISSAO_DE_ESCRITA
 * ou PERMISSAO_DE_EXECUCAO.
 */
bool usuario_tem_permissao(const FCB *fcb, const Usuario *usuario, int permissao_desejada)
{
    /* O root (id 0) pode tudo, igual no Linux */
    if (usuario->id_do_usuario == 0) {
        return true;
    }

    int posicao = descobrir_posicao_do_trio(fcb, usuario);
    int trio    = extrair_trio(fcb->permissoes, posicao);

    /*
     * "&" testa se o bit desejado está ligado no trio.
     * Ex.: trio = 101 (r-x), desejado = escrita = 010
     *      101 & 010 = 000  -> não tem permissão
     */
    bool tem_o_bit = (trio & permissao_desejada) != 0;
    return tem_o_bit;
}


/* Escreve 3 letras ("rwx", "r-x", ...) a partir de um trio de bits. */
static void escrever_trio_como_texto(int trio, char *destino)
{
    if (trio & PERMISSAO_DE_LEITURA) {
        destino[0] = 'r';
    } else {
        destino[0] = '-';
    }

    if (trio & PERMISSAO_DE_ESCRITA) {
        destino[1] = 'w';
    } else {
        destino[1] = '-';
    }

    if (trio & PERMISSAO_DE_EXECUCAO) {
        destino[2] = 'x';
    } else {
        destino[2] = '-';
    }
}

/*
 * Monta o texto de permissões igual ao "ls -l", ex.: "drwxr-xr-x".
 *
 *   posição 0     : 'd' se for diretório, '-' se for arquivo
 *   posições 1-3  : dono
 *   posições 4-6  : grupo
 *   posições 7-9  : outros
 */
void montar_texto_de_permissoes(const FCB *fcb, char destino[11])
{
    if (fcb->tipo == TIPO_DIRETORIO) {
        destino[0] = 'd';
    } else {
        destino[0] = '-';
    }

    int trio_dono   = extrair_trio(fcb->permissoes, POSICAO_DO_TRIO_DONO);
    int trio_grupo  = extrair_trio(fcb->permissoes, POSICAO_DO_TRIO_GRUPO);
    int trio_outros = extrair_trio(fcb->permissoes, POSICAO_DO_TRIO_OUTROS);

    escrever_trio_como_texto(trio_dono,   &destino[1]);
    escrever_trio_como_texto(trio_grupo,  &destino[4]);
    escrever_trio_como_texto(trio_outros, &destino[7]);

    destino[10] = '\0';
}


/* O modo precisa ter exatamente 3 dígitos, cada um de 0 a 7. Ex.: "754" */
static bool modo_e_valido(const char *modo)
{
    if (strlen(modo) != 3) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (modo[i] < '0' || modo[i] > '7') {
            return false;
        }
    }
    return true;
}

/*
 * Converte o texto "754" no número de permissões (0754).
 *
 * Cada dígito vai para o seu trio usando "<<" (empurrar bits para a esquerda),
 * e o "|" (OU bit a bit) junta os três trios num número só:
 *
 *   dono   = 7 = 111   -> 7 << 6 = 111 000 000
 *   grupo  = 5 = 101   -> 5 << 3 =     101 000
 *   outros = 4 = 100   -> 4 << 0 =         100
 *                                  -----------  (OU)
 *                                  111 101 100  = 0754 = rwxr-xr--
 */
static int converter_modo_para_permissoes(const char *modo)
{
    /* Subtrair '0' transforma o caractere '7' no número 7 */
    int digito_dono   = modo[0] - '0';
    int digito_grupo  = modo[1] - '0';
    int digito_outros = modo[2] - '0';

    int permissoes = (digito_dono   << POSICAO_DO_TRIO_DONO)
                   | (digito_grupo  << POSICAO_DO_TRIO_GRUPO)
                   | (digito_outros << POSICAO_DO_TRIO_OUTROS);
    return permissoes;
}

/* chmod <modo> <caminho> : ex.: chmod 754 arquivo.txt */
void comando_chmod(const char *modo, const char *caminho)
{
    if (!modo_e_valido(modo)) {
        printf("chmod: modo inválido '%s' (use 3 dígitos de 0 a 7, ex.: 754)\n", modo);
        return;
    }

    NoArvore *no = encontrar_pelo_caminho(caminho);
    if (no == NULL) {
        printf("chmod: '%s': não encontrado\n", caminho);
        return;
    }

    /* No Linux, só o DONO do arquivo (ou o root) pode mudar as permissões */
    bool e_root = (usuario_atual.id_do_usuario == 0);
    bool e_dono = (usuario_atual.id_do_usuario == no->fcb.id_do_dono);
    if (!e_root && !e_dono) {
        printf("chmod: '%s': operação não permitida (só o dono ou o root)\n", caminho);
        return;
    }

    no->fcb.permissoes = converter_modo_para_permissoes(modo);
}
