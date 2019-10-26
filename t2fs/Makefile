#
# Makefile ESQUELETO
#
# DEVE ter uma regra "all" para geração da biblioteca
# regra "clean" para remover todos os objetos gerados.
#
# NECESSARIO adaptar este esqueleto de makefile para suas necessidades.
#
#

CC=gcc
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src

all: t2fs.o
	rm -rf lib
	mkdir lib
	ar crs $(LIB_DIR)/libt2fs.a $(BIN_DIR)/*.o

t2fs.o:
	$(CC) -o $(BIN_DIR)/t2fs.o -c $(SRC_DIR)/t2fs.c -Wall

testeA: all
	$(CC) -o testeA testeA.c -L$(LIB_DIR) -lt2fs -Wall

t2shell: all
	$(CC) -o t2shell exemplo/t2shell.c -L$(LIB_DIR) -lt2fs -Wall

clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/t2fs.o
	rm -f testeA
	rm -f t2shell


