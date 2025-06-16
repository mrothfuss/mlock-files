CFLAGS=-O3 -Wunused-variable

MLOCK_FILES_SRC = main.c crc32.c
MLOCK_FILES_OBJ = ${MLOCK_FILES_SRC:.c=.o}

all: mlock-files

options:
	@echo build options:
	@echo "CFLAGS	= ${CFLAGS}"
	@echo "CC	= ${CC}"

mlock-files: ${MLOCK_FILES_OBJ}
	@echo ${CC} -o $@ ${MLOCK_FILES_OBJ} ${LDFLAGS}
	@${CC} -o $@ ${MLOCK_FILES_OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f mlock-files ${MLOCK_FILES_OBJ}

.PHONY: all options clean
