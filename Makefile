CFLAGS=-O3 -Wunused-variable

MLOCKD_SRC = mlockd.c crc32.c
MLOCKD_OBJ = ${MLOCKD_SRC:.c=.o}

all: mlockd

options:
	@echo build options:
	@echo "CFLAGS	= ${CFLAGS}"
	@echo "CC	= ${CC}"

mlockd: ${MLOCKD_OBJ}
	@echo ${CC} -o $@ ${MLOCKD_OBJ} ${LDFLAGS}
	@${CC} -o $@ ${MLOCKD_OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f mlockd ${MLOCKD_OBJ}

.PHONY: all options clean
