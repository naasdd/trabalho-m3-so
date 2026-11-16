/*
 * comandos_arquivo.c
 *
 * Objetivo 3.2 do enunciado: criar, escrever, ler, copiar, mover e apagar
 * arquivos, tudo dentro da árvore simulada (nada no disco real).
 *
 * Comandos: touch, echo >, cat, cp, mv, rm, stat, ./arquivo
 *
 * Todos os comandos aceitam caminhos, ex.: "cat docs/a.txt".
 */
#include <stdio.h>
#include <string.h>

#include "sistema_arquivos.h"


/* =====================================================================
 * Funções de apoio (usadas por vários comandos)
 * ===================================================================== */

/*
 * Procura um ARQUIVO (não pasta) pelo caminho.
 * Se não achar, ou se for uma pasta, já mostra o erro e retorna NULL.
 */
static NoArvore *encontrar_arquivo(const char *nome_do_comando, const char *caminho)
{
    NoArvore *no = encontrar_pelo_caminho(caminho);

    if (no == NULL) {
        printf("%s: '%s': arquivo não encontrado\n", nome_do_comando, caminho);
        return NULL;
    }
    if (e_diretorio(no)) {
        printf("%s: '%s': é um diretório\n", nome_do_comando, caminho);
        return NULL;
    }
    return no;
}

/* Mostra uma mensagem clara quando a gravação no disco falha. */
static void mostrar_erro_de_gravacao(const char *nome_do_comando, ResultadoDaGravacao resultado)
{
    if (resultado == ERRO_ARQUIVO_GRANDE_DEMAIS) {
        printf("%s: arquivo grande demais (máximo de %d bytes)\n",
               nome_do_comando, TAMANHO_MAXIMO_DO_ARQUIVO);
    }
    if (resultado == ERRO_DISCO_CHEIO) {
        printf("%s: disco cheio, não há blocos livres suficientes\n", nome_do_comando);
    }
}

/* Responde se 'texto' termina com 'final'. Ex.: ("script.sh", ".sh") -> true */
static bool termina_com(const char *texto, const char *final)
{
    size_t tamanho_texto = strlen(texto);
    size_t tamanho_final = strlen(final);

    if (tamanho_final > tamanho_texto) {
        return false;
    }
    const char *onde_comecaria = texto + (tamanho_texto - tamanho_final);
    return strcmp(onde_comecaria, final) == 0;
}

/* Responde se o conteúdo tem só números (e espaços / quebras de linha). */
static bool conteudo_e_so_numeros(const char *conteudo)
{
    bool achou_algum_digito = false;

    for (size_t i = 0; conteudo[i] != '\0'; i++) {
        char letra = conteudo[i];
        bool e_digito = (letra >= '0' && letra <= '9');
        bool e_espaco = (letra == ' ' || letra == '\n');

        if (e_digito) {
            achou_algum_digito = true;
        } else if (!e_espaco) {
            return false;
        }
    }
    return achou_algum_digito;
}

/*
 * Decide o TIPO do arquivo (um dos atributos do FCB):
 *   - termina com ".sh"  -> programa
 *   - termina com ".bin" -> binário
 *   - só tem números     -> numérico
 *   - qualquer outra coisa -> texto
 */
static TipoArquivo descobrir_tipo_do_arquivo(const char *nome, const char *conteudo)
{
    if (termina_com(nome, ".sh")) {
        return TIPO_PROGRAMA;
    }
    if (termina_com(nome, ".bin")) {
        return TIPO_BINARIO;
    }
    if (conteudo_e_so_numeros(conteudo)) {
        return TIPO_NUMERICO;
    }
    return TIPO_TEXTO;
}

/* Transforma o tipo em texto, para mostrar no stat. */
static const char *nome_do_tipo(TipoArquivo tipo)
{
    switch (tipo) {
        case TIPO_DIRETORIO: return "diretório";
        case TIPO_TEXTO:     return "texto";
        case TIPO_NUMERICO:  return "numérico";
        case TIPO_BINARIO:   return "binário";
        case TIPO_PROGRAMA:  return "programa";
    }
    return "desconhecido";
}

/*
 * Cria um arquivo vazio no caminho indicado.
 * Faz todas as verificações (pasta existe, nome válido, permissão,
 * não existe outro com o mesmo nome). Retorna NULL se der erro.
 */
