/* Copyright (c) 1999 Anthony Towns
 * Copyright (c) 2000 Matthias Klose
 *
 * You may freely use, distribute, and modify this program.
 */

// Don't rely on /bin/sh and popen!

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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

int myexec(char *cmd, ...)
{
    va_list ap;
    pid_t child;

    char *new_argv[10];
    int new_argc = 0, i;

    new_argv[new_argc++] = cmd;
    va_start(ap, cmd);
    while ((new_argv[new_argc++] = va_arg(ap, char *)));
    va_end(ap);

    switch(child = fork()) {
      case -1:
        /* fork failed */
        return EXIT_FAILURE;

      case 0:
        /* i'm the child */
        {
            execv(cmd, new_argv);
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

int check_predepends(void)
{
    return myexec("/usr/bin/dpkg", "--assert-support-predepends", NULL);
}

int dpkg_compare_versions(char *v1, char *op, char *v2)
{
    return myexec("/usr/bin/dpkg", "--compare-versions", v1, op, v2, NULL);
}

int dpkg_divert_add(char *pkg, char *file)
{
    return myexec("/usr/sbin/dpkg-divert", "--package", pkg, "--add", file, NULL);
}

char *check_diversion(char *diversion_name)
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
                   "--list", diversion_name, NULL );
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

#define FIRST_WITHOUT_SHLINK "2.05b-4"

const char *msg =
  "As bash for Debian is destined to provide a working /bin/sh (pointing to\n"
  "/bin/bash) your link will be overwritten by a default link.\n\n"
  "If you don't want further upgrades to overwrite your customization,\n"
  "please read /usr/share/doc/bash/README.Debian for a more permanent solution.\n\n"
  "[Press RETURN to continue]";

int main(int argc, char *argv[]) {
    int targetlen;
    char target[PATH_MAX+1], answer[1024];

    if (argc < 2) {
	printf("\nbash/preinst: expected at least one argument\n\n");
	return EXIT_FAILURE;
    }

    if (check_predepends() != EXIT_SUCCESS) {
	printf("\nPlease upgrade to a new version of dpkg\n\n");
	return EXIT_FAILURE;
    }

/*
    if [ "$1" = upgrade ] && dpkg --compare-versions "$2" lt FIRST_WITHOUT_SHLINK; then
        div=$(dpkg-divert --list /bin/sh)
        if [ -z "$div" ]; then
                dpkg-divert --package bash --add /bin/sh
                ln -sf dash /bin/sh
        fi
        div=$(dpkg-divert --list /usr/share/man/man1/sh.1.gz)
        if [ -z "$div" ]; then
                dpkg-divert --package bash --add /usr/share/man/man1/sh.1.gz
                ln -sf dash.1.gz /usr/share/man/man1/sh.1.gz
        fi
     fi
*/

    if (strcmp(argv[1], "upgrade") == 0) {
	if (argc < 3) {
	    printf("\nbash/preinst upgrade: expected at least two arguments\n\n");
	    return EXIT_FAILURE;
	}
	if (dpkg_compare_versions(argv[2], "lt", FIRST_WITHOUT_SHLINK) == EXIT_SUCCESS) {
	    char *diversion = check_diversion("/bin/sh");

	    if (diversion == NULL)
		return EXIT_FAILURE;
	    // printf("diversion: /%s/\n", diversion);
	    if (strcmp(diversion, "") == 0) {
		// link is not diverted
		dpkg_divert_add("bash", "/bin/sh");

		unlink("/bin/sh");
		symlink("dash", "/bin/sh");
	    }

	    diversion = check_diversion("/usr/share/man/man1/sh.1.gz");

	    if (diversion == NULL)
		return EXIT_FAILURE;
	    // printf("diversion: /%s/\n", diversion);
	    if (strcmp(diversion, "") == 0) {
		// link is not diverted
		dpkg_divert_add("bash", "/usr/share/man/man1/sh.1.gz");

		unlink("/usr/share/man/man1/sh.1.gz");
		symlink("dash.1.gz", "/usr/share/man/man1/sh.1.gz");
	    }

	}
    }

#if 0
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
	char *diversion = check_diversion("/bin/sh");

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
#endif

    return EXIT_SUCCESS;
}
