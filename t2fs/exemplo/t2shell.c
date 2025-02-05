
/**

    T2 shell, para teste do T2FS - Sistema de arquivos do trabalho 2 de Sistemas Operacionais I

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/t2fs.h"


void cmdMan(void);

void cmdFormat(void);

void cmdWho(void);
void cmdLs(void);
void cmdMkdir(void);
void cmdRmdir(void);

void cmdOpen(void);
void cmdRead(void);
void cmdClose(void);

void cmdWrite(void);
void cmdCreate(void);
void cmdDelete(void);

void cmdCp(void);
void cmdFscp(void);

void cmdGetCW(void);
void cmdChangeCW(void);


void cmdExit(void);

static void dump(char *buffer, int size) {
    int base, i;
    char c;
    for (base=0; base<size; base+=16) {
        printf ("%04d ", base);
        for (i=0; i<16; ++i) {
            if (base+i<size) printf ("%02X ", buffer[base+i]);
            else printf ("   ");
        }

        printf (" *");

        for (i=0; i<16; ++i) {
            if (base+i<size) c = buffer[base+i];
            else c = ' ';

            if (c<' ' || c>'z' ) c = ' ';
            printf ("%c", c );
        }
        printf ("*\n");
    }
}



char helpExit[]  = "             -> finish this shell";
char helpMan[]   = "[comando]    -> command help";
char helpWho[]   = "             -> shows T2FS authors";
char helpLs[]    = "[pahname]    -> list files in [pathname]";
char helpMkdir[] = "[dirname]    -> create [dirname] in T2FS";
char helpRmdir[] = "[dirname]    -> deletes [dirname] from T2FS";
char helpOpen[]  = "[file]       -> open [file] from T2FS";
char helpRead[]  = "[hdl] [siz]  -> read [siz] bytes from file [hdl]";
char helpClose[] = "[hdl         -> close [hdl]";
char helpWrite[] = "[hdl] [str]  -> write [str] bytes to file [hdl]";
char helpCreate[]= "[file]       -> create new [file] in T2FS";
char helpDelete[]= "[file]       -> deletes [file] from T2FS";
char helpSeek[]  = "[hdl] [pos]  -> set CP of [hdl] file on [pos]";
char helpTrunc[] = "[hdl] [siz]  -> truncate file [hdl] to [siz] bytes";
char helpLn[]     = "[lnk] [file] -> create link [lnk] to [file]";
char helpFormat[] = "[bs]         -> format virtual disk";

char helpCopy[]    = "[src] [dst]  -> copy files: [src] -> [dst]";
char helpFscp[]	  =	"[src] [dst]  -> copy files: [src] -> [dst]"
					"\n    fscp -t [src] [dst]  -> copy HostFS to T2FS"
					"\n    fscp -f [src] [dst]  -> copy T2FS   to HostFS";

char helpChangeCW[] = "[pathname]   -> change to [pathname]";
char helpGetCW[] = "             -> shows Current Path";


struct {
	char name[20];
	char *helpString;
	void (*f)(void);
} cmdList[] = {
	{ "exit", helpExit, cmdExit }, { "x", helpExit, cmdExit },
	{ "man", helpMan, cmdMan },
	{ "who", helpWho, cmdWho }, { "id", helpWho, cmdWho },
	{ "dir", helpLs, cmdLs }, { "ls", helpLs, cmdLs },

	{ "open", helpOpen, cmdOpen },
	{ "read", helpRead, cmdRead }, { "rd", helpRead, cmdRead },
	{ "close", helpClose, cmdClose }, { "cl", helpClose, cmdClose },
	{ "write", helpWrite, cmdWrite }, { "wr", helpWrite, cmdWrite },
	{ "create", helpCreate, cmdCreate }, { "cr", helpCreate, cmdCreate },
	{ "delete", helpDelete, cmdDelete }, { "del", helpDelete, cmdDelete },

	{ "format", helpFormat, cmdFormat },

	{ "cp", helpCopy, cmdCp },
	{ "fscp", helpFscp, cmdFscp },
	{ "fim", helpExit, NULL }
};



void tst_identify() {
    char name[256];
	int err;

	printf ("Teste do identify()\n");

    err = identify2(name, 256);
    if (err) {
        printf ("Erro: %d\n", err);
        return;
    }

	printf ("Ok!\n\n");
}

void tst_open(char *src) {
    FILE2 hSrc;

	printf ("Teste do open() e close()\n");

	hSrc = open2 (src);

    if (hSrc<0) {
        printf ("Erro: Open %s (handle=%d)\n", src, hSrc);
        return;
    }

	if (close2(hSrc)) {
        printf ("Erro: Close (handle=%d)\n", hSrc);
        return;
	}

	printf ("Ok!\n\n");
}

void tst_read(char *src) {
	char buffer[256];
    FILE2 hSrc;

	printf ("Teste do read()\n");

    hSrc = open2 (src);
    if (hSrc<0) {
        printf ("Erro: Open %s (handle=%d)\n", src, hSrc);
        return;
    }

    int err = read2(hSrc, buffer, 256);
    if (err<0) {
        printf ("Error: Read %s (handle=%d), err=%d\n", src, hSrc, err);
		close2(hSrc);
        return;
    }
    if (err==0) {
        printf ("Error: Arquivo vazio %s (handle=%d)\n", src, hSrc);
		close2(hSrc);
        return;
    }

    dump(buffer, err);

	if (close2(hSrc)) {
        printf ("Erro: Close (handle=%d)\n", hSrc);
        return;
	}
	printf ("Ok!\n\n");
}

void tst_list_dir(char *src) {
    int d;
	int n;

	printf ("Teste do opendir(), readdir() e closedir()\n");

    // Abre o diret�rio pedido
    d = opendir2();
    if (d<0) {
        printf ("Erro: Opendir %s (handle=%d)\n", src, d);
        return;
    }

    // Coloca diretorio na tela
    DIRENT2 dentry;
    while ( readdir2(&dentry) == 0 ) {
        printf ("%c %8u %s\n", (dentry.fileType==0x02?'d':'-'), dentry.fileSize, dentry.name);
    }

    n = closedir2();
	if (n) {
        printf ("Erro: Closedir %s (handle=%d)\n", src, d);
        return;
	}
	printf ("Ok!\n\n");
}

void tst_create(char *src) {
    FILE2 hFile;
	int err;

	printf ("Teste do create2()\n");

    hFile = create2 (src);
    if (hFile<0) {
        printf ("Error: Create %s, handle=%d\n", src, hFile);
        return;
    }

	err = close2(hFile);
	if (err) {
        printf ("Erro: Close %s, handle=%d, err=%d\n", src, hFile, err);
        return;
	}

	printf ("Ok!\n\n");
}

void tst_write(char *src, char *texto) {
    FILE2 handle;
	int err;

	printf ("Teste do write2()\n");

    handle = open2 (src);
    if (handle<0) {
        printf ("Erro: Open %s, handle=%d (PROVAVEL CAUSA = arquivo nao existe)\n", src, handle);
        return;
    }

    err = write2(handle, texto, strlen(texto));
    if (err<0) {
        printf ("Error: Write %s, handle=%d, err=%d\n", src, handle, err);
		close2(handle);
        return;
    }

	if (close2(handle)) {
        printf ("Erro: Close %s, handle=%d\n", src, handle);
        return;
	}

	printf ("Ok!\n\n");

}

void tst_delete(char *src) {
    int err;

	printf ("Teste do delete2()\n");

	err = delete2(src);
	if (err) {
        printf ("Erro: Delete %s, err=%d\n", src, err);
        return;
	}

	printf ("Ok!\n\n");

}




void teste(int tstNumber) {
	if (tstNumber<0) {
		printf (" 1 - identify2()\n");
		printf (" 2 - open        open2,close2          [x.txt]\n");
		printf (" 3 - read        open2,read2,close2    [x.txt]\n");
		printf (" 4 - list_dir                          [.]\n");
		printf (" 5 - seek        open2,seek2,close2    [x.txt; 7]\n");

		printf (" 6 - create      create2,close2        [y.txt]\n");
		printf (" 7 - write       open,write,close      [y.txt, abced...]\n");
		printf (" 8 - truncate    open,truncate,close   [y.txt, 11]\n");
		printf (" 9 - delete      delete2               [y.txt]\n");

		return;
	}
	switch(tstNumber) {
		case 1:
			tst_identify();
			break;
		case 2:
			tst_open("x.txt");
			break;
		case 3:
			tst_read("x.txt");
			break;
		case 4:
			tst_list_dir(".");
			break;
		case 5:
			break;

		case 6:
			tst_create("y.txt");
			tst_list_dir(".");		// Verifica��o
			break;
		case 7:
			tst_write("y.txt", "[abcdefghijklmnopqrst]");
			tst_read("y.txt");		// Verifica��o
			break;
		case 8:
			tst_read("y.txt");		// Verifica��o
			break;
		case 9:
			tst_delete("y.txt");
			tst_list_dir(".");		// Verifica��o
			break;

	}
}


int main()
{
    char cmd[256];
    char *token;
    int i,n;
    int flagAchou, flagEncerrar;

    printf ("Testing for T2FS - v 2018.1.2\n");
    //token = strtok("who"," \t");
    strcpy(cmd, "man");
    token = strtok(cmd," \t");
    cmdMan();

    flagEncerrar = 0;
    while (1) {
        printf ("T2FS> ");
        gets(cmd);
        if( (token = strtok(cmd," \t")) != NULL ) {
			// Verifica se � comando de teste
			n = atoi(token);
			if (n) {
				teste(n);
				continue;
			}
			//
			flagAchou = 0;
			for (i=0; strcmp(cmdList[i].name,"fim")!=0; i++) {
				if (strcmp(cmdList[i].name, token)==0) {
					flagAchou = 1;
					cmdList[i].f();
					if (cmdList[i].f==cmdExit) {
						flagEncerrar = 1;
						break;
					}
				}
			}
			if (!flagAchou) printf ("???\n");
        }
		if (flagEncerrar) break;
    }
    return 0;
}

/**
Encerra a opera��o do teste
*/
void cmdExit(void) {
    printf ("bye, bye!\n");
}