static NoArvore *criar_arquivo_vazio(const char *nome_do_comando, const char *caminho)
{
    NoArvore *pasta;
    char nome[TAMANHO_MAXIMO_NOME];

    if (!separar_pasta_e_nome(caminho, &pasta, nome)) {
        printf("%s: '%s': a pasta não existe\n", nome_do_comando, caminho);
        return NULL;
    }
    if (!nome_e_valido(nome)) {
        printf("%s: nome inválido: '%s'\n", nome_do_comando, caminho);
        return NULL;
    }
    if (procurar_filho_pelo_nome(pasta, nome) != NULL) {
        printf("%s: '%s' já existe\n", nome_do_comando, nome);
        return NULL;
    }
    /* Para criar algo dentro de uma pasta, precisa poder ESCREVER nela */
    if (!usuario_tem_permissao(&pasta->fcb, &usuario_atual, PERMISSAO_DE_ESCRITA)) {
        printf("%s: permissão negada para criar arquivos nesta pasta\n", nome_do_comando);
        return NULL;
    }

    NoArvore *novo_arquivo = criar_no(nome, TIPO_TEXTO);
    if (novo_arquivo == NULL) {
        printf("%s: limite de %d arquivos e pastas atingido\n", nome_do_comando, MAXIMO_DE_NOS);
        return NULL;
    }
    adicionar_filho(pasta, novo_arquivo);
    pasta->fcb.data_de_modificacao = time(NULL);
    return novo_arquivo;
}

/*
 * Para cp e mv: descobre em qual pasta e com qual nome o item vai ficar.
 *
 *   "cp a.txt docs"       (docs é uma pasta) -> pasta docs, nome "a.txt"
 *   "cp a.txt b.txt"                         -> pasta atual, nome "b.txt"
 *   "cp a.txt docs/b.txt"                    -> pasta docs, nome "b.txt"
 */
static bool decidir_destino(const char *destino, const char *nome_original,
                            NoArvore **pasta_destino, char *nome_destino)
{
    NoArvore *no_destino = encontrar_pelo_caminho(destino);

    bool destino_e_uma_pasta_existente = (no_destino != NULL && e_diretorio(no_destino));
    if (destino_e_uma_pasta_existente) {
        *pasta_destino = no_destino;
        strncpy(nome_destino, nome_original, TAMANHO_MAXIMO_NOME - 1);
        nome_destino[TAMANHO_MAXIMO_NOME - 1] = '\0';
        return true;
    }

    return separar_pasta_e_nome(destino, pasta_destino, nome_destino);
}


/* =====================================================================
 * Comandos
 * ===================================================================== */

/*
 * touch <arquivo>
 *   - se não existe: cria um arquivo vazio
 *   - se já existe: só atualiza as datas (igual ao Linux)
 */
void comando_touch(const char *caminho)
{
    NoArvore *existente = encontrar_pelo_caminho(caminho);

    if (existente != NULL) {
        if (!usuario_tem_permissao(&existente->fcb, &usuario_atual, PERMISSAO_DE_ESCRITA)) {
            printf("touch: '%s': permissão negada\n", caminho);
            return;
        }
        time_t agora = time(NULL);
        existente->fcb.data_de_modificacao   = agora;
        existente->fcb.data_do_ultimo_acesso = agora;
        return;
    }

    criar_arquivo_vazio("touch", caminho);
}


/*
 * echo <texto> > <arquivo>    -> substitui o conteúdo
 * echo <texto> >> <arquivo>   -> adiciona no final
 *
 * Igual ao Linux, o echo coloca uma quebra de linha ("\n") no fim do texto.
 * Por isso "echo oi > a.txt" gera um arquivo de 3 bytes: 'o', 'i', '\n'.
 */
