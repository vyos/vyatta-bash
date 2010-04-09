/* Copyright (c) 1999 Anthony Towns
 * Copyright (c) 2000, 2002 Matthias Klose
 * Copyright (c) 2008 Canonical Ltd
 *
 * You may freely use, distribute, and modify this program.
 */

// Don't rely on /bin/sh and popen!

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>

/* XXX: evil kludge, deal with arbitrary name lengths */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "md5.h"

int check_predepends(void)
{
    pid_t child;

    switch(child = fork()) {
      case -1:
        /* fork failed */
        return EXIT_FAILURE;

      case 0:
        /* i'm the child */
        {
            execl( "/usr/bin/dpkg", "/usr/bin/dpkg",
                   "--assert-support-predepends", NULL );
           _exit(127);
        }

      default:
        /* i'm the parent */
        {
            int status;
            pid_t pid;
            pid = wait(&status);
            if (pid == child) {
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    return EXIT_SUCCESS;
                }
            }
        }
    }
    return EXIT_FAILURE;
}

#define BUFSIZE 8192

int md5sum_match(char *fn, char* fn_digest)
{
  md5_state_t md5;
  md5_byte_t digest[16];
  unsigned char hexdigest[33];
  int i, j, fd;
  size_t nbytes;
  md5_byte_t buf[BUFSIZE];

  // if not existant, md5sums don't match
  if (access(fn, R_OK))
    return 0;
  if ((fd = open(fn, O_RDONLY)) == -1)
    return 0;
  
  md5_init(&md5);
  while (nbytes = read(fd, buf, BUFSIZE))
    md5_append(&md5, buf, nbytes);
  md5_finish(&md5, digest);
  close(fd);

  for (i = j = 0; i < 16; i++) {
    unsigned char c;
    c = (digest[i] >> 4) & 0xf;
    c = (c > 9) ? c + 'a' - 10 : c + '0';
    hexdigest[j++] = c;
    c = (digest[i] & 0xf);
    c = (c > 9) ? c + 'a' - 10 : c + '0';
    hexdigest[j++] = c;
  }
  hexdigest[j] = '\0';
#ifdef NDEBUG
  fprintf(stderr, "fn=%s, md5sum=%s, expected=%s\n", fn, hexdigest, fn_digest);
#endif
  return !strcmp(fn_digest, hexdigest);
}

int unmodified_file(char *fn, int md5sumc, unsigned char* md5sumv[])
{
  int i;

  // if not existant, pretend its unmodified
  if (access(fn, R_OK))
    return 1;
  for (i = 0; i < md5sumc; i++) {
    if (md5sum_match(fn, md5sumv[i]))
      return 1;
  }
  return 0;
}

unsigned char *md5sumv_bash_profile[] = {
  "d1a8c44e7dd1bed2f3e75d1343b6e4e1", // dapper, edgy, etch
  "0bc1802860b758732b862ef973cd79ff", // feisty, gutsy
};
const int md5sumc_bash_profile = sizeof(md5sumv_bash_profile) / sizeof (char *);

unsigned char *md5sumv_profile[] = {
  "7d97942254c076a2ea5ea72180266420", // feisty, gutsy
};
const int md5sumc_profile = sizeof(md5sumv_profile) / sizeof (char *);

#ifdef BC_CONFIG
unsigned char *md5sumv_completion[] = {
  "2bc0b6cf841eefd31d607e618f1ae29d", // dapper
  "ae1d298e51ea7f8253eea8b99333561f", // edgy
  "adc2e9fec28bd2d4a720e725294650f0", // feisty
  "c8bce25ea68fb0312579a421df99955c", // gutsy, and last one in bash
  "9da8d1c95748865d516764fb9af82af9", // etch, sid (last one in bash)
};
const int md5sumc_completion = sizeof(md5sumv_completion) / sizeof (char *);
#endif

char *check_diversion(void)
{
    pid_t child;
    int pipedes[2];

    if (pipe(pipedes))
	return NULL;


    switch(child = fork()) {
      case -1:
        /* fork failed */
	close(pipedes[0]);
	close(pipedes[1]);
        return NULL;

      case 0:
        /* i'm the child */
        {
	    if (dup2(pipedes[STDOUT_FILENO], STDOUT_FILENO) < 0)
		_exit(127);
	    close(pipedes[STDIN_FILENO]);
	    close(pipedes[STDOUT_FILENO]);
            execl( "/usr/sbin/dpkg-divert", "/usr/sbin/dpkg-divert",
                   "--list", "/bin/sh", NULL );
           _exit(127);
        }

      default:
        /* i'm the parent */
        {
	    static char line[1024];
	    FILE *fd;

	    close (pipedes[STDOUT_FILENO]);
	    fcntl (pipedes[STDIN_FILENO], F_SETFD, FD_CLOEXEC);
	    fd = fdopen (pipedes[STDIN_FILENO], "r");

	    while (fgets(line, 1024, fd) != NULL) {
		line[strlen(line)-1] = '\0';
		break;
	    }
	    fclose(fd);
	    return line;
        }
    }
    return NULL;
}

const char *msg =
  "As bash for Debian is destined to provide a working /bin/sh (pointing to\n"
  "/bin/bash) your link will be overwritten by a default link.\n\n"
  "If you don't want further upgrades to overwrite your customization, please\n"
  "read /usr/share/doc/bash/README.Debian.gz for a more permanent solution.\n\n"
  "[Press RETURN to continue]";

int main(void) {
    int targetlen;
    char target[PATH_MAX+1], answer[1024], *fn;

    fn = "/etc/skel/.bash_profile";
    if (!access(fn, R_OK)) {
      if (unmodified_file(fn, md5sumc_bash_profile, md5sumv_bash_profile)) {
	printf("removing %s in favour of /etc/skel/.profile\n", fn);
	unlink(fn);
      }
      else {
	fn = "/etc/skel/.profile";
	if (!access(fn, R_OK)) {
	  if (unmodified_file(fn, md5sumc_profile, md5sumv_profile)) {
	    printf("renaming /etc/skel/.bash_profile to %s\n", fn);
	    rename("/etc/skel/.bash_profile", fn);
	  }
	}
      }
    }

#ifdef BC_CONFIG
    fn = "/etc/bash_completion";
    if (!access(fn, R_OK)) {
      if (unmodified_file(fn, md5sumc_completion, md5sumv_completion)) {
	printf("removing unmodified %s, now in package bash-completion\n", fn);
	unlink(fn);
      }
    }
#endif

    if (check_predepends() != EXIT_SUCCESS) {
	printf("\nPlease upgrade to a new version of dpkg\n\n");
	return EXIT_FAILURE;
    }
    targetlen = readlink("/bin/sh", target, PATH_MAX);
    if (targetlen == -1) {
	// error reading link. Will be overwritten.
	puts("The bash upgrade discovered that something is wrong with your /bin/sh link.");
	puts(msg);
	fgets(answer, 1024, stdin);
	return EXIT_SUCCESS;
    }
    target[targetlen] = '\0';
    if (strcmp(target, "bash") != 0 && strcmp(target, "/bin/bash") != 0) {
	char *diversion = check_diversion();

	if (diversion == NULL)
	    return EXIT_FAILURE;
	// printf("diversion: /%s/\n", diversion);
	if (strcmp(diversion, "") != 0)
	    // link is diverted
	    return EXIT_SUCCESS;
	printf("The bash upgrade discovered that your /bin/sh link points to %s.\n", target);
	puts(msg);
	fgets(answer, 1024, stdin);
	return EXIT_SUCCESS;
    }

    return EXIT_SUCCESS;
}
