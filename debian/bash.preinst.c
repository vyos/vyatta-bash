/* Copyright (c) 1999 Anthony Towns
 * Copyright (c) 2000, 2002 Matthias Klose
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
  "If you don't want further upgrades to overwrite your customization,\n"
  "please read /usr/share/doc/bash/README.Debian for a more permanent solution.\n\n"
  "[Press RETURN to continue]";

int main(void) {
    int targetlen;
    char target[PATH_MAX+1], answer[1024];

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