void comando_echo_para_arquivo(const char *caminho, const char *texto, bool adicionar_no_final)
{
    NoArvore *arquivo = encontrar_pelo_caminho(caminho);

    /* Se o arquivo não existe, o echo cria (igual ao Linux) */
    if (arquivo == NULL) {
        arquivo = criar_arquivo_vazio("echo", caminho);
        if (arquivo == NULL) {
            return;
        }
    } else {
        if (e_diretorio(arquivo)) {
            printf("echo: '%s': é um diretório\n", caminho);
            return;
        }
        if (!usuario_tem_permissao(&arquivo->fcb, &usuario_atual, PERMISSAO_DE_ESCRITA)) {
            printf("echo: '%s': permissão negada\n", caminho);
            return;
        }
    }

    /* Monta o conteúdo novo em um vetor temporário */
    char conteudo_novo[TAMANHO_MAXIMO_DO_ARQUIVO + 1];
    size_t tamanho_atual = 0;

    if (adicionar_no_final) {
        /* ">>": começa com o que o arquivo já tem */
        tamanho_atual = ler_conteudo_do_disco(&arquivo->fcb, conteudo_novo, sizeof(conteudo_novo));
    }

    size_t tamanho_do_texto = strlen(texto);
    size_t tamanho_final = tamanho_atual + tamanho_do_texto + 1;   /* +1 do "\n" */

    if (tamanho_final > TAMANHO_MAXIMO_DO_ARQUIVO) {
        mostrar_erro_de_gravacao("echo", ERRO_ARQUIVO_GRANDE_DEMAIS);
        return;
    }

    memcpy(conteudo_novo + tamanho_atual, texto, tamanho_do_texto);
    conteudo_novo[tamanho_atual + tamanho_do_texto] = '\n';
    conteudo_novo[tamanho_final] = '\0';

    ResultadoDaGravacao resultado = gravar_conteudo_no_disco(&arquivo->fcb, conteudo_novo, tamanho_final);
    if (resultado != GRAVOU_COM_SUCESSO) {
        mostrar_erro_de_gravacao("echo", resultado);
        return;
    }

    arquivo->fcb.tipo = descobrir_tipo_do_arquivo(arquivo->fcb.nome, conteudo_novo);
    time_t agora = time(NULL);
    arquivo->fcb.data_de_modificacao   = agora;
    arquivo->fcb.data_do_ultimo_acesso = agora;
}


/* cat <arquivo> : mostra o conteúdo */
void comando_cat(const char *caminho)
{
    NoArvore *arquivo = encontrar_arquivo("cat", caminho);
    if (arquivo == NULL) {
        return;
    }

    if (!usuario_tem_permissao(&arquivo->fcb, &usuario_atual, PERMISSAO_DE_LEITURA)) {
        printf("cat: '%s': permissão negada\n", caminho);
        return;
    }

    char conteudo[TAMANHO_MAXIMO_DO_ARQUIVO + 1];
    ler_conteudo_do_disco(&arquivo->fcb, conteudo, sizeof(conteudo));
    printf("%s", conteudo);

    arquivo->fcb.data_do_ultimo_acesso = time(NULL);
}


/*
 * rm <arquivo> : apaga um arquivo
 *
 * Detalhe do Linux: para apagar, a permissão que importa é a de
 * ESCRITA NA PASTA (porque apagar = tirar o nome da lista da pasta),
 * e não a permissão do próprio arquivo.
 */
void comando_rm(const char *caminho)
{
    NoArvore *arquivo = encontrar_pelo_caminho(caminho);

    if (arquivo == NULL) {
        printf("rm: '%s': arquivo não encontrado\n", caminho);
        return;
    }
    if (e_diretorio(arquivo)) {
        printf("rm: '%s': é um diretório (use rmdir)\n", caminho);
        return;
    }

    NoArvore *pasta = pasta_de_cima(arquivo);
    if (!usuario_tem_permissao(&pasta->fcb, &usuario_atual, PERMISSAO_DE_ESCRITA)) {
        printf("rm: '%s': permissão negada\n", caminho);
        return;
    }

    liberar_no_e_todos_os_filhos(arquivo);   /* devolve os blocos e libera a posição no vetor */
    pasta->fcb.data_de_modificacao = time(NULL);
}


/* Escreve uma data no formato "14/11/2026 20:15:03" */
static void imprimir_data(const char *rotulo, time_t data)
{
    char texto_data[32];
    strftime(texto_data, sizeof(texto_data), "%d/%m/%Y %H:%M:%S", localtime(&data));
    printf("  %-14s %s\n", rotulo, texto_data);
}