/**
Informa os comandos aceitos pelo programa de teste
*/
void cmdMan(void) {
	int i;
	char *token = strtok(NULL," \t");

	// man sem nome de comando
	if (token==NULL) {
		for (i=0; strcmp(cmdList[i].name,"fim")!=0; i++) {
			printf ("%-10s", cmdList[i].name);
			if (i%6==5) printf ("\n");
		}
		printf ("\n");
		return;
	}

	// man com nome de comando
	for (i=0; strcmp(cmdList[i].name,"fim")!=0; i++) {
		if (strcmp(cmdList[i].name,token)==0) {
			printf ("%-10s %s\n", cmdList[i].name, cmdList[i].helpString);
		}
	}


}


/**
Chama a fun��o que formata o disco
*/
void cmdFormat(void) {
	int sectors_per_block;

    char *token = strtok(NULL," \t");
    if (token==NULL) {
        printf ("Missing block size (in sectors)\n");
        return;
    }
    if (sscanf(token, "%d", &sectors_per_block)==0) {
        printf ("Invalid block size\n");
        return;
    }

    int err = format2 (0, sectors_per_block);
    if (err) {
        printf ("Error: %d\n", err);
        return;
    }

    printf ("Disk formated\n");
}


/**
Chama da fun��o identify2 da biblioteca e coloca o string de retorno na tela
*/
void cmdWho(void) {
    char name[256];
    int err = identify2(name, 256);
    if (err) {
        printf ("Erro: %d\n", err);
        return;
    }
    printf ("%s\n", name);
}

