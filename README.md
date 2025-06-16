# mlockd

A small daemon to map and lock files into memory on unix-like systems.

## daemon mode

When running in daemon mode (-D) a PID file is required. The parent process will only exit once the background process has finished loading all files (useful for scripts).

## building
```
make
```

## usage
```
./mlockd [options] [files]

        -f <files.txt>       read files from a list
        -D <pid.file>        fork after loading all files
        -l                   lazy mode, do not stop if a file fails to load
```

## example output
```
./mlockd *
mlockd[0]: LICENSE loaded (CRC32: 8A053B6E)
mlockd[1]: Makefile loaded (CRC32: 4829741D)
mlockd[2]: crc32.c loaded (CRC32: E03F9464)
mlockd[3]: crc32.h loaded (CRC32: EF4B68EB)
mlockd[4]: crc32.o loaded (CRC32: B9493845)
mlockd[5]: mlockd loaded (CRC32: 4FC6B126)
mlockd[6]: mlockd.c loaded (CRC32: 731EA0E6)
mlockd[7]: mlockd.o loaded (CRC32: 59E9FA1A)
mlockd: 8 files mapped
mlockd: 8 files locked
mlockd: All files loaded
^Cmlockd: Received signal 2 [SIGINT]
mlockd: Terminating
mlockd: 8/8 files unlocked (OK)
mlockd: 8/8 files unmapped (OK)
mlockd: Exit Success
```