/* stat <arquivo> : mostra todos os campos do FCB (o "inode") */
void comando_stat(const char *caminho)
{
    NoArvore *no = encontrar_pelo_caminho(caminho);
    if (no == NULL) {
        printf("stat: '%s': não encontrado\n", caminho);
        return;
    }

    FCB *fcb = &no->fcb;

    char texto_permissoes[11];
    montar_texto_de_permissoes(fcb, texto_permissoes);

    printf("  %-14s %s\n",        "Nome:",       fcb->nome);
    printf("  %-14s %d\n",        "Inode:",      fcb->numero_inode);
    printf("  %-14s %s\n",        "Tipo:",       nome_do_tipo(fcb->tipo));
    printf("  %-14s %zu bytes\n", "Tamanho:",    fcb->tamanho_em_bytes);
    /* %-15s aqui (e não 14) porque o "õ" ocupa 2 bytes e o printf conta bytes */
    printf("  %-15s %04o (%s)\n", "Permissões:", fcb->permissoes, texto_permissoes);
    printf("  %-14s %d\n",        "Dono (id):",  fcb->id_do_dono);
    printf("  %-14s %d\n",        "Grupo (id):", fcb->id_do_grupo);
    imprimir_data("Criado:",     fcb->data_de_criacao);
    imprimir_data("Modificado:", fcb->data_de_modificacao);
    imprimir_data("Acessado:",   fcb->data_do_ultimo_acesso);

    /* A lista de blocos: é aqui que se vê a alocação indexada */
    printf("  %-14s ", "Blocos:");
    if (fcb->quantidade_de_blocos == 0) {
        printf("nenhum");
    }
    for (int i = 0; i < fcb->quantidade_de_blocos; i++) {
        if (i > 0) {
            printf(", ");
        }
        printf("%d", fcb->blocos_do_arquivo[i]);
    }
    printf("  (%d de no máximo %d)\n", fcb->quantidade_de_blocos, MAXIMO_DE_BLOCOS_POR_ARQUIVO);
}


/*
 * cp <origem> <destino> : copia um arquivo
 *
 * A cópia é um arquivo NOVO: ganha outro número de inode
 * e outros blocos no disco, com o mesmo conteúdo.
 */
void comando_cp(const char *origem, const char *destino)
{
    NoArvore *arquivo_origem = encontrar_arquivo("cp", origem);
    if (arquivo_origem == NULL) {
        return;
    }
    if (!usuario_tem_permissao(&arquivo_origem->fcb, &usuario_atual, PERMISSAO_DE_LEITURA)) {
        printf("cp: '%s': permissão negada para ler\n", origem);
        return;
    }

    NoArvore *pasta_destino;
    char nome_destino[TAMANHO_MAXIMO_NOME];
    if (!decidir_destino(destino, arquivo_origem->fcb.nome, &pasta_destino, nome_destino)) {
        printf("cp: '%s': a pasta de destino não existe\n", destino);
        return;
    }

    NoArvore *arquivo_destino = procurar_filho_pelo_nome(pasta_destino, nome_destino);
    bool criou_arquivo_novo = false;

    if (arquivo_destino != NULL) {
        /* Já existe algo com esse nome: vamos sobrescrever (igual ao Linux) */
        if (arquivo_destino == arquivo_origem) {
            printf("cp: origem e destino são o mesmo arquivo\n");
            return;
        }
        if (e_diretorio(arquivo_destino)) {
            printf("cp: '%s': já existe uma pasta com esse nome\n", nome_destino);
            return;
        }
        if (!usuario_tem_permissao(&arquivo_destino->fcb, &usuario_atual, PERMISSAO_DE_ESCRITA)) {
            printf("cp: '%s': permissão negada para escrever\n", nome_destino);
            return;
        }
    } else {
        if (!nome_e_valido(nome_destino)) {
            printf("cp: nome inválido: '%s'\n", destino);
            return;
        }
        if (!usuario_tem_permissao(&pasta_destino->fcb, &usuario_atual, PERMISSAO_DE_ESCRITA)) {
            printf("cp: permissão negada para criar arquivos na pasta de destino\n");
            return;
        }
        arquivo_destino = criar_no(nome_destino, arquivo_origem->fcb.tipo);
        if (arquivo_destino == NULL) {
            printf("cp: limite de %d arquivos e pastas atingido\n", MAXIMO_DE_NOS);
            return;
        }
        adicionar_filho(pasta_destino, arquivo_destino);
        criou_arquivo_novo = true;
    }

    /* Lê da origem e grava no destino (que recebe blocos próprios) */
    char conteudo[TAMANHO_MAXIMO_DO_ARQUIVO + 1];
    size_t tamanho = ler_conteudo_do_disco(&arquivo_origem->fcb, conteudo, sizeof(conteudo));

    ResultadoDaGravacao resultado = gravar_conteudo_no_disco(&arquivo_destino->fcb, conteudo, tamanho);
    if (resultado != GRAVOU_COM_SUCESSO) {
        mostrar_erro_de_gravacao("cp", resultado);
        /* Não deixamos um arquivo vazio "pela metade" para trás */
        if (criou_arquivo_novo) {
            liberar_no_e_todos_os_filhos(arquivo_destino);
        }
        return;
    }

    arquivo_destino->fcb.tipo       = arquivo_origem->fcb.tipo;
    arquivo_destino->fcb.permissoes = arquivo_origem->fcb.permissoes;
    arquivo_destino->fcb.data_de_modificacao = time(NULL);
    arquivo_origem->fcb.data_do_ultimo_acesso = time(NULL);
}