/**
Copia arquivo dentro do T2FS
Os parametros s�o:
    primeiro parametro => arquivo origem
    segundo parametro  => arquivo destino
*/
void cmdCp(void) {

    // Pega os nomes dos arquivos origem e destion
    char *src = strtok(NULL," \t");
    char *dst = strtok(NULL," \t");
    if (src==NULL || dst==NULL) {
        printf ("Missing parameter\n");
        return;
    }
    // Abre o arquivo origem, que deve existir
    FILE2 hSrc = open2 (src);
    if (hSrc<0) {
        printf ("Open source file error: %d\n", hSrc);
        return;
    }
    // Cria o arquivo de destino, que ser� resetado se existir
    FILE2 hDst = create2 (dst);
    if (hDst<0) {
        close2(hSrc);
        printf ("Create destination file error: %d\n", hDst);
        return;
    }
    // Copia os dados de source para destination
    char buffer[2];
    while( read2(hSrc, buffer, 1) == 1 ) {
        write2(hDst, buffer, 1);
    }
    // Fecha os arquicos
    close2(hSrc);
    close2(hDst);

    printf ("Files successfully copied\n");
}

/**
Copia arquivo de um sistema de arquivos para o outro
Os parametros s�o:
    primeiro parametro => dire��o da copia
        -t copiar para o T2FS
        -f copiar para o FS do host
    segundo parametro => arquivo origem
    terceiro parametro  => arquivo destino
*/
void cmdFscp(void) {
    // Pega a dire��o e os nomes dos arquivos origem e destion
    char *direcao = strtok(NULL, " \t");
    char *src = strtok(NULL," \t");
    char *dst = strtok(NULL," \t");
    if (direcao==NULL || src==NULL || dst==NULL) {
        printf ("Missing parameter\n");
        return;
    }
    // Valida dire��o
    if (strncmp(direcao, "-t", 2)==0) {
        // src == host
        // dst == T2FS

        // Abre o arquivo origem, que deve existir
        FILE *hSrc = fopen(src, "r+");
        if (hSrc==NULL) {
            printf ("Open source file error\n");
            return;
        }
        // Cria o arquivo de destino, que ser� resetado se existir
        FILE2 hDst = create2 (dst);
        if (hDst<0) {
            fclose(hSrc);
            printf ("Create destination file error: %d\n", hDst);
            return;
        }
        // Copia os dados de source para destination
        char buffer[2];
        while( fread((void *)buffer, (size_t)1, (size_t)1, hSrc) == 1 ) {
            write2(hDst, buffer, 1);
        }
        // Fecha os arquicos
        fclose(hSrc);
        close2(hDst);
    }
    else if (strncmp(direcao, "-f", 2)==0) {
        // src == T2FS
        // dst == host

        // Abre o arquivo origem, que deve existir
        FILE2 hSrc = open2 (src);
        if (hSrc<0) {
            printf ("Open source file error: %d\n", hSrc);
            return;
        }
        // Cria o arquivo de destino, que ser� resetado se existir
        FILE *hDst = fopen(dst, "w+");
        if (hDst==NULL) {
            printf ("Open destination file error\n");
            return;
        }
        // Copia os dados de source para destination
        char buffer[2];
        while ( read2(hSrc, buffer, 1) == 1 ) {
            fwrite((void *)buffer, (size_t)1, (size_t)1, hDst);
        }
        // Fecha os arquicos
        close2(hSrc);
        fclose(hDst);
    }
    else {
        printf ("Invalid copy direction\n");
        return;
    }

    printf ("Files successfully copied\n");
}

