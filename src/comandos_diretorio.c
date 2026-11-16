/*
 * comandos_diretorio.c
 *
 * Objetivo 3.1 do enunciado: criar pastas e navegar entre elas.
 * Comandos: mkdir, cd, ls, pwd
 */
#include <stdio.h>
#include <string.h>

#include "sistema_arquivos.h"


/* mkdir <caminho> : cria uma pasta (na pasta atual ou no caminho indicado) */
void comando_mkdir(const char *caminho)
{
    /*
     * Aceita nome simples ("docs") ou caminho ("/home/alice", "a/b").
     * separar_pasta_e_nome diz em qual pasta criar e com qual nome.
     */
    NoArvore *pasta;
    char nome[TAMANHO_MAXIMO_NOME];

    if (!separar_pasta_e_nome(caminho, &pasta, nome)) {
        printf("mkdir: '%s': a pasta não existe\n", caminho);
        return;
    }

    if (!nome_e_valido(nome)) {
        printf("mkdir: nome inválido: '%s'\n", caminho);
        return;
    }

    if (procurar_filho_pelo_nome(pasta, nome) != NULL) {
        printf("mkdir: '%s' já existe\n", caminho);
        return;
    }

    /* Para criar algo dentro de uma pasta, precisa poder ESCREVER nela */
    if (!usuario_tem_permissao(&pasta->fcb, &usuario_atual, PERMISSAO_DE_ESCRITA)) {
        printf("mkdir: permissão negada\n");
        return;
    }

    NoArvore *nova_pasta = criar_no(nome, TIPO_DIRETORIO);
    if (nova_pasta == NULL) {
        printf("mkdir: limite de %d arquivos e pastas atingido\n", MAXIMO_DE_NOS);
        return;
    }
    adicionar_filho(pasta, nova_pasta);
    pasta->fcb.data_de_modificacao = time(NULL);
}


/* cd <caminho> : muda a pasta atual */
void comando_cd(const char *caminho)
{
    NoArvore *destino = encontrar_pelo_caminho(caminho);

    if (destino == NULL) {
        printf("cd: '%s': diretório não encontrado\n", caminho);
        return;
    }

    if (!e_diretorio(destino)) {
        printf("cd: '%s': não é um diretório\n", caminho);
        return;
    }

    /* No Linux, para ENTRAR numa pasta é preciso permissão de EXECUÇÃO (x) */
    if (!usuario_tem_permissao(&destino->fcb, &usuario_atual, PERMISSAO_DE_EXECUCAO)) {
        printf("cd: '%s': permissão negada\n", caminho);
        return;
    }

    diretorio_atual = destino;
    destino->fcb.data_do_ultimo_acesso = time(NULL);
}


/*
 * Imprime uma linha no estilo "ls -l":
 *   drwxr-xr-x  1000  100     0  12/11 20:36  alice/
 *   permissões  dono  grupo  tam  data        nome
 */
static void imprimir_linha_detalhada(NoArvore *no)
{
    char texto_permissoes[11];
    montar_texto_de_permissoes(&no->fcb, texto_permissoes);

    char texto_data[20];
    struct tm *data_local = localtime(&no->fcb.data_de_modificacao);
    strftime(texto_data, sizeof(texto_data), "%d/%m %H:%M", data_local);

    printf("%s  %4d  %4d  %5zu  %s  %s",
           texto_permissoes,
           no->fcb.id_do_dono,
           no->fcb.id_do_grupo,
           no->fcb.tamanho_em_bytes,
           texto_data,
           no->fcb.nome);

    /* Pastas ganham uma "/" no final para diferenciar de arquivos */
    if (e_diretorio(no)) {
        printf("/");
    }
    printf("\n");
}

/* Imprime só o nome (formato simples do "ls"). */
static void imprimir_nome(NoArvore *no)
{
    printf("%s", no->fcb.nome);
    if (e_diretorio(no)) {
        printf("/");
    }
    printf("  ");
}


/* ls [-l] [caminho] : lista o conteúdo de uma pasta */
void comando_ls(const char *caminho, bool formato_detalhado)
{
    NoArvore *alvo = encontrar_pelo_caminho(caminho);

    if (alvo == NULL) {
        printf("ls: '%s': não encontrado\n", caminho);
        return;
    }

    /* Se pediu "ls" de um ARQUIVO, mostramos só ele */
    if (!e_diretorio(alvo)) {
        if (formato_detalhado) {
            imprimir_linha_detalhada(alvo);
        } else {
            imprimir_nome(alvo);
            printf("\n");
        }
        return;
    }

    /* Para listar uma pasta, precisa poder LER ela */
    if (!usuario_tem_permissao(&alvo->fcb, &usuario_atual, PERMISSAO_DE_LEITURA)) {
        printf("ls: permissão negada\n");
        return;
    }

    /* Percorre o vetor de nós mostrando quem é filho desta pasta */
    for (int i = 0; i < MAXIMO_DE_NOS; i++) {
        NoArvore *no = &tabela_de_nos[i];
        if (!e_filho_de(no, alvo)) {
            continue;
        }
        if (formato_detalhado) {
            imprimir_linha_detalhada(no);
        } else {
            imprimir_nome(no);
        }
    }

    /* No formato simples, os nomes ficam na mesma linha; pulamos linha no fim */
    bool pasta_tem_algo = !pasta_esta_vazia(alvo);
    if (!formato_detalhado && pasta_tem_algo) {
        printf("\n");
    }

    alvo->fcb.data_do_ultimo_acesso = time(NULL);
}


/* rmdir <pasta> : apaga uma pasta VAZIA (igual ao Linux) */
void comando_rmdir(const char *caminho)
{
    NoArvore *pasta = encontrar_pelo_caminho(caminho);

    if (pasta == NULL) {
        printf("rmdir: '%s': não encontrado\n", caminho);
        return;
    }
    if (!e_diretorio(pasta)) {
        printf("rmdir: '%s': não é um diretório (use rm)\n", caminho);
        return;
    }
    /* Não dá para apagar a pasta onde você está, nem uma acima dela */
    if (pasta == raiz || esta_dentro_de(diretorio_atual, pasta)) {
        printf("rmdir: '%s': não é possível remover a pasta atual ou uma acima dela\n", caminho);
        return;
    }
    if (!pasta_esta_vazia(pasta)) {
        printf("rmdir: '%s': a pasta não está vazia\n", caminho);
        return;
    }

    NoArvore *pasta_mae = pasta_de_cima(pasta);
    if (!usuario_tem_permissao(&pasta_mae->fcb, &usuario_atual, PERMISSAO_DE_ESCRITA)) {
        printf("rmdir: '%s': permissão negada\n", caminho);
        return;
    }

    liberar_no_e_todos_os_filhos(pasta);   /* libera a posição no vetor */
    pasta_mae->fcb.data_de_modificacao = time(NULL);
}


/* pwd : mostra o caminho da pasta atual */
void comando_pwd(void)
{
    char caminho[TAMANHO_MAXIMO_CAMINHO];
    montar_caminho_completo(diretorio_atual, caminho, sizeof(caminho));
    printf("%s\n", caminho);
}
