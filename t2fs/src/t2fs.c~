#include "../include/apidisk.h"
#include "../include/t2disk.h"
#include "../include/t2fs.h"
#include "../include/bitmap2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
Verbosidade do Debug:
0 - Sem debug (para entregar)
1 - Minimo
2 - Bastante verboso
3 - Tudo e qualquer coisa
*/

#define DEBUG_MODE 3

#define MAX_ARQUIVOS_ABERTOS 10

#define NO_ALLOC -20 //Codigo de erro usado quando nao foi possivel alocar um bloco (particao cheia)

//Master Boot Record
typedef struct mbr{
    WORD trabVersion;
    WORD sectorSize;
    WORD partitionTableInit;
    WORD partitionQuant;
    char partitionTable[248];
} MBR;

MBR mbr;
int mbrLoaded = 0;                //Variavel binaria que indica se o MBR esta carregado (primeira coisa a ser feita sempre)

int mountedPartition = -1;        //Numero da particao montada (-1 se nenhuma)
struct t2fs_superbloco mountedSB; //Superbloco montado (somente valido se existe uma particao montada)

int dirOpen = 0;                  //Variavel binaria que indica se o diretorio raiz esta aberto

//Entradas do diretorio em "cache" para facil acesso (lista encadeada)
typedef struct cde2{
    struct cde2* next;
    DIRENT2 dirent;
    int iNodeNumber;
} CACHED_DIRENTS;
CACHED_DIRENTS* cached_dirents = NULL;
CACHED_DIRENTS* current_dirent;

//Arquivos Abertos
typedef struct arq_ab{
    int iNodeNumber;         //Numero do inode do arquivo (usado para idetificar se o arquivo aberto eh valido, iNodeNumber != -1)
    char fileName[256];

    int filePointer;
    int fileSize;            //O mesmo que bytesFileSize do inode. Mantido aqui para nao precisar ler o inode.

    char *arqBuffer;         //Buffer do tamanho de um BLOCO!
    int arqBufferPointer;

    int needsToWriteOnClose; //Para saber se precisa escrever o buffer quando fechar o arquivo

} ARQUIVO_ABERTO;
ARQUIVO_ABERTO arquivosAbertos[MAX_ARQUIVOS_ABERTOS];

//Carrega o MBR
void loadMbr(){
    int i;
    for(i = 0; i < MAX_ARQUIVOS_ABERTOS; i++)
        arquivosAbertos[i].iNodeNumber = -1;

    read_sector(0, (unsigned char *) &mbr);

    mbrLoaded = 1;

    if(DEBUG_MODE)
        printf("\033[0;32mMBR Carregado\n\033[0m");
}

//Le um inode do disco.
void readINode(int partition, int iNodeNumber, struct t2fs_inode *iNode){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + partition*32));
    const int iNodesPerSector = SECTOR_SIZE / sizeof(struct t2fs_inode); //Deve ser uns 8...

    struct t2fs_superbloco oSuper;

    unsigned char buffer[SECTOR_SIZE] = {0};

    read_sector(firstPartitionSector, buffer);
    memcpy(&oSuper, buffer, sizeof(struct t2fs_superbloco));

    int iNodeSector = firstPartitionSector;
    iNodeSector = iNodeSector + oSuper.blockSize * (oSuper.freeBlocksBitmapSize + oSuper.freeInodeBitmapSize + oSuper.superblockSize) + iNodeNumber / iNodesPerSector;

    read_sector(iNodeSector, buffer);
    memcpy(iNode, buffer + (iNodeNumber % iNodesPerSector) * sizeof(struct t2fs_inode), sizeof(struct t2fs_inode));
}

//Le um iNode da particao montada
void readINodeM(int iNodeNumber, struct t2fs_inode *iNode){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int iNodesPerSector = SECTOR_SIZE / sizeof(struct t2fs_inode); //Deve ser uns 8...

    unsigned char buffer[SECTOR_SIZE] = {0};

    int iNodeSector = firstPartitionSector;
    iNodeSector = iNodeSector + mountedSB.blockSize * (mountedSB.freeBlocksBitmapSize + mountedSB.freeInodeBitmapSize + mountedSB.superblockSize) + iNodeNumber / iNodesPerSector;

    read_sector(iNodeSector, buffer);
    memcpy(iNode, buffer + (iNodeNumber % iNodesPerSector) * sizeof(struct t2fs_inode), sizeof(struct t2fs_inode));
}

//Escreve um inode no disco
void writeINode(int partition, int iNodeNumber, struct t2fs_inode iNode){
    int firstPartitionSector = *((int*) (mbr.partitionTable + partition*32));
    int iNodesPerSector = SECTOR_SIZE / sizeof(struct t2fs_inode);

    struct t2fs_superbloco oSuper;

    unsigned char buffer[SECTOR_SIZE] = {0};

    read_sector(firstPartitionSector, buffer);
    memcpy(&oSuper, buffer, sizeof(struct t2fs_superbloco));

    int iNodeSector = firstPartitionSector;
    iNodeSector = iNodeSector + oSuper.blockSize * (oSuper.freeBlocksBitmapSize + oSuper.freeInodeBitmapSize + oSuper.superblockSize) + iNodeNumber / iNodesPerSector;

    read_sector(iNodeSector, buffer);
    memcpy(buffer + (iNodeNumber % iNodesPerSector) * sizeof(struct t2fs_inode), &iNode, sizeof(struct t2fs_inode));
    write_sector(iNodeSector, buffer);
}

//Escreve um inode na particao montada
void writeINodeM(int iNodeNumber, struct t2fs_inode iNode){
    int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    int iNodesPerSector = SECTOR_SIZE / sizeof(struct t2fs_inode);

    unsigned char buffer[SECTOR_SIZE] = {0};

    int iNodeSector = firstPartitionSector;
    iNodeSector = iNodeSector + mountedSB.blockSize * (mountedSB.freeBlocksBitmapSize + mountedSB.freeInodeBitmapSize + mountedSB.superblockSize) + iNodeNumber / iNodesPerSector;

    read_sector(iNodeSector, buffer);
    memcpy(buffer + (iNodeNumber % iNodesPerSector) * sizeof(struct t2fs_inode), &iNode, sizeof(struct t2fs_inode));
    write_sector(iNodeSector, buffer);
}

//Aloca um bloco e adiciona o record nele. Retorna o numero do bloco alocado.
//Retorna NO_ALLOC se nao conseguiu alocar um bloco.
DWORD addRecordToNewBlock(struct t2fs_record record, struct t2fs_inode *iNodeRaiz){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));

    unsigned char buffer[SECTOR_SIZE] = {0};

    openBitmap2(firstPartitionSector);

    DWORD newBlockNum = searchBitmap2(BITMAP_DADOS, 0);

    if((int) newBlockNum <= 0){
      closeBitmap2();
      return NO_ALLOC;
    }

    DWORD newSectorNum = firstPartitionSector + newBlockNum * mountedSB.blockSize;
    setBitmap2(BITMAP_DADOS, newBlockNum, 1);

    closeBitmap2();

    //Preenche o bloco com zeros
    int i;
    for(i = 0; i < mountedSB.blockSize; i++)
        write_sector(newSectorNum + i, buffer);

    memcpy(buffer, &record, sizeof(struct t2fs_record));
    write_sector(newSectorNum, buffer);

    iNodeRaiz->blocksFileSize++;

    return newBlockNum;
}

//Aloca um bloco de indices e o primeiro bloco de dados desse bloco. Adiciona o record no bloco de dados.
//Retorna o numero do bloco de indices alocado, ou NO_ALLOC se nao conseguiu alocar.
DWORD allocateIndirectBlockAndAddRecord(struct t2fs_record record, struct t2fs_inode *iNodeRaiz){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));

    unsigned char buffer[SECTOR_SIZE] = {0};

    //Aloca um bloco para servir de bloco de indices
    openBitmap2(firstPartitionSector);
    DWORD newIndirectionBlockNum = searchBitmap2(BITMAP_DADOS, 0);

    if((int) newIndirectionBlockNum <= 0){
      closeBitmap2();
      return NO_ALLOC;
    }

    setBitmap2(BITMAP_DADOS, newIndirectionBlockNum, 1);
    closeBitmap2();

    DWORD newIndirectionSectorNum = firstPartitionSector + newIndirectionBlockNum * mountedSB.blockSize;
    //Preenche o bloco com zeros
    int i;
    for(i = 1; i < mountedSB.blockSize; i++)
        write_sector(newIndirectionSectorNum + i, buffer);

    DWORD newDataBlock = addRecordToNewBlock(record, iNodeRaiz);

    if((int) newDataBlock == NO_ALLOC){
      openBitmap2(firstPartitionSector);
      setBitmap2(BITMAP_DADOS, newIndirectionBlockNum, 0);
      closeBitmap2();
      return NO_ALLOC;
    }

    memcpy(buffer, &newDataBlock, sizeof(DWORD));
    write_sector(newIndirectionSectorNum, buffer);

    if(DEBUG_MODE > 2)
        printf("\033[0;35mAlocado bloco %d como bloco de dados para o diretorio raiz\n\033[0m", newDataBlock);

    return newIndirectionBlockNum;
}