/**
Cria o arquivo informado no parametro
Retorna eventual sinaliza��o de erro
Retorna o HANDLE do arquivo criado
*/
void cmdCreate(void) {
    FILE2 hFile;

    char *token = strtok(NULL," \t");
    if (token==NULL) {
        printf ("Missing parameter\n");
        return;
    }

    hFile = create2 (token);
    if (hFile<0) {
        printf ("Error: %d\n", hFile);
        return;
    }

    printf ("File created with handle %d\n", hFile);
}

/**
Apaga o arquivo informado no parametro
Retorna eventual sinaliza��o de erro
*/
void cmdDelete(void) {

    char *token = strtok(NULL," \t");
    if (token==NULL) {
        printf ("Missing parameter\n");
        return;
    }

    int err = delete2(token);
    if (err<0) {
        printf ("Error: %d\n", err);
        return;
    }

    printf ("File %s was deleted\n", token);
}

/**
Abre o arquivo informado no parametro [0]
Retorna sinaliza��o de erro
Retorna HANDLE do arquivo retornado
*/
void cmdOpen(void) {
    FILE2 hFile;

    char *token = strtok(NULL," \t");
    if (token==NULL) {
        printf ("Missing parameter\n");
        return;
    }

    hFile = open2 (token);
    if (hFile<0) {
        printf ("Error: %d\n", hFile);
        return;
    }

    printf ("File opened with handle %d\n", hFile);
}

