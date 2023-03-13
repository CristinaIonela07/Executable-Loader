/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "exec_parser.h"
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

static so_exec_t *exec;
static int fd;

static void segv_handler(int signum, siginfo_t *info, void *context)
{
	struct sigaction sa;
	char *fault_address = info->si_addr;
	int ok = 0;

	// Se verifica daca adresa fault-ului este in afara tuturor segmentelor
	for (int i = 0; i < exec->segments_no; i++) {
		char *virtual_address = (char *)(exec->segments[i].vaddr);
		int size_in_memory = exec->segments[i].mem_size;

		if (virtual_address <= fault_address &&
			fault_address < virtual_address + size_in_memory) {
			ok = 1;
			break;
		}
	}

	/* 
	 * Daca nu s-a gasit in niciun segment fault-ul,
	 * se apeleaza handler-ul default
	 */
	if (ok == 0) {
		sa.sa_sigaction(signum, info, context);
		return;
	}

	// Se parcurge vectorul de segmente
	for (int i = 0; i < exec->segments_no; i++) {
		char *virtual_address = (char *)(exec->segments[i].vaddr);
		int size_in_memory = exec->segments[i].mem_size;

		//Se aloca memorie pentru campul "data"
		if (exec->segments[i].data == NULL)
			exec->segments[i].data = (void *)calloc(size_in_memory, sizeof(char));

		// Adresa de start a paginii
		int page_nr = (fault_address - virtual_address) / getpagesize();

		/*
		 * Daca adresa fault-ului se gaseste in segmentul "i" din executabil si
		 * pagina nu a fost mapata, urmeaza sa se execute operatiile corespunzatoare
		 */
		if (virtual_address <= fault_address &&
		fault_address < virtual_address + size_in_memory &&
		((char *)(exec->segments[i].data))[page_nr] == 0) {
			// Se mapeaza pagina in memorie
			char *map = mmap((fault_address - (int)fault_address % getpagesize()),
							 getpagesize(), PROT_WRITE, MAP_ANONYMOUS |
							 MAP_FIXED | MAP_PRIVATE, -1, 0);

			// Daca maparea a esuat, se iese din program cu un cod de eroare
			if (map == MAP_FAILED)
				exit(EXIT_FAILURE);

			// Se noteaza faptul ca pagina a fost mapata cu succes
			((char *)(exec->segments[i].data))[page_nr] = 1;

			// Dimensiunea datelor ce urmeaza sa fie citite din fisier
			int size_read = exec->segments[i].file_size - page_nr * getpagesize();

			if (size_read > 0) {
				// Se cauta pozitia de la care se va citi din fisier
				off_t position = lseek(fd, exec->segments[i].offset +
				page_nr * getpagesize(), SEEK_SET);

				if (position < 0) {
					sa.sa_sigaction(signum, info, context);
					return;
				}

				/*
				 * Se copiaza din fisier in memorie tinand cont de
				 * dimensiunea unei pagini
				 */
				if (size_read < getpagesize())
					read(fd, map, size_read);
				else
					read(fd, map, getpagesize());
			}

			// Se seteaza permisiunile in pagina mapata
			mprotect(map, getpagesize(), exec->segments[i].perm);
		}
		/*
		 * Daca page fault-ul este generat intr-o pagina deja mapata
		 * inseamna ca segmentul respectiv nu are permisiunile necesare
		 * si se executa handler-ul default
		 */
		else {
			if (((char *)(exec->segments[i].data))[page_nr] == 1)
				sa.sa_sigaction(signum, info, context);
		}
	}
}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, NULL);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
	// Se deschide fisierul executabil
	fd = open(path, O_RDONLY | O_CREAT);

	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	so_start_exec(exec, argv);

	close(fd);
	return -1;
}
