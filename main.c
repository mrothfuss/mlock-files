#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>

#define PROGRAM_NAME "mlock-files"

// global status
static volatile int running = 1;

struct lock_data {
	void *addr;
	size_t len;
	unsigned int locked: 1;
	unsigned int mapped: 1;
};

void signal_handle(int sig) {
	fprintf(stderr, "%s: Received signal %d\n", PROGRAM_NAME, sig);
	running = 0;
}

void usage(char *prog) {
	fprintf(stderr, "usage: %s [options] [files]\n\n", prog);
	fprintf(stderr, "\t-f <files.txt>       read files from a list\n");
	fprintf(stderr, "\t-D <pid.file>        fork after loading all files\n");
	fprintf(stderr, "\t-l                   lazy mode, do not stop if a file fails to load\n");
	fprintf(stderr, "\n");
}

int load_file(char *path, struct lock_data **data, int *data_allocated, int *data_used) {
	struct lock_data *d;
	int fd;
	struct stat st;
	uint64_t sum = 0;
	int i;


	if(*data_used == *data_allocated) {
		if(*data_allocated == 0)
			*data_allocated = 1024;
		else
			*data_allocated = (*data_allocated) * 2;
		*data = realloc(*data, sizeof(struct lock_data) * (*data_allocated));
		if(!(*data)) {
			fprintf(stderr, "%s: realloc failed (%d bytes)\n", PROGRAM_NAME, *data_allocated);
			*data_allocated = 0;
			*data_used = 0;
			return -1;
		}
	}
	d = &((*data)[(*data_used)++]);
	d->addr = NULL;
	d->len = 0;
	d->locked = 0;
	d->mapped = 0;

	// open
	fd = open(path, O_RDONLY);
	if(fd < 0) {
		fprintf(stderr, "%s: could not open %s\n", PROGRAM_NAME, path);
		return -1;
	}

	// stat
	if(fstat(fd, &st)) {
		fprintf(stderr, "%s: could not fstat %s\n", PROGRAM_NAME, path);
		close(fd);
		return -1;
	}

	// map
	d->addr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if(d->addr == MAP_FAILED) {
		fprintf(stderr, "%s: could not mmap %s\n", PROGRAM_NAME, path);
		return -1;
	}
	d->len = st.st_size;
	d->mapped = 1;

	// lock
	if(mlock(d->addr, d->len)) {
		fprintf(stderr, "%s: could not mlock %s\n", PROGRAM_NAME, path);
		munmap(d->addr, d->len);
		d->addr = NULL;
		d->len = 0;
		d->mapped = 0;
		return -1;
	}
	d->locked = 1;

	// force a full read (compute/display something to prevent removal by compiler optimization)
	for(i = 0; i < d->len; i++) {
		sum += ((uint8_t*)d->addr)[i];
	}
	fprintf(stderr, "%s: %s loaded (sum: %08lX)\n", PROGRAM_NAME, path, sum);
	return 0;
}

int main(int argc, char **argv) {
	// fork
	int fd[2];
	pid_t p;
	int return_value = 0;
	// sleep
	sigset_t sig_mask;
	// args
	int opt;
	int lazy = 0;
	char *fl_path = NULL;
	FILE *fl_fp = NULL;
	char *fl_line = NULL;
	size_t fl_size = 0;
	ssize_t fl_read = 0;
	char *daemonize = NULL;
	// data
	struct lock_data *data = NULL;
	int data_allocated = 0;
	int data_used;
	// misc
	int i;

	// Args
	while((opt = getopt(argc, argv, "lf:D:")) != -1) {
		switch(opt) {
			case 'l':
				lazy = 1;
				break;
			case 'f':
				fl_path = optarg;
				break;
			case 'D':
				daemonize = optarg;
				break;
			default:
				usage(argv[0]);
				return -1;
		}
	}

	// Arg Parse / validation 
	if(fl_path == NULL) {
		data_allocated = argc - optind;
	} else {
		data_allocated = 1024;
	}
	if(data_allocated == 0) {
		usage(argv[0]);
		return -1;
	}

	// handle fork
	if(daemonize) {
		pipe(fd);
		p = fork();

		if(p < 0) {
			fprintf(stderr, "%s: could not fork\n", PROGRAM_NAME);
			return -1;
		}
		if(p > 0) { // parent
			close(fd[1]); // close writing end
			// wait for child to send load success/fail message
			if(read(fd[0], &return_value, sizeof(return_value)) == sizeof(return_value)) {
				return return_value;
			} else {
				return -1;
			}
		}
		// child
		close(fd[0]);
	}

	// allocate
	data_used = 0;
	data = malloc(sizeof(struct lock_data) * data_allocated);
	if(!data) {
		fprintf(stderr, "%s: initial malloc failed\n", PROGRAM_NAME);
		return_value = -1;
		goto EXIT;
	}

	// load -f files
	if(fl_path) {
		fl_fp = fopen(fl_path, "r");
		if(!fl_fp) {
			fprintf(stderr, "%s: could not open %s\n", PROGRAM_NAME, fl_path);
			return_value = -1;
			goto EXIT;
		}
		while((fl_read = getline(&fl_line, &fl_size, fl_fp)) != -1) {
			if(fl_line[fl_read-1] == '\n') fl_line[fl_read-1] = 0;
			if(load_file(fl_line, &data, &data_allocated, &data_used)) {
				return_value = -1;
				if(!lazy) goto EXIT;
			}
		}
		if(fl_line) {
			free(fl_line);
			fl_line = NULL;
		}
	}

	// load arg files
	for(; optind < argc; optind++) {
		if(load_file(argv[optind], &data, &data_allocated, &data_used)) {
			return_value = -1;
			if(!lazy) goto EXIT;
		}
	}

	// Message
	if(return_value == 0) {
		fprintf(stderr, "%s: All files loaded\n", PROGRAM_NAME);
	} else {
		fprintf(stderr, "%s: WARNING, not all files were loaded\n", PROGRAM_NAME);
	}
	if(daemonize) {
		write(fd[1], &return_value, sizeof(return_value));
		daemonize = NULL; // disabled write() in EXIT section
	}

	// Sleep
	signal(SIGINT, signal_handle);
	signal(SIGQUIT, signal_handle);
	signal(SIGTERM, signal_handle);
	sigemptyset(&sig_mask);
	while(running) sigsuspend(&sig_mask);

EXIT:
	// Cleanup
	fprintf(stderr, "%s: Exiting Cleanly\n", PROGRAM_NAME);
	if(daemonize) {
		write(fd[1], &return_value, sizeof(return_value));
	}
	for(i = 0; i < data_used; i++) {
		if(!data[i].addr) continue;
		if(!data[i].len) continue;
		if(data[i].locked) munlock(data[i].addr, data[i].len);
		if(data[i].mapped) munmap(data[i].addr, data[i].len);
	}
	if(data) {
		free(data);
	}
	if(fl_line) {
		free(fl_line);
	}
	fprintf(stderr, "%s: Done\n", PROGRAM_NAME);
	return return_value;
}