DWORD allocateDoubleIndirectBlockAndAddRecord(struct t2fs_record record, struct t2fs_inode *iNodeRaiz){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));

    unsigned char buffer[SECTOR_SIZE] = {0};

    //Aloca um bloco para servir de bloco de indices (de blocos de indices)
    openBitmap2(firstPartitionSector);
    DWORD newDoubleIndirectionBlockNum = searchBitmap2(BITMAP_DADOS, 0);

    if((int) newDoubleIndirectionBlockNum <= 0){
      closeBitmap2();
      return NO_ALLOC;
    }

    setBitmap2(BITMAP_DADOS, newDoubleIndirectionBlockNum, 1);
    closeBitmap2();

    DWORD newDoubleIndirectionSectorNum = firstPartitionSector + newDoubleIndirectionBlockNum * mountedSB.blockSize;
    //Preenche o bloco com zeros
    int i;
    for(i = 1; i < mountedSB.blockSize; i++)
        write_sector(newDoubleIndirectionSectorNum + i, buffer);

    DWORD newIndirectDataBlock = allocateIndirectBlockAndAddRecord(record, iNodeRaiz);

    if(newIndirectDataBlock == NO_ALLOC){
      openBitmap2(firstPartitionSector);
      setBitmap2(BITMAP_DADOS, newDoubleIndirectionBlockNum, 0);
      closeBitmap2();
      return NO_ALLOC;
    }

    memcpy(buffer, &newIndirectDataBlock, sizeof(DWORD));
    write_sector(newDoubleIndirectionSectorNum, buffer);

    if(DEBUG_MODE > 2)
        printf("\033[0;35mAlocado bloco %d como bloco de indices para o diretorio raiz\n\033[0m", newIndirectDataBlock);

    return newDoubleIndirectionBlockNum;
}

//Tenta adicionar um record em um bloco de dados.
//Retorna 0 se conseguiu, ou -1 se o bloco esta cheio.
int tryAddRecordToBlock(int blockN, struct t2fs_record record){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int entriesPerSector = SECTOR_SIZE / sizeof(struct t2fs_record);

    int i, j;
    struct t2fs_record auxRecord;

    unsigned char buffer[SECTOR_SIZE] = {0};
    for(i = 0; i < mountedSB.blockSize; i++){
        read_sector(firstPartitionSector + blockN * mountedSB.blockSize + i, buffer);
        for(j = 0; j < entriesPerSector; j++){
            memcpy(&auxRecord, buffer + j * sizeof(struct t2fs_record), sizeof(struct t2fs_record));
            if(auxRecord.TypeVal == 0x00){
                memcpy(buffer + j * sizeof(struct t2fs_record), &record, sizeof(struct t2fs_record));
                write_sector(firstPartitionSector + blockN * mountedSB.blockSize + i, buffer);
                return 0;
            }
        }
    }

    return -1;
}