/*
 * mv <origem> <destino> : move ou renomeia (arquivo ou pasta)
 *
 * Diferente do cp, o mv NÃO copia o conteúdo: o inode e os blocos
 * continuam os mesmos. Só mudamos o nó de lugar na árvore
 * (e/ou trocamos o nome). Por isso o mv é instantâneo no Linux.
 */
void comando_mv(const char *origem, const char *destino)
{
    NoArvore *item = encontrar_pelo_caminho(origem);
    if (item == NULL) {
        printf("mv: '%s': não encontrado\n", origem);
        return;
    }
    if (item == raiz) {
        printf("mv: não é possível mover a raiz\n");
        return;
    }

    NoArvore *pasta_destino;
    char nome_destino[TAMANHO_MAXIMO_NOME];
    if (!decidir_destino(destino, item->fcb.nome, &pasta_destino, nome_destino)) {
        printf("mv: '%s': a pasta de destino não existe\n", destino);
        return;
    }

    /* Mover uma pasta para dentro dela mesma criaria um "laço" na árvore */
    if (esta_dentro_de(pasta_destino, item)) {
        printf("mv: não é possível mover uma pasta para dentro dela mesma\n");
        return;
    }
    if (!nome_e_valido(nome_destino)) {
        printf("mv: nome inválido: '%s'\n", destino);
        return;
    }

    NoArvore *ja_existe = procurar_filho_pelo_nome(pasta_destino, nome_destino);
    if (ja_existe == item) {
        return;   /* origem e destino iguais: não há nada a fazer */
    }
    if (ja_existe != NULL) {
        printf("mv: '%s' já existe no destino\n", nome_destino);
        return;
    }

    /* Precisa poder escrever nas DUAS pastas: tirar de uma e colocar na outra */
    NoArvore *pasta_origem = pasta_de_cima(item);
    bool pode_tirar   = usuario_tem_permissao(&pasta_origem->fcb,  &usuario_atual, PERMISSAO_DE_ESCRITA);
    bool pode_colocar = usuario_tem_permissao(&pasta_destino->fcb, &usuario_atual, PERMISSAO_DE_ESCRITA);
    if (!pode_tirar || !pode_colocar) {
        printf("mv: permissão negada\n");
        return;
    }

    /* Mover = só trocar o índice da pasta-mãe anotado no nó */
    strncpy(item->fcb.nome, nome_destino, TAMANHO_MAXIMO_NOME - 1);
    adicionar_filho(pasta_destino, item);

    time_t agora = time(NULL);
    pasta_origem->fcb.data_de_modificacao  = agora;
    pasta_destino->fcb.data_de_modificacao = agora;
}


/*
 * ./<arquivo> : "executa" um arquivo
 *
 * Não roda nada de verdade: serve para demonstrar a permissão de
 * EXECUÇÃO (x). Se o usuário tiver o x, mostramos o conteúdo como se
 * fosse a saída do programa; se não tiver, a execução é bloqueada.
 */
void comando_executar(const char *caminho)
{
    NoArvore *arquivo = encontrar_arquivo("executar", caminho);
    if (arquivo == NULL) {
        return;
    }

    if (!usuario_tem_permissao(&arquivo->fcb, &usuario_atual, PERMISSAO_DE_EXECUCAO)) {
        printf("%s: permissão negada (o arquivo não tem permissão de execução para você)\n", caminho);
        return;
    }

    char conteudo[TAMANHO_MAXIMO_DO_ARQUIVO + 1];
    ler_conteudo_do_disco(&arquivo->fcb, conteudo, sizeof(conteudo));

    printf("[executando '%s' (simulado)]\n", arquivo->fcb.nome);
    printf("%s", conteudo);
    arquivo->fcb.data_do_ultimo_acesso = time(NULL);
}