/**
Fecha o arquivo cujo handle � o parametro
Retorna sinaliza��o de erro
Retorna mensagem de opera��o completada
*/
void cmdClose(void) {
    FILE2 handle;

    char *token = strtok(NULL," \t");
    if (token==NULL) {
        printf ("Missing parameter\n");
        return;
    }

    if (sscanf(token, "%d", &handle)==0) {
        printf ("Invalid parameter\n");
        return;
    }

    int err = close2(handle);
    if (err<0) {
        printf ("Error: %d\n", err);
        return;
    }

    printf ("Closed file with handle %d\n", handle);
}


void cmdRead(void) {
    FILE2 handle;
    int size;

    // get first parameter => file handle
    char *token = strtok(NULL," \t");
    if (token==NULL) {
        printf ("Missing parameter\n");
        return;
    }
    if (sscanf(token, "%d", &handle)==0) {
        printf ("Invalid parameter\n");
        return;
    }

    // get second parameter => number of bytes
    token = strtok(NULL," \t");
    if (token==NULL) {
        printf ("Missing parameter\n");
        return;
    }
    if (sscanf(token, "%d", &size)==0) {
        printf ("Invalid parameter\n");
        return;
    }

    // Alloc buffer for reading file
    char *buffer = malloc(size);
    if (buffer==NULL) {
        printf ("Memory full\n");
        return;
    }

    // get file bytes
    int err = read2(handle, buffer, size);
    if (err<0) {
        printf ("Error: %d\n", err);
        return;
    }
    if (err==0) {
        printf ("Empty file\n");
        return;
    }

    // show bytes read
    dump(buffer, err);
    printf ("%d bytes read from file-handle %d\n", err, handle);

    free(buffer);
}

void cmdWrite(void) {
    FILE2 handle;
    int size;
    int err;

    // get first parameter => file handle
    char *token = strtok(NULL," \t");
    if (token==NULL) {
        printf ("Missing parameter\n");
        return;
    }
    if (sscanf(token, "%d", &handle)==0) {
        printf ("Invalid parameter\n");
        return;
    }

    // get second parameter => string
    token = strtok(NULL," \t");
    if (token==NULL) {
        printf ("Missing parameter\n");
        return;
    }
    size = strlen(token);

    // get file bytes
    err = write2(handle, token, size);
    if (err<0) {
        printf ("Error: %d\n", err);
        return;
    }
    if (err!=size) {
        printf ("Erro: escritos %d bytes, mas apenas %d foram efetivos\n", size, err);
        return;
    }

    printf ("%d bytes writen to file-handle %d\n", err, handle);
}

void cmdLs(void) {

    // Abre o diret�rio pedido
    int d;
    d = opendir2();
    if (d<0) {
        printf ("Open dir error: %d\n", d);
        return;
    }

    // Coloca diretorio na tela
    DIRENT2 dentry;
    while ( readdir2(&dentry) == 0 ) {
        printf ("%c %8u %s\n", (dentry.fileType==0x02?'d':'-'), dentry.fileSize, dentry.name);
    }

    closedir2();


}


