#include <stdio.h>
#include <string.h>
#include "include/t2fs.h"

int main(){
    char buffer[20000] = {0};
    char buffer2[20000] = {0};
    int i;

    strcpy(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa (bloco de indices duplo)\n");
    strcat(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa (bloco de indices duplo)\n");
    strcat(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa (bloco de indices duplo)\n");
    strcat(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa (bloco de indices duplo)\n");
    for(i = 0; i < 20; i++)
        strcat(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa (bloco de indices duplo)\n");
    for(i = 0; i < 40; i++)
        strcat(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa... Ta dificil! (bloco de indices duplo)\n");
    for(i = 0; i < 60; i++)
        strcat(buffer, "Batatinha quando nasce. Texto muito grande para tentar encher o coisa... Ta dificil mesmo! (bloco de indices duplo)\n");
    for(i = 0; i < 40; i++)
        strcat(buffer, "Agora vai, o bloco duplo vai ser usado, por favor funcione, vai ser muito tri se funcionar (bloco de indices duplo)\n");
    strcat(buffer, "Deu :)\n");

    int szb = strlen(buffer) + 1;

    format2(0, 1);
    mount(0);

    opendir2();

    create2("Texto.txt");
    create2("Foto.jpg");
    create2("TrabalhoSisop.c");
    create2("Senhas.txt");
    create2("Minecraft.exe");
    create2("Cogumelo.exe");
    create2("Sisop2.exe");
    create2("BYOB.mp3");
    create2("Novo Documento.txt");
    create2("Receita de Miojo.txt");
    create2("Notas da P2.pdf");

    DIRENT2 de;

    write2(3, buffer, szb);
    close2(0);
    close2(1);
    close2(2);
    close2(3);
    close2(4);
    close2(5);
    close2(6);
    close2(7);
    close2(8);
    close2(9);

    closedir2();

    strcpy(buffer, "Espalha rama pelo chao.");

    opendir2();

    //delete2("Senhas.txt");

    while(!readdir2(&de)){
         printf("Nome: %s\nTamanho: %d Bytes\n\n", de.name, de.fileSize);
    }

    FILE2 hand = open2("Senhas.txt");

    read2(hand, buffer2, szb);
    close2(hand);
    printf("Buffer2: %s\n", buffer2);

    closedir2();

    umount();
    return 0;
}