//Tenta adicionar um record em algum bloco de dados apontado pelo bloco de indices.
//Retorna 0 se conseguiu, ou -1 se o bloco esta cheio, NO_ALLOC se nao existem mais blocos livres;
int tryAddRecordToIndirectBlock(int blockN, struct t2fs_record record, struct t2fs_inode *iNodeRaiz){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int blocksIndexesPerSector = SECTOR_SIZE / sizeof(DWORD);

    int cSector = 0, diskSector;
    DWORD dataBlockN;
    int i;

    unsigned char buffer[SECTOR_SIZE] = {0};

    while(cSector < mountedSB.blockSize){
        diskSector = firstPartitionSector + blockN * mountedSB.blockSize + cSector;
        read_sector(diskSector, buffer);

        for(i = 0; i < blocksIndexesPerSector; i++){
            //Le o proximo ponteiro
            memcpy(&dataBlockN, buffer + i * sizeof(DWORD), sizeof(DWORD));
            if(dataBlockN == 0){ //Se for vazio, beleza! Aloca um bloco e insere o record.
                dataBlockN = addRecordToNewBlock(record, iNodeRaiz);

                if(dataBlockN == NO_ALLOC)
                  return NO_ALLOC;

                memcpy(buffer + i * sizeof(DWORD), &dataBlockN, sizeof(DWORD));
                write_sector(diskSector, buffer);

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mAlocado bloco %d como bloco de dados para o diretorio raiz\n\033[0m", dataBlockN);

                return 0;
            }
            else{ //Se nao for, tenta adicionar no bloco apontado.
                if(tryAddRecordToBlock(dataBlockN, record) == 0)
                    return 0;
            }
        }

        cSector++;
    }

    return -1;
}

//Tenta adicionar um record em algum lugar...
//Retorna 0 se conseguiu, ou -1 se o bloco esta cheio (Nesse caso ja era...), ou NO_ALLOC se nao conseguiu alocar blocos.
int tryAddRecordToDoubleIndirectBlock(int blockN, struct t2fs_record record, struct t2fs_inode *iNodeRaiz){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int blocksIndexesPerSector = SECTOR_SIZE / sizeof(DWORD);

    int cSector = 0, diskSector;
    DWORD dataBlockN;
    int i;

    unsigned char buffer[SECTOR_SIZE] = {0};

    while(cSector < mountedSB.blockSize){
        diskSector = firstPartitionSector + blockN * mountedSB.blockSize + cSector;
        read_sector(diskSector, buffer);

        for(i = 0; i < blocksIndexesPerSector; i++){
            //Le o proximo ponteiro
            memcpy(&dataBlockN, buffer + i * sizeof(DWORD), sizeof(DWORD));
            if(dataBlockN == 0){ //Se for vazio, beleza! Aloca um bloco e insere o record.
                dataBlockN = allocateIndirectBlockAndAddRecord(record, iNodeRaiz);

                if(dataBlockN == NO_ALLOC)
                  return NO_ALLOC;

                memcpy(buffer + i * sizeof(DWORD), &dataBlockN, sizeof(DWORD));
                write_sector(diskSector, buffer);

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mAlocado bloco %d como bloco de indices para o diretorio raiz\n\033[0m", dataBlockN);

                return 0;
            }
            else{ //Se nao for, tenta adicionar no bloco apontado.
                int result = tryAddRecordToIndirectBlock(dataBlockN, record, iNodeRaiz);
                if(result == 0)
                    return 0;

                if(result == NO_ALLOC)
                  return NO_ALLOC;
            }
        }

        cSector++;
    }

    return -1;
}

//Adiciona um "record" no diretorio raiz da particao montada
//Retorna 0 se conseguiu, ou -1 se nao conseguiu, ou NO_ALLOC se nao existem blocos livres.
int addRecord(struct t2fs_record record){
    struct t2fs_inode iNodeRaiz;
    struct t2fs_inode iNodeRecord;
    readINodeM(0, &iNodeRaiz);
    readINodeM(record.inodeNumber, &iNodeRecord);

    if(iNodeRaiz.dataPtr[0] == 0){
        iNodeRaiz.dataPtr[0] = addRecordToNewBlock(record, &iNodeRaiz);

        if(iNodeRaiz.dataPtr[0] == NO_ALLOC)
          return NO_ALLOC;

        if(DEBUG_MODE > 2)
            printf("\033[0;35mAlocado bloco %d como primeiro bloco de dados para o diretorio raiz\n\033[0m", iNodeRaiz.dataPtr[0]);
    }
    else{
        if(tryAddRecordToBlock(iNodeRaiz.dataPtr[0], record) == -1){
            if(iNodeRaiz.dataPtr[1] == 0){
                iNodeRaiz.dataPtr[1] = addRecordToNewBlock(record, &iNodeRaiz);

                if(iNodeRaiz.dataPtr[1] == NO_ALLOC)
                  return NO_ALLOC;

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mAlocado bloco %d como segundo bloco de dados para o diretorio raiz\n\033[0m", iNodeRaiz.dataPtr[1]);
            }
            else{
                int result = tryAddRecordToBlock(iNodeRaiz.dataPtr[1], record);

                if(result == NO_ALLOC)
                  return NO_ALLOC;

                if(result == -1){
                    if(iNodeRaiz.singleIndPtr == 0){
                        iNodeRaiz.singleIndPtr = allocateIndirectBlockAndAddRecord(record, &iNodeRaiz);

                        if(iNodeRaiz.singleIndPtr == NO_ALLOC)
                            return NO_ALLOC;

                        if(DEBUG_MODE > 2)
                            printf("\033[0;35mAlocado bloco %d como bloco de indices para o ponteiro de indirecao simples do diretorio raiz\n\033[0m", iNodeRaiz.singleIndPtr);
                    }
                    else{
                        int result = tryAddRecordToIndirectBlock(iNodeRaiz.singleIndPtr, record, &iNodeRaiz);

                        if(result == NO_ALLOC)
                          return NO_ALLOC;

                        if(result == -1){
                            if(iNodeRaiz.doubleIndPtr == 0){
                                iNodeRaiz.doubleIndPtr = allocateDoubleIndirectBlockAndAddRecord(record, &iNodeRaiz);

                                if(iNodeRaiz.doubleIndPtr == NO_ALLOC)
                                    return NO_ALLOC;

                                if(DEBUG_MODE > 2)
                                    printf("\033[0;35mAlocado bloco %d como bloco de indices de blocos de indice para o ponteiro de indirecao dupla do diretorio raiz\n\033[0m", iNodeRaiz.doubleIndPtr);
                            }
                            else{
                                int result = tryAddRecordToDoubleIndirectBlock(iNodeRaiz.doubleIndPtr, record, &iNodeRaiz);

                                if(result == NO_ALLOC)
                                  return NO_ALLOC;

                                if(result == -1)
                                    return -1;
                            }
                        }
                    }
                }
            }
        }
    }

    writeINodeM(0, iNodeRaiz);

    if(cached_dirents == NULL){
        cached_dirents = malloc(sizeof(CACHED_DIRENTS));
        strcpy(cached_dirents->dirent.name, record.name);
        cached_dirents->iNodeNumber = record.inodeNumber;
        cached_dirents->dirent.fileType = 0x01;
        cached_dirents->dirent.fileSize = iNodeRecord.bytesFileSize;
        cached_dirents->next = NULL;
        current_dirent = cached_dirents;
    }
    else{
        CACHED_DIRENTS* aux = cached_dirents;
        while(aux->next != NULL)
            aux = aux->next;

        aux->next = malloc(sizeof(CACHED_DIRENTS));
        aux = aux->next;
        strcpy(aux->dirent.name, record.name);
        aux->iNodeNumber = record.inodeNumber;
        aux->dirent.fileType = 0x01;
        aux->dirent.fileSize = iNodeRecord.bytesFileSize;
        aux->next = NULL;
    }

    if(DEBUG_MODE > 1)
        printf("\033[0;33mAdicionado record para o arquivo %s\n\033[0m", record.name);

    return 0;
}

int tryRemoveRecordFromBlock(DWORD blockN, char* filename){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int recordsPerSector = SECTOR_SIZE / sizeof(struct t2fs_record);

    struct t2fs_record record;

    unsigned char buffer[SECTOR_SIZE] = {0};

    int i, j;
    for(i = 0; i < mountedSB.blockSize; i++){
        read_sector(firstPartitionSector + blockN * mountedSB.blockSize + i, buffer);
        for(j = 0; j < recordsPerSector; j++){
            memcpy(&record, buffer + j * sizeof(struct t2fs_record), sizeof(struct t2fs_record));
            if(record.TypeVal != 0x00 && !strcmp(record.name, filename)){
                record.TypeVal = 0x00;
                memcpy(buffer + j * sizeof(struct t2fs_record), &record, sizeof(struct t2fs_record));
                write_sector(firstPartitionSector + blockN * mountedSB.blockSize + i, buffer);
                return 0;
            }
        }
    }

    return -1;
}

int tryRemoveRecordFromIndirectBlock(DWORD blockN, char* filename){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int blocksPerIndirectionBlockSector = SECTOR_SIZE / sizeof(DWORD);

    unsigned char buffer[SECTOR_SIZE] = {0};

    DWORD dataBlockN;

    int i, j;
    for(i = 0; i < mountedSB.blockSize; i++){
        read_sector(firstPartitionSector + blockN * mountedSB.blockSize + i, buffer);
        for(j = 0; j < blocksPerIndirectionBlockSector; j++){
            memcpy(&dataBlockN, buffer + j * sizeof(DWORD), sizeof(DWORD));
            if(dataBlockN != 0){
                if(tryRemoveRecordFromBlock(dataBlockN, filename) == 0)
                    return 0;
            }
            //Else pode retornar -1
        }
    }

    return -1;
}

int tryRemoveRecordFromDoubleIndirectBlock(DWORD blockN, char* filename){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int blocksPerIndirectionBlockSector = SECTOR_SIZE / sizeof(DWORD);

    unsigned char buffer[SECTOR_SIZE] = {0};

    DWORD indirectBlockN;

    int i, j;
    for(i = 0; i < mountedSB.blockSize; i++){
        read_sector(firstPartitionSector + blockN * mountedSB.blockSize + i, buffer);
        for(j = 0; j < blocksPerIndirectionBlockSector; j++){
            memcpy(&indirectBlockN, buffer + j * sizeof(DWORD), sizeof(DWORD));
            if(indirectBlockN != 0){
                if(tryRemoveRecordFromIndirectBlock(indirectBlockN, filename) == 0)
                    return 0;
            }
            //Else pode retornar -1
        }
    }

    return -1;
}

//Retorna 0 se removeu o record, ou -1 se nao encontrou
int removeRecord(char *filename){

    struct t2fs_inode iNodeRaiz;
    readINodeM(0, &iNodeRaiz);

    if(iNodeRaiz.dataPtr[0] == 0)
        return -1;
    if(tryRemoveRecordFromBlock(iNodeRaiz.dataPtr[0], filename) == -1){
        if(iNodeRaiz.dataPtr[1] == 0)
            return -1;
        if(tryRemoveRecordFromBlock(iNodeRaiz.dataPtr[1], filename) == -1){
            if(iNodeRaiz.singleIndPtr == 0)
                return -1;
            if(tryRemoveRecordFromIndirectBlock(iNodeRaiz.singleIndPtr, filename) == -1){
                if(iNodeRaiz.doubleIndPtr == 0)
                    return -1;
                return tryRemoveRecordFromDoubleIndirectBlock(iNodeRaiz.doubleIndPtr, filename);
            }
        }
    }

    if(DEBUG_MODE > 1)
        printf("\033[0;33mRemovido record do arquivo %s\n\033[0m", filename);

    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size) {
  strncpy (name, "Bernardo Hummes - 287689\nIvan Peter Lamb - 287692\nMaria Cecilia - 287703", size);
  return 0;
}

/*-----------------------------------------------------------------------------
Função:	Formata logicamente uma partição do disco virtual t2fs_disk.dat para o sistema de
		arquivos T2FS definido usando blocos de dados de tamanho
		corresponde a um múltiplo de setores dados por sectors_per_block.
-----------------------------------------------------------------------------*/
int format2(int partition, int sectors_per_block) {
	if(!mbrLoaded)
        loadMbr();

    if(mbr.partitionQuant <= partition || partition < 0 || sectors_per_block < 1)
        return -1;

    const int firstPartitionSector = *((int*) (mbr.partitionTable + partition*32));
    const int lastPartitionSector =  *((int*) (mbr.partitionTable + partition*32 + 4));
    const int partitionSectors = lastPartitionSector - firstPartitionSector + 1;

    const int iNodesPerSector = SECTOR_SIZE / sizeof(struct t2fs_inode);

    unsigned char buffer[SECTOR_SIZE] = {0};

    //Zera toda a particao. Realmente nao precisa disso, mas por que nao?
    int i;
    for(i = firstPartitionSector; i <= lastPartitionSector; i++)
        write_sector(i, buffer);

    //Preenche o Superbloco
    struct t2fs_superbloco oSuper;

    oSuper.id[0] = 'T';
    oSuper.id[1] = '2';
    oSuper.id[2] = 'F';
    oSuper.id[3] = 'S';

    oSuper.version = 0x7E32;
    oSuper.superblockSize = 1;

    //Setores por bloco e tamanho da particao
    oSuper.blockSize = sectors_per_block;
    oSuper.diskSize = partitionSectors / sectors_per_block;

    //Tamanho da area de iNodes (10% dos blocos)
    oSuper.inodeAreaSize = (oSuper.diskSize / 10);
    if(oSuper.inodeAreaSize * 10 != oSuper.diskSize)
        oSuper.inodeAreaSize++;

    //Tamanho do bitmap de blocos livres
    oSuper.freeBlocksBitmapSize = oSuper.diskSize / (SECTOR_SIZE * 8 * oSuper.blockSize);
    if(oSuper.freeBlocksBitmapSize * (SECTOR_SIZE * 8 * oSuper.blockSize) != oSuper.diskSize)
        oSuper.freeBlocksBitmapSize++;

    //Tamanho do bitmap de iNodes
    oSuper.freeInodeBitmapSize = (oSuper.inodeAreaSize * oSuper.blockSize * iNodesPerSector) / (SECTOR_SIZE * 8 * oSuper.blockSize);
    if(oSuper.freeInodeBitmapSize * (SECTOR_SIZE * 8 * oSuper.blockSize) != (oSuper.inodeAreaSize * oSuper.blockSize * iNodesPerSector))
        oSuper.freeInodeBitmapSize++;

    oSuper.Checksum = 0;

    memcpy(buffer, &oSuper, sizeof(struct t2fs_superbloco));

    //Calcula o Checksum
    for(i = 0; i < 5; i++){
        oSuper.Checksum = oSuper.Checksum + *((unsigned int *) (buffer + 4*i));
    }
    memcpy(buffer + 20, &oSuper.Checksum, 4);

    //Escreve o superbloco no disco
    write_sector(firstPartitionSector, buffer);

    if(DEBUG_MODE){
        printf("\033[0;32m\nFormatada a particao %d, Superbloco:\n", partition);
        read_sector(firstPartitionSector, buffer);
        memcpy(&oSuper, buffer, sizeof(struct t2fs_superbloco));
        printf("ID: %c%c%c%c\n", oSuper.id[0], oSuper.id[1], oSuper.id[2], oSuper.id[3]);
        printf("Version: %04X\n", oSuper.version);
        printf("SuperblockSize: %d\n", oSuper.superblockSize);
        printf("FreeBlocsBitmapSize: %d\n", oSuper.freeBlocksBitmapSize);
        printf("FreeInodeBitmapSize: %d\n", oSuper.freeInodeBitmapSize);
        printf("InodeAreaSize: %d\n", oSuper.inodeAreaSize);
        printf("BlockSize: %d\nDiskSize: %d\n", oSuper.blockSize, oSuper.diskSize);
        printf("Checksum: %d", oSuper.Checksum);
        printf("\033[0m\n\n");
    }

    //Limpa os Bitmaps
    openBitmap2(firstPartitionSector);
    for(i = 0; i < oSuper.diskSize; i++)
        setBitmap2(BITMAP_DADOS, i, 0);

    for(i = 0; i < oSuper.superblockSize; i++){
        setBitmap2(BITMAP_DADOS, i, 1);

        if(DEBUG_MODE > 2)
            printf("\033[0;35mAlocado bloco %d como superbloco\n\033[0m", i);
    }

    for(i = 0; i < oSuper.freeBlocksBitmapSize; i++){
        setBitmap2(BITMAP_DADOS, oSuper.superblockSize + i, 1);

        if(DEBUG_MODE > 2)
            printf("\033[0;35mAlocado bloco %d como bitmap de blocos livres\n\033[0m", oSuper.superblockSize + i);
    }

    for(i = 0; i < oSuper.freeInodeBitmapSize; i++){
        setBitmap2(BITMAP_DADOS, oSuper.superblockSize + oSuper.freeBlocksBitmapSize + i, 1);

        if(DEBUG_MODE > 2)
            printf("\033[0;35mAlocado bloco %d como bitmap de inodes livres\n\033[0m", oSuper.superblockSize + oSuper.freeBlocksBitmapSize + i);
    }

    for(i = 0; i < oSuper.inodeAreaSize; i++){
        setBitmap2(BITMAP_DADOS, oSuper.superblockSize + oSuper.freeBlocksBitmapSize + oSuper.freeInodeBitmapSize + i, 1);

        if(DEBUG_MODE > 2)
            printf("\033[0;35mAlocado bloco %d como bloco para inodes\n\033[0m", oSuper.superblockSize + oSuper.freeBlocksBitmapSize + oSuper.freeInodeBitmapSize + i);
    }

    for(i = 0; i < oSuper.inodeAreaSize * iNodesPerSector * oSuper.blockSize; i++)
        setBitmap2(BITMAP_INODE, i, 0);

    //Cria o diretorio raiz
    setBitmap2(BITMAP_INODE, 0, 1);

    struct t2fs_inode iNodeRaiz;
    iNodeRaiz.blocksFileSize = 0;
    iNodeRaiz.bytesFileSize = 0;
    iNodeRaiz.dataPtr[0] = 0;
    iNodeRaiz.dataPtr[1] = 0;
    iNodeRaiz.singleIndPtr = 0;
    iNodeRaiz.doubleIndPtr = 0;
    iNodeRaiz.RefCounter = 1;

    writeINode(partition, 0, iNodeRaiz);

    closeBitmap2();

    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Monta a partição indicada por "partition" no diretório raiz

Erros: -1 = Ponto de montagem (Raiz) nao esta livre
       -2 = Superbloco invalido (Particao nao formatada)
       -3 = Particao nao existe
-----------------------------------------------------------------------------*/
int mount(int partition) {
    if(!mbrLoaded)
        loadMbr();

    if(mbr.partitionQuant <= partition || partition < 0){

        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao montar particao %d: Particao nao existe\n\033[0m", partition);

        return -3;
    }

    if(mountedPartition == -1){
        unsigned char buffer[SECTOR_SIZE] = {0};

        read_sector(*((int*) (mbr.partitionTable + partition*32)), buffer);
        memcpy(&mountedSB, buffer, sizeof(struct t2fs_superbloco));

        //Verifica o Checksum
        int i;
        int cs = 0;
        for(i = 0; i < 5; i++)
            cs = cs + *((unsigned int *) (buffer + 4*i));

        if(cs != mountedSB.Checksum || cs == 0){

            if(DEBUG_MODE)
                printf("\033[0;31mFalha ao montar particao %d: Superbloco invalido\n\033[0m", partition);

            return -2;
        }

        mountedPartition = partition;

        if(DEBUG_MODE)
            printf("\033[0;32mMontada particao %d\n\033[0m", partition);

        return 0;
    }

    if(DEBUG_MODE)
        printf("\033[0;31mFalha ao montar particao %d: Ponto de montagem ocupado\n\033[0m", partition);

	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Desmonta a partição atualmente montada, liberando o ponto de montagem.

Erros: -1 Ponto de montagem vazio
-----------------------------------------------------------------------------*/
int umount(void) {
    if(mountedPartition == -1){
        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao desmontar particao: Nenhuma particao montada\n\033[0m");
    }

    if(DEBUG_MODE)
        printf("\033[0;32mDesmontada particao %d\n\033[0m", mountedPartition);

    mountedPartition = -1;

    int i;
    for(i = 0; i < MAX_ARQUIVOS_ABERTOS; i++)
        arquivosAbertos[i].iNodeNumber = -1;

    return 0;
}

void freeBlocskInIndirectionBlock(int blockN){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int blocksIndexesPerSector = SECTOR_SIZE / sizeof(DWORD);

    unsigned char buffer[SECTOR_SIZE] = {0};

    int i, j, sectorN;
    DWORD dataBlockN;

    for(i = 0; i < mountedSB.blockSize; i++){
        sectorN = firstPartitionSector + blockN * mountedSB.blockSize + i;
        read_sector(sectorN, buffer);
        for(j = 0; j < blocksIndexesPerSector; j++){
            memcpy(&dataBlockN, buffer + j * sizeof(DWORD), sizeof(DWORD));
            if(dataBlockN != 0){
                setBitmap2(BITMAP_DADOS, dataBlockN, 0); //OBS: Bitmap deve estar aberto

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mLiberado bloco %d\n\033[0m", dataBlockN);
            }
            //Else: Poderia retornar
        }
    }
}

void freeBlocksInDoubleIndirectionBlock(int blockN){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int blocksIndexesPerSector = SECTOR_SIZE / sizeof(DWORD);

    unsigned char buffer[SECTOR_SIZE] = {0};

    int i, j, sectorN;
    DWORD dataBlockN;

    for(i = 0; i < mountedSB.blockSize; i++){
        sectorN = firstPartitionSector + blockN * mountedSB.blockSize + i;
        read_sector(sectorN, buffer);
        for(j = 0; j < blocksIndexesPerSector; j++){
            memcpy(&dataBlockN, buffer + j * sizeof(DWORD), sizeof(DWORD));
            if(dataBlockN != 0){
                freeBlocskInIndirectionBlock(dataBlockN);
                setBitmap2(BITMAP_DADOS, dataBlockN, 0); //OBS: Bitmap deve estar aberto

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mLiberado bloco %d\n\033[0m", dataBlockN);
            }
            //Else: Poderia retornar
        }
    }
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo arquivo no disco e abrí-lo,
		sendo, nesse último aspecto, equivalente a função open2.
		No entanto, diferentemente da open2, se filename referenciar um
		arquivo já existente, o mesmo terá seu conteúdo removido e
		assumirá um tamanho de zero bytes.

Erros: -1 Particao nao montada
       -2 Arquivo existente aberto
       -3 Maximo de arquivos abertos
       -4 Sem iNodes livres para o novo arquivo
       -5 Diretorio (raiz) nao aberto
       -6 Sem espaco para novas entradas no diretorio raiz
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename) {
    if(mountedPartition == -1){

        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao criar arquivo %s : Nenhuma particao montada\n\033[0m", filename);

        return -1;
    }

    if(dirOpen == 0){

        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao criar arquivo %s : Diretorio nao aberto\n\033[0m", filename);

        return -5;
    }

    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));

    int j;
    int posArqAbertos = 0;

    while(posArqAbertos < MAX_ARQUIVOS_ABERTOS && arquivosAbertos[posArqAbertos].iNodeNumber != -1)
        posArqAbertos++;
    if(posArqAbertos == MAX_ARQUIVOS_ABERTOS){
        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao criar arquivo %s : Maximo de arquivos abertos\n\033[0m", filename);

        return -3;
    }

    struct t2fs_inode iNodeArquivo;

    CACHED_DIRENTS* aux = cached_dirents;
    while(aux != NULL){
        if(!strcmp(filename, aux->dirent.name)){
           for(j = 0; j < MAX_ARQUIVOS_ABERTOS; j++){
                if(arquivosAbertos[j].iNodeNumber == aux->iNodeNumber){
                    if(DEBUG_MODE)
                        printf("\033[0;31mFalha ao criar arquivo %s : Arquivo existente aberto\n\033[0m", filename);

                    return -2;
                }
            }

            readINodeM(aux->iNodeNumber, &iNodeArquivo);
            openBitmap2(firstPartitionSector);

            if(iNodeArquivo.dataPtr[0] != 0){
                setBitmap2(BITMAP_DADOS, iNodeArquivo.dataPtr[0], 0);

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mLiberado bloco %d\n\033[0m", iNodeArquivo.dataPtr[0]);
            }
            if(iNodeArquivo.dataPtr[1] != 0){
                setBitmap2(BITMAP_DADOS, iNodeArquivo.dataPtr[1], 0);

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mLiberado bloco %d\n\033[0m", iNodeArquivo.dataPtr[1]);
            }

            iNodeArquivo.blocksFileSize = 0;
            iNodeArquivo.bytesFileSize = 0;

            if(iNodeArquivo.singleIndPtr != 0){
                freeBlocskInIndirectionBlock(iNodeArquivo.singleIndPtr);
                setBitmap2(BITMAP_DADOS, iNodeArquivo.singleIndPtr, 0);

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mLiberado bloco %d\n\033[0m", iNodeArquivo.singleIndPtr);
            }
            if(iNodeArquivo.doubleIndPtr != 0){
                freeBlocksInDoubleIndirectionBlock(iNodeArquivo.doubleIndPtr);
                setBitmap2(BITMAP_DADOS, iNodeArquivo.doubleIndPtr, 0);

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mLiberado bloco %d\n\033[0m", iNodeArquivo.doubleIndPtr);
            }

            closeBitmap2();

            iNodeArquivo.doubleIndPtr = 0;
            iNodeArquivo.singleIndPtr = 0;
            iNodeArquivo.dataPtr[0] = 0;
            iNodeArquivo.dataPtr[1] = 0;
            writeINodeM(aux->iNodeNumber, iNodeArquivo);

            arquivosAbertos[posArqAbertos].iNodeNumber = aux->iNodeNumber;
            strcpy(arquivosAbertos[posArqAbertos].fileName, filename);
            arquivosAbertos[posArqAbertos].filePointer = 0;
            arquivosAbertos[posArqAbertos].arqBufferPointer = 0;
            arquivosAbertos[posArqAbertos].needsToWriteOnClose = 0;
            arquivosAbertos[posArqAbertos].fileSize = 0;
            arquivosAbertos[posArqAbertos].arqBuffer = malloc(SECTOR_SIZE * mountedSB.blockSize);

            if(DEBUG_MODE)
                printf("\033[0;32mCriado arquivo %s (Conteudo removido)\n\033[0m", filename);

            return posArqAbertos;
        }
        aux = aux->next;
    }

    //Nao achou o arquivo, cria um novo
    openBitmap2(firstPartitionSector);
    int niNodeNovoArquivo = searchBitmap2(BITMAP_INODE, 0);

    if(niNodeNovoArquivo == -1){
        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao criar arquivo %s : Sem inodes livres\n\033[0m", filename);

        return -4;
    }

    iNodeArquivo.blocksFileSize = 0;
    iNodeArquivo.bytesFileSize = 0;
    iNodeArquivo.RefCounter = 1;
    iNodeArquivo.doubleIndPtr = 0;
    iNodeArquivo.singleIndPtr = 0;
    iNodeArquivo.dataPtr[1] = 0;
    iNodeArquivo.dataPtr[0] = 0;

    setBitmap2(BITMAP_INODE, niNodeNovoArquivo, 1);

    struct t2fs_record novoRecord;
    novoRecord.TypeVal = 0x01;
    strcpy(novoRecord.name, filename);
    novoRecord.inodeNumber = niNodeNovoArquivo;

    if(addRecord(novoRecord) != 0){
        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao criar arquivo %s : Sem entradas livres no diretorio raiz\n\033[0m", filename);

        setBitmap2(BITMAP_INODE, niNodeNovoArquivo, 0);
        closeBitmap2();
        return -6;
    }

    writeINodeM(niNodeNovoArquivo, iNodeArquivo);

    closeBitmap2();

    arquivosAbertos[posArqAbertos].iNodeNumber = novoRecord.inodeNumber;
    strcpy(arquivosAbertos[posArqAbertos].fileName, filename);
    arquivosAbertos[posArqAbertos].filePointer = 0;
    arquivosAbertos[posArqAbertos].needsToWriteOnClose = 0;
    arquivosAbertos[posArqAbertos].arqBufferPointer = 0;
    arquivosAbertos[posArqAbertos].arqBuffer = malloc(SECTOR_SIZE * mountedSB.blockSize);
    arquivosAbertos[posArqAbertos].fileSize = 0;

    if(DEBUG_MODE)
        printf("\033[0;32mCriado arquivo %s\n\033[0m", filename);

    return posArqAbertos;
}

void writeBlock(DWORD blockN, char* block){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));

    int i;
    for(i = 0; i < mountedSB.blockSize; i++){
        write_sector(firstPartitionSector + blockN * mountedSB.blockSize + i, (unsigned char *) block + i * SECTOR_SIZE);
    }
}

void readBlock(DWORD blockN, char* block){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));

    int i;
    for(i = 0; i < mountedSB.blockSize; i++){
        read_sector(firstPartitionSector + blockN * mountedSB.blockSize + i, (unsigned char *) block + i * SECTOR_SIZE);
    }
}

//Retorna 0 se conseguiu ler, ou -1 se o bloco apontado nao existe
int readFromIndirectionBlock(DWORD blockN, DWORD dataBlockIndex, char* block){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));

    int blockSector = 0;
    while(dataBlockIndex >= (SECTOR_SIZE / sizeof(DWORD))){
        dataBlockIndex = dataBlockIndex - (SECTOR_SIZE / sizeof(DWORD));
        blockSector++;
    }

    DWORD dataBlockN;

    unsigned char buffer[SECTOR_SIZE] = {0};
    read_sector(firstPartitionSector + blockN * mountedSB.blockSize + blockSector, buffer);
    memcpy(&dataBlockN, buffer + dataBlockIndex * sizeof(DWORD), sizeof(DWORD));

    if(dataBlockN == 0)
        return -1;

    readBlock(dataBlockN, block);
    return 0;
}

int writeOnIndirectionBlock(DWORD blockN, DWORD dataBlockIndex, char* block){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));

    int blockSector = 0;
    while(dataBlockIndex >= (SECTOR_SIZE / sizeof(DWORD))){
        dataBlockIndex = dataBlockIndex - (SECTOR_SIZE / sizeof(DWORD));
        blockSector++;
    }

    DWORD dataBlockN;

    unsigned char buffer[SECTOR_SIZE] = {0};
    read_sector(firstPartitionSector + blockN * mountedSB.blockSize + blockSector, buffer);
    memcpy(&dataBlockN, buffer + dataBlockIndex * sizeof(DWORD), sizeof(DWORD));

    if(dataBlockN == 0){
        openBitmap2(firstPartitionSector);
        dataBlockN = searchBitmap2(BITMAP_DADOS, 0);

        if((int) dataBlockN <= 0){
            closeBitmap2();
            return NO_ALLOC;
        }

        setBitmap2(BITMAP_DADOS, dataBlockN, 1);
        closeBitmap2();
        memcpy(buffer + dataBlockIndex * sizeof(DWORD), &dataBlockN, sizeof(DWORD));
        write_sector(firstPartitionSector + blockN * mountedSB.blockSize + blockSector, buffer);

        if(DEBUG_MODE > 2)
            printf("\033[0;35mAlocado bloco %d como bloco de dados para algum arquivo\n\033[0m", dataBlockN);
    }
    writeBlock(dataBlockN, block);

    return 0;
}

//Retorna 0 se conseguiu ler, ou -1 se nao
int readFromDoubleIndirectionBlock(DWORD blockN, DWORD dataBlockIndex, char* block){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int blocksPerIndirectionBlock = (SECTOR_SIZE * mountedSB.blockSize) / sizeof(DWORD);

    int blockSector = 0;
    DWORD indBlockIndex = dataBlockIndex / blocksPerIndirectionBlock;

    while(indBlockIndex >= (SECTOR_SIZE / sizeof(DWORD))){
        indBlockIndex = indBlockIndex - (SECTOR_SIZE / sizeof(DWORD));
        blockSector++;
    }

    DWORD indBlockN;

    unsigned char buffer[SECTOR_SIZE] = {0};
    read_sector(firstPartitionSector + blockN * mountedSB.blockSize + blockSector, buffer);
    memcpy(&indBlockN, buffer + indBlockIndex * sizeof(DWORD), sizeof(DWORD));

    if(indBlockN == 0)
        return -1;

    return readFromIndirectionBlock(indBlockN, dataBlockIndex % blocksPerIndirectionBlock, block);
}

int writeOnDoubleIndirectionBlock(DWORD blockN, DWORD dataBlockIndex, char* block){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int blocksPerIndirectionBlock = (SECTOR_SIZE * mountedSB.blockSize) / sizeof(DWORD);

    int blockSector = 0;
    DWORD indBlockIndex = dataBlockIndex / blocksPerIndirectionBlock;

    while(indBlockIndex >= (SECTOR_SIZE / sizeof(DWORD))){
        indBlockIndex = indBlockIndex - (SECTOR_SIZE / sizeof(DWORD));
        blockSector++;
    }

    DWORD indBlockN;

    unsigned char clean_buffer[SECTOR_SIZE] = {0};

    unsigned char buffer[SECTOR_SIZE] = {0};
    read_sector(firstPartitionSector + blockN * mountedSB.blockSize + blockSector, buffer);
    memcpy(&indBlockN, buffer + indBlockIndex * sizeof(DWORD), sizeof(DWORD));

    if(indBlockN == 0){
        openBitmap2(firstPartitionSector);
        indBlockN = searchBitmap2(BITMAP_DADOS, 0);

        if((int) indBlockN <= 0){
            closeBitmap2();
            return NO_ALLOC;
        }

        setBitmap2(BITMAP_DADOS, indBlockN, 1);
        closeBitmap2();

        int i;
        for(i = 0; i < mountedSB.blockSize; i++)
            write_sector(firstPartitionSector + indBlockN * mountedSB.blockSize + i, clean_buffer);

        memcpy(buffer + indBlockIndex * sizeof(DWORD), &indBlockN, sizeof(DWORD));
        write_sector(firstPartitionSector + blockN * mountedSB.blockSize + blockSector, buffer);

        if(DEBUG_MODE > 2)
            printf("\033[0;35mAlocado bloco %d como bloco de indices para algum arquivo\n\033[0m", indBlockN);
    }
    if(writeOnIndirectionBlock(indBlockN, dataBlockIndex % blocksPerIndirectionBlock, block) == NO_ALLOC){
        openBitmap2(firstPartitionSector);
        setBitmap2(BITMAP_DADOS, indBlockN, 0);
        closeBitmap2();

        if(DEBUG_MODE > 2)
            printf("\033[0;35mLiberado bloco %d\n\033[0m", indBlockN);

        return NO_ALLOC;
    }

    return 0;
}

//Retorna 0 se conseguiu escrever, ou -1 se arquivo cheio, ou NO_ALLOC se nao conseguiu alocar blocos.
int writeBlockToFile(DWORD fileBlockN, char* block, struct t2fs_inode *iNodeFile){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int blocksPerIndirectionBlock = (SECTOR_SIZE * mountedSB.blockSize) / sizeof(DWORD);

    unsigned char buffer[SECTOR_SIZE] = {0};
    int i;

    if(fileBlockN == 0){
        if(iNodeFile->dataPtr[0] == 0){
            openBitmap2(firstPartitionSector);
            iNodeFile->dataPtr[0] = searchBitmap2(BITMAP_DADOS, 0);

            if((int) iNodeFile->dataPtr[0] <= 0){
                if(DEBUG_MODE)
                    printf("\033[0;31mFalha ao alocar blocos para um arquivo\n\033[0m");

                closeBitmap2();
                return NO_ALLOC;
            }

            setBitmap2(BITMAP_DADOS, iNodeFile->dataPtr[0], 1);
            closeBitmap2();

            if(DEBUG_MODE > 2)
                printf("\033[0;35mAlocado bloco %d como bloco de dados para algum arquivo\n\033[0m", iNodeFile->dataPtr[0]);
        }
        writeBlock(iNodeFile->dataPtr[0], block);
    }
    else{
        if(fileBlockN == 1){
            if(iNodeFile->dataPtr[1] == 0){
                openBitmap2(firstPartitionSector);
                iNodeFile->dataPtr[1] = searchBitmap2(BITMAP_DADOS, 0);

                if((int) iNodeFile->dataPtr[1] <= 0){
                    if(DEBUG_MODE)
                        printf("\033[0;31mFalha ao alocar blocos para um arquivo\n\033[0m");

                    closeBitmap2();
                    return NO_ALLOC;
                }

                setBitmap2(BITMAP_DADOS, iNodeFile->dataPtr[1], 1);
                closeBitmap2();

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mAlocado bloco %d como bloco de dados para algum arquivo\n\033[0m", iNodeFile->dataPtr[1]);
            }
            writeBlock(iNodeFile->dataPtr[1], block);
        }
        else{
            if(fileBlockN - 2 < blocksPerIndirectionBlock){
                if(iNodeFile->singleIndPtr == 0){
                    openBitmap2(firstPartitionSector);
                    iNodeFile->singleIndPtr = searchBitmap2(BITMAP_DADOS, 0);

                    if((int) iNodeFile->singleIndPtr <= 0){
                        if(DEBUG_MODE)
                            printf("\033[0;31mFalha ao alocar blocos para um arquivo\n\033[0m");

                        closeBitmap2();
                        return NO_ALLOC;
                    }

                    setBitmap2(BITMAP_DADOS, iNodeFile->singleIndPtr, 1);
                    closeBitmap2();

                    //Limpa o bloco de indices
                    for(i = 0; i < mountedSB.blockSize; i++)
                        write_sector(firstPartitionSector + iNodeFile->singleIndPtr * mountedSB.blockSize + i, buffer);

                    if(DEBUG_MODE > 2)
                        printf("\033[0;35mAlocado bloco %d como bloco de indices para algum arquivo\n\033[0m", iNodeFile->singleIndPtr);
                }
                if(writeOnIndirectionBlock(iNodeFile->singleIndPtr, fileBlockN - 2, block) == NO_ALLOC){
                    if(DEBUG_MODE)
                        printf("\033[0;31mFalha ao alocar blocos para um arquivo\n\033[0m");

                    return NO_ALLOC;
                }
            }
            else{
                if(fileBlockN - 2 - blocksPerIndirectionBlock < blocksPerIndirectionBlock * blocksPerIndirectionBlock){
                    if(iNodeFile->doubleIndPtr == 0){
                        openBitmap2(firstPartitionSector);
                        iNodeFile->doubleIndPtr = searchBitmap2(BITMAP_DADOS, 0);

                        if((int) iNodeFile->doubleIndPtr <= 0){
                            if(DEBUG_MODE)
                                printf("\033[0;31mFalha ao alocar blocos para um arquivo\n\033[0m");

                            closeBitmap2();
                            return NO_ALLOC;
                        }

                        setBitmap2(BITMAP_DADOS, iNodeFile->doubleIndPtr, 1);
                        closeBitmap2();

                        //Limpa o bloco de indices
                        for(i = 0; i < mountedSB.blockSize; i++)
                            write_sector(firstPartitionSector + iNodeFile->doubleIndPtr * mountedSB.blockSize + i, buffer);

                        if(DEBUG_MODE > 2)
                            printf("\033[0;35mAlocado bloco %d como bloco de indices duplo para algum arquivo\n\033[0m", iNodeFile->doubleIndPtr);
                    }
                    if(writeOnDoubleIndirectionBlock(iNodeFile->doubleIndPtr, fileBlockN - 2 - blocksPerIndirectionBlock, block) == NO_ALLOC){
                        if(DEBUG_MODE)
                        printf("\033[0;31mFalha ao alocar blocos para um arquivo\n\033[0m");

                        return NO_ALLOC;
                    }
                }
                else{
                    return -1;
                }
            }
        }
    }
    return 0;
}

//Le um Block de um arquivo
//Retorna 0 se leu ou -1 se o bloco nao esta escrito
int readBlockFromFile(DWORD fileBlockN, char* block, int iNodeN){
    const int blocksPerIndirectionBlock = (SECTOR_SIZE * mountedSB.blockSize) / sizeof(DWORD);

    struct t2fs_inode iNodeFile;
    readINodeM(iNodeN, &iNodeFile);

    if(fileBlockN == 0){
        if(iNodeFile.dataPtr[0] == 0)
            return -1;
        readBlock(iNodeFile.dataPtr[0], block);
    }
    else{
        if(fileBlockN == 1){
            if(iNodeFile.dataPtr[1] == 0)
                return -1;
            readBlock(iNodeFile.dataPtr[1], block);
        }
        else{
            if(fileBlockN - 2 < blocksPerIndirectionBlock){
                if(iNodeFile.singleIndPtr == 0)
                    return -1;
                return readFromIndirectionBlock(iNodeFile.singleIndPtr, fileBlockN - 2, block);
            }
            else{
                if(fileBlockN - 2 - blocksPerIndirectionBlock < blocksPerIndirectionBlock * blocksPerIndirectionBlock){
                    if(iNodeFile.doubleIndPtr == 0){
                        return -1;
                    }
                    return readFromDoubleIndirectionBlock(iNodeFile.doubleIndPtr, fileBlockN - 2 - blocksPerIndirectionBlock, block);
                }
                else{
                    return -1;
                }
            }
        }
    }

    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco.

Erros: -1 Nenhuma particao montada
       -2 Diretorio nao aberto
       -3 Arquivo nao existe
       -4 Arquivo aberto
-----------------------------------------------------------------------------*/
int delete2 (char *filename) {
    if(mountedPartition == -1){
        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao deletar o arquivo %s : Nenhuma particao montada\n\033[0m", filename);

        return -1;
    }

    if(dirOpen == 0){
        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao deletar o arquivo %s : Diretorio nao aberto\n\033[0m", filename);

        return -2;
    }

    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));

    int j;

    struct t2fs_inode iNodeArquivo;

	CACHED_DIRENTS* aux = cached_dirents;
    while(aux != NULL){
        if(!strcmp(filename, aux->dirent.name)){
           for(j = 0; j < MAX_ARQUIVOS_ABERTOS; j++){
                if(arquivosAbertos[j].iNodeNumber == aux->iNodeNumber){
                    if(DEBUG_MODE)
                        printf("\033[0;31mFalha ao deletar o arquivo %s : Arquivo aberto\n\033[0m", filename);

                    return -4;
                }
            }

            readINodeM(aux->iNodeNumber, &iNodeArquivo);

            removeRecord(aux->dirent.name);

            //Se for a ultima referencia, libera o arquivo e o inode
            if(iNodeArquivo.RefCounter == 1){
                openBitmap2(firstPartitionSector);

                if(iNodeArquivo.dataPtr[0] != 0){
                    setBitmap2(BITMAP_DADOS, iNodeArquivo.dataPtr[0], 0);

                    if(DEBUG_MODE > 2)
                        printf("\033[0;35mLiberado bloco %d\n\033[0m", iNodeArquivo.dataPtr[0]);
                }
                if(iNodeArquivo.dataPtr[1] != 0){
                    setBitmap2(BITMAP_DADOS, iNodeArquivo.dataPtr[1], 0);

                    if(DEBUG_MODE > 2)
                        printf("\033[0;35mLiberado bloco %d\n\033[0m", iNodeArquivo.dataPtr[1]);
                }
                if(iNodeArquivo.singleIndPtr != 0){
                    freeBlocskInIndirectionBlock(iNodeArquivo.singleIndPtr);
                    setBitmap2(BITMAP_DADOS, iNodeArquivo.singleIndPtr, 0);

                    if(DEBUG_MODE > 2)
                        printf("\033[0;35mLiberado bloco %d\n\033[0m", iNodeArquivo.singleIndPtr);
                }
                if(iNodeArquivo.doubleIndPtr != 0){
                    freeBlocksInDoubleIndirectionBlock(iNodeArquivo.doubleIndPtr);
                    setBitmap2(BITMAP_DADOS, iNodeArquivo.doubleIndPtr, 0);

                    if(DEBUG_MODE > 2)
                        printf("\033[0;35mLiberado bloco %d\n\033[0m", iNodeArquivo.doubleIndPtr);
                }

                setBitmap2(BITMAP_INODE, aux->iNodeNumber, 0);

                closeBitmap2();
            }
            else{
                iNodeArquivo.RefCounter--;
                writeINodeM(aux->iNodeNumber, iNodeArquivo);
            }

            //Remove entrada da cache
            if(current_dirent == aux)
                current_dirent = aux->next;

            if(cached_dirents == aux){
                cached_dirents = aux->next;
                free(aux);
            }
            else{
                CACHED_DIRENTS* aux2 = cached_dirents;
                while(aux2->next != aux)
                    aux2 = aux2->next;

                aux2->next = aux->next;
                free(aux);
            }

            if(DEBUG_MODE)
                printf("\033[0;32mDeletado o arquivo %s\n\033[0m", filename);

            return 0;
        }
        aux = aux->next;
    }

    if(DEBUG_MODE)
        printf("\033[0;31mFalha ao deletar o arquivo %s : Arquivo nao existe\n\033[0m", filename);

    return -3;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.

Erros: -1 Nenhuma particao montada
       -2 Diretorio nao aberto
       -3 Numero maximo de arquivos abertos
       -4 Arquivo aberto
       -5 Arquivo nao encontrado
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename) {
	if(mountedPartition == -1)
        return -1;

    if(dirOpen == 0)
        return -2;

    int j;
    int posArqAbertos = 0;

    while(posArqAbertos < MAX_ARQUIVOS_ABERTOS && arquivosAbertos[posArqAbertos].iNodeNumber != -1)
        posArqAbertos++;
    if(posArqAbertos == MAX_ARQUIVOS_ABERTOS)
        return -3;

    struct t2fs_inode iNodeArquivo;

    CACHED_DIRENTS* aux = cached_dirents;
    while(aux != NULL){
        if(!strcmp(filename, aux->dirent.name)){
           for(j = 0; j < MAX_ARQUIVOS_ABERTOS; j++){
                if(arquivosAbertos[j].iNodeNumber == aux->iNodeNumber)
                    return -4;
            }

            readINodeM(aux->iNodeNumber, &iNodeArquivo);

            arquivosAbertos[posArqAbertos].iNodeNumber = aux->iNodeNumber;
            strcpy(arquivosAbertos[posArqAbertos].fileName, filename);
            arquivosAbertos[posArqAbertos].filePointer = 0;
            arquivosAbertos[posArqAbertos].arqBufferPointer = 0;
            arquivosAbertos[posArqAbertos].needsToWriteOnClose = 0;
            arquivosAbertos[posArqAbertos].fileSize = iNodeArquivo.bytesFileSize;
            arquivosAbertos[posArqAbertos].arqBuffer = malloc(SECTOR_SIZE * mountedSB.blockSize);

            if(arquivosAbertos[posArqAbertos].fileSize > 0)
                readBlock(iNodeArquivo.dataPtr[0], arquivosAbertos[posArqAbertos].arqBuffer);

            return posArqAbertos;
        }
        aux = aux->next;
    }

    return -5;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um arquivo.

Erros: -1 Handle invalido
       -2 Sem espaco no disco para gravar o arquivo
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle) {
	if(handle < 0 || handle >= MAX_ARQUIVOS_ABERTOS || arquivosAbertos[handle].iNodeNumber == -1)
        return -1;

    char *arqB = arquivosAbertos[handle].arqBuffer;
    const int arqBsize = SECTOR_SIZE * mountedSB.blockSize;

    if(arquivosAbertos[handle].needsToWriteOnClose == 1){
        struct t2fs_inode iNodeFile;
        readINodeM(arquivosAbertos[handle].iNodeNumber, &iNodeFile);

        if(writeBlockToFile((arquivosAbertos[handle].filePointer / arqBsize), arqB, &iNodeFile) == NO_ALLOC)
            return -2;

        if(iNodeFile.bytesFileSize < arquivosAbertos[handle].filePointer)
            iNodeFile.bytesFileSize = arquivosAbertos[handle].filePointer;

        writeINodeM(arquivosAbertos[handle].iNodeNumber, iNodeFile);
    }

    arquivosAbertos[handle].iNodeNumber = -1;
    free(arquivosAbertos[handle].arqBuffer);

    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a leitura de uma certa quantidade
		de bytes (size) de um arquivo.

Erros: -1 Handle invalido
       -2 Fim do arquivo alcancado
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size) {
	if(handle < 0 || handle >= MAX_ARQUIVOS_ABERTOS || arquivosAbertos[handle].iNodeNumber == -1)
        return -1;

    char *arqB = arquivosAbertos[handle].arqBuffer;
    const int arqBsize = SECTOR_SIZE * mountedSB.blockSize;

    int bytesRead = 0;

    while(bytesRead < size){
        while(bytesRead < size && arquivosAbertos[handle].arqBufferPointer < arqBsize && arquivosAbertos[handle].filePointer < arquivosAbertos[handle].fileSize){
            *(buffer + bytesRead) = *(arqB + arquivosAbertos[handle].arqBufferPointer);
            arquivosAbertos[handle].arqBufferPointer++;
            arquivosAbertos[handle].filePointer++;
            bytesRead++;
        }
        if(bytesRead == size)
            return 0;
        if(arquivosAbertos[handle].filePointer == arquivosAbertos[handle].fileSize)
            return -2;
        if(arquivosAbertos[handle].arqBufferPointer == arqBsize){
            if(arquivosAbertos[handle].needsToWriteOnClose == 1){
              struct t2fs_inode iNodeFile;
              readINodeM(arquivosAbertos[handle].iNodeNumber, &iNodeFile);
              writeBlockToFile((arquivosAbertos[handle].filePointer / arqBsize) - 1, arqB, &iNodeFile);
              writeINodeM(arquivosAbertos[handle].iNodeNumber, iNodeFile);
            }

            readBlockFromFile((arquivosAbertos[handle].filePointer / arqBsize), arqB, arquivosAbertos[handle].iNodeNumber);
            arquivosAbertos[handle].arqBufferPointer = 0;
        }
    }

    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a escrita de uma certa quantidade
		de bytes (size) de  um arquivo.

Erros: -1 Handle invalido
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size) {
	if(handle < 0 || handle >= MAX_ARQUIVOS_ABERTOS || arquivosAbertos[handle].iNodeNumber == -1)
        return -1;

    char *arqB = arquivosAbertos[handle].arqBuffer;
    const int arqBsize = SECTOR_SIZE * mountedSB.blockSize;
    int bufferPointer = 0;

    arquivosAbertos[handle].needsToWriteOnClose = 1;

    while(bufferPointer < size){
        while(arquivosAbertos[handle].arqBufferPointer < arqBsize && bufferPointer < size){
            *(arqB + arquivosAbertos[handle].arqBufferPointer) = *(buffer + bufferPointer);
            bufferPointer++;
            arquivosAbertos[handle].arqBufferPointer++;
            arquivosAbertos[handle].filePointer++;
        }

        if(arquivosAbertos[handle].arqBufferPointer == arqBsize){
            //Encheu o buffer
            struct t2fs_inode iNodeFile;
            readINodeM(arquivosAbertos[handle].iNodeNumber, &iNodeFile);

            if(writeBlockToFile((arquivosAbertos[handle].filePointer / arqBsize) - 1, arqB, &iNodeFile) == NO_ALLOC)
                return -2;

            writeINodeM(arquivosAbertos[handle].iNodeNumber, iNodeFile);
            readBlockFromFile((arquivosAbertos[handle].filePointer / arqBsize), arqB, arquivosAbertos[handle].iNodeNumber);
            arquivosAbertos[handle].arqBufferPointer = 0;
        }
    }

	return 0;
}

void loadDirentsOnBlockToCache(int blockN){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int recordsPerSector = SECTOR_SIZE / sizeof(struct t2fs_record);

    unsigned char buffer[SECTOR_SIZE] = {0};

    struct t2fs_inode iNodeEntrada;
    int i, j;
    int sectorN;
    struct t2fs_record cRecord;
    DIRENT2 cDirent;

    for(i = 0; i < mountedSB.blockSize; i++){
        sectorN = firstPartitionSector + blockN * mountedSB.blockSize + i;
        read_sector(sectorN, buffer);
        for(j = 0; j < recordsPerSector; j++){
            memcpy(&cRecord, buffer + j * sizeof(struct t2fs_record), sizeof(struct t2fs_record));
            if(cRecord.TypeVal != 0x00){
                memcpy(cDirent.name, cRecord.name, 51);
                cDirent.fileType = 0x01;
                readINodeM(cRecord.inodeNumber, &iNodeEntrada);
                cDirent.fileSize = iNodeEntrada.bytesFileSize;

                CACHED_DIRENTS* aux = cached_dirents;
                if(aux == NULL){
                    cached_dirents = (CACHED_DIRENTS*) malloc(sizeof(CACHED_DIRENTS));
                    cached_dirents->next = NULL;
                    current_dirent = cached_dirents;
                    aux = cached_dirents;
                    aux->iNodeNumber = cRecord.inodeNumber;
                }
                else{
                    while(aux->next != NULL)
                        aux = aux->next;
                    aux->next = (CACHED_DIRENTS*) malloc(sizeof(CACHED_DIRENTS));
                    aux->next->next = NULL;
                    aux->next->iNodeNumber = cRecord.inodeNumber;
                    aux = aux->next;
                }

                aux->dirent = cDirent;
            }
        }
    }
}

void loadDirentsOnIndirectBlockToCache(int blockN){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int blocksPerIndirectionBlock = ((mountedSB.blockSize * SECTOR_SIZE) / sizeof(DWORD));

    int stopSingle = 0;

    unsigned char buffer[SECTOR_SIZE] = {0};

    int cSector = 0;
    int cBlock = 0;
    DWORD blockNumber;

    while(!stopSingle){
        int sectorN = firstPartitionSector + blockN * mountedSB.blockSize + cSector;
        read_sector(sectorN, buffer);
        do {
            memcpy(&blockNumber, buffer + cBlock * sizeof(DWORD), sizeof(DWORD));
            cBlock++;

            if(blockNumber != 0){
                loadDirentsOnBlockToCache(blockNumber);
            }

        } while(blockNumber != 0 && cBlock != blocksPerIndirectionBlock / mountedSB.blockSize);

        if(blockNumber == 0)
            stopSingle = 1;

        cSector++;
        if(cSector == mountedSB.blockSize)
            stopSingle = 1;

        cBlock = 0;
    }
}

void loadDirentsOnDoubleIndirectBlockToCache(int blockN){
    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));
    const int blocksPerIndirectionBlock = ((mountedSB.blockSize * SECTOR_SIZE) / sizeof(DWORD));

    int stopDouble = 0;

    unsigned char buffer[SECTOR_SIZE] = {0};

    int cSector = 0;
    int cBlock = 0;
    DWORD blockNumber;

    while(!stopDouble){
        int sectorN = firstPartitionSector + blockN * mountedSB.blockSize + cSector;
        read_sector(sectorN, buffer);
        do {
            memcpy(&blockNumber, buffer + cBlock * sizeof(DWORD), sizeof(DWORD));
            cBlock++;

            if(blockNumber != 0){
                loadDirentsOnIndirectBlockToCache(blockNumber);
            }

        } while(blockNumber != 0 && cBlock != blocksPerIndirectionBlock / mountedSB.blockSize);

        if(blockNumber == 0)
            stopDouble = 1;

        cSector++;
        if(cSector == mountedSB.blockSize)
            stopDouble = 1;

        cBlock = 0;
    }
}

/*-----------------------------------------------------------------------------
Função:	Função que abre o diretório raiz.

Erros: -1 Ponto de montagem vazio
       -2 Diretorio aberto
-----------------------------------------------------------------------------*/
int opendir2 () {
	if(mountedPartition == -1)
        return -1;
    if(dirOpen == 1)
        return -2;
    dirOpen = 1;

    struct t2fs_inode iNodeRaiz;
    readINodeM(0, &iNodeRaiz);

    if(iNodeRaiz.dataPtr[0] != 0)
        loadDirentsOnBlockToCache(iNodeRaiz.dataPtr[0]);
    if(iNodeRaiz.dataPtr[1] != 0)
        loadDirentsOnBlockToCache(iNodeRaiz.dataPtr[1]);
    if(iNodeRaiz.singleIndPtr != 0){
        loadDirentsOnIndirectBlockToCache(iNodeRaiz.singleIndPtr);
    }
    if(iNodeRaiz.doubleIndPtr != 0){
        loadDirentsOnDoubleIndirectBlockToCache(iNodeRaiz.doubleIndPtr);
    }

    current_dirent = cached_dirents;

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.

Erros: -1 = Nada montado
       -2 = Fim das entradas do diretorio
-----------------------------------------------------------------------------*/
int readdir2 (DIRENT2 *dentry) {
    if(mountedPartition == -1)
        return -1;

    if(current_dirent == NULL)
        return -2;

    *dentry = current_dirent->dirent;
    current_dirent = current_dirent->next;

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.

Erros: -1 Diretorio nao aberto
-----------------------------------------------------------------------------*/
int closedir2 () {
    if(!dirOpen)
        return -1;

    CACHED_DIRENTS* aux = cached_dirents;

    while(aux != NULL){
        aux = aux->next;
        free(cached_dirents);
        cached_dirents = aux;
    }
    cached_dirents = NULL;
    current_dirent = NULL;

    int i;
    for(i = 0; i < MAX_ARQUIVOS_ABERTOS; i++)
        arquivosAbertos[i].iNodeNumber = -1;

    dirOpen = 0;
    return 0;
}

/*-----------------------------------------------------------------------------
Função:	Cria um link simbólico (soft link)

Entra:	linkname -> nome do link
		filename -> nome do arquivo apontado pelo link

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int sln2(char *linkname, char *filename){
    // cria arquivo novo e escreve o nome do arquivo apontado

	if(mountedPartition == -1){

        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao criar arquivo %s : Nenhuma particao montada\n\033[0m", linkname);

        return -1;
    }

    if(dirOpen == 0){

        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao criar arquivo %s : Diretorio nao aberto\n\033[0m", linkname);

        return -5;
    }

    const int firstPartitionSector = *((int*) (mbr.partitionTable + mountedPartition*32));

    int j;
    int posArqAbertos = 0;

    while(posArqAbertos < MAX_ARQUIVOS_ABERTOS && arquivosAbertos[posArqAbertos].iNodeNumber != -1)
        posArqAbertos++;
    if(posArqAbertos == MAX_ARQUIVOS_ABERTOS){
        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao criar arquivo %s : Maximo de arquivos abertos\n\033[0m", linkname);

        return -3;
    }

    struct t2fs_inode iNodeArquivo;
	
	    CACHED_DIRENTS* aux = cached_dirents;
    while(aux != NULL){
        if(!strcmp(linkname, aux->dirent.name)){
           for(j = 0; j < MAX_ARQUIVOS_ABERTOS; j++){
                if(arquivosAbertos[j].iNodeNumber == aux->iNodeNumber){
                    if(DEBUG_MODE)
                        printf("\033[0;31mFalha ao criar arquivo %s : Arquivo existente aberto\n\033[0m", linkname);

                    return -2;
                }
            }

            readINodeM(aux->iNodeNumber, &iNodeArquivo);
            openBitmap2(firstPartitionSector);

            if(iNodeArquivo.dataPtr[0] != 0){
                setBitmap2(BITMAP_DADOS, iNodeArquivo.dataPtr[0], 0);

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mLiberado bloco %d\n\033[0m", iNodeArquivo.dataPtr[0]);
            }
            if(iNodeArquivo.dataPtr[1] != 0){
                setBitmap2(BITMAP_DADOS, iNodeArquivo.dataPtr[1], 0);

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mLiberado bloco %d\n\033[0m", iNodeArquivo.dataPtr[1]);
            }

            iNodeArquivo.blocksFileSize = 0;
            iNodeArquivo.bytesFileSize = 0;

            if(iNodeArquivo.singleIndPtr != 0){
                freeBlocskInIndirectionBlock(iNodeArquivo.singleIndPtr);
                setBitmap2(BITMAP_DADOS, iNodeArquivo.singleIndPtr, 0);

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mLiberado bloco %d\n\033[0m", iNodeArquivo.singleIndPtr);
            }
            if(iNodeArquivo.doubleIndPtr != 0){
                freeBlocksInDoubleIndirectionBlock(iNodeArquivo.doubleIndPtr);
                setBitmap2(BITMAP_DADOS, iNodeArquivo.doubleIndPtr, 0);

                if(DEBUG_MODE > 2)
                    printf("\033[0;35mLiberado bloco %d\n\033[0m", iNodeArquivo.doubleIndPtr);
            }

            closeBitmap2();

            iNodeArquivo.doubleIndPtr = 0;
            iNodeArquivo.singleIndPtr = 0;
            iNodeArquivo.dataPtr[0] = 0;
            iNodeArquivo.dataPtr[1] = 0;
            writeINodeM(aux->iNodeNumber, iNodeArquivo);

            arquivosAbertos[posArqAbertos].iNodeNumber = aux->iNodeNumber;
            strcpy(arquivosAbertos[posArqAbertos].fileName, linkname);
            arquivosAbertos[posArqAbertos].filePointer = 0;
            arquivosAbertos[posArqAbertos].arqBufferPointer = 0;
            arquivosAbertos[posArqAbertos].needsToWriteOnClose = 0;
            arquivosAbertos[posArqAbertos].fileSize = 0;
            arquivosAbertos[posArqAbertos].arqBuffer = malloc(SECTOR_SIZE * mountedSB.blockSize);

            if(DEBUG_MODE)
                printf("\033[0;32mCriado arquivo %s (Conteudo removido)\n\033[0m", linkname);


            return posArqAbertos;
        }
        aux = aux->next;
    }
	
	//Nao achou o arquivo, cria um novo
    openBitmap2(firstPartitionSector);
    int niNodeNovoArquivo = searchBitmap2(BITMAP_INODE, 0);

    if(niNodeNovoArquivo == -1){
        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao criar arquivo %s : Sem inodes livres\n\033[0m", linkname);

        return -4;
    }

    iNodeArquivo.blocksFileSize = 0;
    iNodeArquivo.bytesFileSize = 0;
    iNodeArquivo.RefCounter = 1;
    iNodeArquivo.doubleIndPtr = 0;
    iNodeArquivo.singleIndPtr = 0;
    iNodeArquivo.dataPtr[1] = 0;
    iNodeArquivo.dataPtr[0] = 0;

    setBitmap2(BITMAP_INODE, niNodeNovoArquivo, 1);

    struct t2fs_record novoRecord;
    novoRecord.TypeVal = 0x02;  // mudar para link
    strcpy(novoRecord.name, linkname);
    novoRecord.inodeNumber = niNodeNovoArquivo;

    if(addRecord(novoRecord) != 0){
        if(DEBUG_MODE)
            printf("\033[0;31mFalha ao criar arquivo %s : Sem entradas livres no diretorio raiz\n\033[0m", linkname);

        setBitmap2(BITMAP_INODE, niNodeNovoArquivo, 0);
        closeBitmap2();
        return -6;
    }

    writeINodeM(niNodeNovoArquivo, iNodeArquivo);

    closeBitmap2();

    arquivosAbertos[posArqAbertos].iNodeNumber = novoRecord.inodeNumber;
    strcpy(arquivosAbertos[posArqAbertos].fileName, linkname);
    arquivosAbertos[posArqAbertos].filePointer = 0;
    arquivosAbertos[posArqAbertos].needsToWriteOnClose = 0;
    arquivosAbertos[posArqAbertos].arqBufferPointer = 0;
    arquivosAbertos[posArqAbertos].arqBuffer = malloc(SECTOR_SIZE * mountedSB.blockSize);
    arquivosAbertos[posArqAbertos].fileSize = 0;

    if(DEBUG_MODE)
        printf("\033[0;32mCriado arquivo %s\n\033[0m", linkname);


    int r = write2(posArqAbertos, filename, strlen(filename)+1);
    printf("%d", r);
    return r;
}

/*-----------------------------------------------------------------------------
Função:	Cria um link estrito (hard link)

Entra:	linkname -> nome do link
		filename -> nome do arquivo apontado pelo link

Saída:	Se a operação foi realizada com sucesso, a função retorna "0" (zero).
	Em caso de erro, será retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int hln2(char *linkname, char *filename){
	// diz que inode é o mesmo do arquivo e aumenta o ref count do inode
	// n esquecer de add record no dir
    return -1;
}

