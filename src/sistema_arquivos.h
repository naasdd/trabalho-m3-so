/*
 * sistema_arquivos.h
 *
 * Este é o arquivo central do projeto. Tudo o que os outros arquivos .c
 * precisam conhecer está declarado aqui:
 *
 *   1. Constantes (tamanhos máximos, permissões, etc.)
 *   2. Tipos de dados (structs e enums)
 *   3. Variáveis globais (raiz, diretório atual, usuário atual)
 *   4. Funções de cada arquivo .c
 *
 * Relação com a teoria da disciplina:
 *   - struct FCB        -> File Control Block / "inode" simulado
 *   - struct NoArvore   -> diretório estruturado em árvore, guardado
 *                          num ARRAY DE STRUCTS (a tabela de nós)
 *   - struct Usuario    -> quem está tentando acessar (controle de acesso)
 *   - disco.c           -> disco simulado dividido em blocos
 */
#ifndef SISTEMA_ARQUIVOS_H
#define SISTEMA_ARQUIVOS_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>


/* =====================================================================
 * 1. CONSTANTES
 * ===================================================================== */

/* Tamanho do vetor de nós: no máximo 128 arquivos + pastas ao mesmo tempo */
#define MAXIMO_DE_NOS            128

/* Limites de nomes e caminhos */
#define TAMANHO_MAXIMO_NOME      32   /* ex.: "relatorio.txt"          */
#define TAMANHO_MAXIMO_CAMINHO   256  /* ex.: "/home/alice/docs/a.txt" */

/* Disco simulado: 256 blocos de 64 bytes = 16 KB no total */
#define TAMANHO_DO_BLOCO              64
#define QUANTIDADE_DE_BLOCOS_NO_DISCO 256

/* Alocação indexada: cada arquivo pode usar no máximo 16 blocos.
 * Logo, o maior arquivo possível tem 16 * 64 = 1024 bytes. */
#define MAXIMO_DE_BLOCOS_POR_ARQUIVO  16
#define TAMANHO_MAXIMO_DO_ARQUIVO     (MAXIMO_DE_BLOCOS_POR_ARQUIVO * TAMANHO_DO_BLOCO)

/*
 * PERMISSÕES (igual ao Linux)
 *
 * Cada arquivo tem 9 bits de permissão, divididos em 3 grupos de 3:
 *
 *       dono      grupo     outros
 *      r  w  x   r  w  x   r  w  x
 *
 * Cada letra vale um número:  r = 4,  w = 2,  x = 1.
 * Somando as letras de cada grupo, temos um dígito de 0 a 7.
 *
 * Exemplo: chmod 754
 *     7 = 4+2+1 = rwx   (dono pode ler, escrever e executar)
 *     5 = 4+0+1 = r-x   (grupo pode ler e executar)
 *     4 = 4+0+0 = r--   (outros só podem ler)
 *
 * No código guardamos isso como um número octal: 0754.
 */
#define PERMISSAO_DE_LEITURA   4   /* r */
#define PERMISSAO_DE_ESCRITA   2   /* w */
#define PERMISSAO_DE_EXECUCAO  1   /* x */

/* Quantos bits "andar para a direita" para chegar no trio de cada classe */
#define POSICAO_DO_TRIO_DONO    6
#define POSICAO_DO_TRIO_GRUPO   3
#define POSICAO_DO_TRIO_OUTROS  0

/* Permissões que todo arquivo/diretório recebe ao ser criado */
#define PERMISSOES_PADRAO_DE_ARQUIVO   0644   /* rw-r--r-- */
#define PERMISSOES_PADRAO_DE_DIRETORIO 0755   /* rwxr-xr-x */


/* =====================================================================
 * 2. TIPOS DE DADOS
 * ===================================================================== */

/* O "tipo" de um arquivo (um dos atributos do FCB). */
typedef enum {
    TIPO_DIRETORIO,
    TIPO_TEXTO,
    TIPO_NUMERICO,
    TIPO_BINARIO,
    TIPO_PROGRAMA
} TipoArquivo;

/*
 * FCB - File Control Block (o "inode" simulado)
 *
 * Guarda TODAS as informações sobre um arquivo, MENOS o conteúdo.
 * O conteúdo fica espalhado nos blocos do disco, e o FCB só guarda
 * a lista de QUAIS blocos são dele (campo blocos_do_arquivo).
 */
typedef struct {
    int          numero_inode;        /* identificador único do arquivo */
    char         nome[TAMANHO_MAXIMO_NOME];
    TipoArquivo  tipo;
    size_t       tamanho_em_bytes;

    time_t       data_de_criacao;
    time_t       data_de_modificacao;
    time_t       data_do_ultimo_acesso;

    int          permissoes;          /* ex.: 0754 (ver explicação acima) */
    int          id_do_dono;
    int          id_do_grupo;

    /* Alocação indexada: os números dos blocos onde está o conteúdo */
    int          blocos_do_arquivo[MAXIMO_DE_BLOCOS_POR_ARQUIVO];
    int          quantidade_de_blocos;
} FCB;

/*
 * NoArvore - um item da árvore de diretórios (pode ser arquivo ou pasta).
 *
 * ESTRUTURA ESCOLHIDA PELO GRUPO: ARRAY DE STRUCTS.
 *
 * Todos os arquivos e pastas moram num único vetor, a "tabela de nós"
 * (parecida com a tabela de inodes de um disco Linux de verdade).
 * Cada posição do vetor guarda um NoArvore.
 *
 * A árvore é montada só com NÚMEROS: cada nó guarda a POSIÇÃO (índice)
 * da sua pasta-mãe dentro do vetor.
 *
 *   índice:   0        1        2         3        4
 *           +------+ +------+ +-------+ +------+ +------+
 *   nome:   |  /   | | home | | alice | | bob  | | (livre)
 *   pai:    |  0   | |  0   | |   1   | |  1   | |  -   |
 *           +------+ +------+ +-------+ +------+ +------+
 *
 *   Isso desenha a árvore:      /
 *                               └── home
 *                                   ├── alice
 *                                   └── bob
 *
 *   - Os filhos de uma pasta são todos os nós cujo "pai" é o índice dela.
 *   - A raiz fica sempre no índice 0 e é pai de si mesma.
 *   - Uma posição com em_uso == false está livre para um arquivo novo.
 */
typedef struct {
    bool  em_uso;          /* esta posição do vetor está ocupada?     */
    FCB   fcb;
    int   indice_do_pai;   /* posição da pasta-mãe dentro do vetor    */
} NoArvore;

/* O que pode acontecer ao tentar gravar um conteúdo no disco. */
typedef enum {
    GRAVOU_COM_SUCESSO,
    ERRO_ARQUIVO_GRANDE_DEMAIS,   /* precisaria de mais de 16 blocos       */
    ERRO_DISCO_CHEIO              /* não sobrou bloco livre suficiente     */
} ResultadoDaGravacao;

/* Um usuário do sistema simulado. */
typedef struct {
    int  id_do_usuario;
    int  id_do_grupo;
    char nome[TAMANHO_MAXIMO_NOME];
} Usuario;


/* =====================================================================
 * 3. VARIÁVEIS GLOBAIS
 * (definidas em arvore.c e terminal.c)
 * ===================================================================== */
extern NoArvore *tabela_de_nos;      /* o vetor com MAXIMO_DE_NOS nós   */
extern NoArvore *raiz;               /* a pasta "/" (tabela_de_nos[0])  */
extern NoArvore *diretorio_atual;    /* onde o usuário está agora       */
extern Usuario   usuario_atual;      /* quem está usando o terminal     */


/* =====================================================================
 * 4. FUNÇÕES
 * ===================================================================== */

/* ---------- arvore.c : estrutura da árvore ---------- */
void      iniciar_sistema_de_arquivos(void);
void      encerrar_sistema_de_arquivos(void);
NoArvore *criar_no(const char *nome, TipoArquivo tipo);
void      adicionar_filho(NoArvore *pasta, NoArvore *filho);
int       indice_do_no(NoArvore *no);
NoArvore *pasta_de_cima(NoArvore *no);
bool      e_filho_de(NoArvore *no, NoArvore *pasta);
bool      pasta_esta_vazia(NoArvore *pasta);
int       contar_nos_em_uso(void);
NoArvore *procurar_filho_pelo_nome(NoArvore *pasta, const char *nome);
NoArvore *encontrar_pelo_caminho(const char *caminho);
void      montar_caminho_completo(NoArvore *no, char *destino, size_t tamanho_destino);
bool      nome_e_valido(const char *nome);
bool      e_diretorio(NoArvore *no);
bool      separar_pasta_e_nome(const char *caminho, NoArvore **pasta_encontrada, char *nome_encontrado);
void      liberar_no_e_todos_os_filhos(NoArvore *no);
bool      esta_dentro_de(NoArvore *possivel_descendente, NoArvore *pasta);

/* ---------- comandos_diretorio.c : 3.1 ---------- */
void comando_mkdir(const char *caminho);
void comando_cd(const char *caminho);
void comando_ls(const char *caminho, bool formato_detalhado);
void comando_pwd(void);
void comando_rmdir(const char *caminho);

/* ---------- comandos_arquivo.c : 3.2 ---------- */
void comando_touch(const char *nome);
void comando_echo_para_arquivo(const char *nome, const char *texto, bool adicionar_no_final);
void comando_cat(const char *nome);
void comando_cp(const char *origem, const char *destino);
void comando_mv(const char *origem, const char *destino);
void comando_rm(const char *nome);
void comando_stat(const char *nome);
void comando_executar(const char *nome);

/* ---------- permissoes.c : 3.3 ---------- */
bool usuario_tem_permissao(const FCB *fcb, const Usuario *usuario, int permissao_desejada);
void montar_texto_de_permissoes(const FCB *fcb, char destino[11]);
void comando_chmod(const char *modo, const char *nome);

/* ---------- disco.c : 3.4 ---------- */
void   iniciar_disco(void);
int    reservar_bloco_livre(void);
void   liberar_bloco(int numero_do_bloco);
int    contar_blocos_livres(void);
ResultadoDaGravacao gravar_conteudo_no_disco(FCB *fcb, const char *conteudo, size_t tamanho);
size_t ler_conteudo_do_disco(const FCB *fcb, char *destino, size_t tamanho_destino);
void   liberar_todos_os_blocos_do_arquivo(FCB *fcb);
void   comando_disk(void);

/* ---------- terminal.c ---------- */
void rodar_terminal(void);

#endif /* SISTEMA_ARQUIVOS_H */
