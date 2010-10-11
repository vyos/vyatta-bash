/* logging.c -- Shell command logging functionality */

/* This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2, or (at your option) any later
   version.

   Bash is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License along
   with Bash; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place, Suite 330, Boston, MA 02111 USA.

   This code was originally developed by Vyatta, Inc.
   Portions created by Vyatta are Copyright (C) 2010 Vyatta, Inc. */

#include "shell.h"
#include "filecntl.h"
#include "jobs.h"

#include <utmp.h>
#include <unistd.h>
#include <mqueue.h>

#include "logmessage.h"

/*
 * Find terminal port associated with stdin.
 * Since shell is interactive
 */
static const char *get_tty()
{
  const char *tty = ttyname(0);

  if (tty) {
    if (!strncmp(tty, "/dev/", 5))
      tty += 5;
  }

  return tty;
}

static mqd_t message_queue = -1;
static const char *current_tty;

void initialize_logging ()
{
  const char *tty;

  tty = get_tty();
  if (tty == NULL)
    return;	/* Not really an interactive shell */

  message_queue = mq_open("/vbash", O_WRONLY);
  if (message_queue == (mqd_t)-1)
    return;	/* No logging server running */
	
  SET_CLOSE_ON_EXEC(message_queue);

  current_tty = strdup(tty);
}

/*
 * Logs the result of the child for more detailed accounting
 */
void log_process_exit (child)
    PROCESS *child;
{
  size_t cc;
  struct command_log *lrec;

  if (message_queue == (mqd_t) -1)	/* message queue does not exist */
    return;

  if (child->command == NULL)		/* no command info */
    return;

  cc = sizeof(*lrec) + strlen(child->command) + 1;
  lrec = alloca(cc);
  memset(lrec, 0, cc);

  lrec->pid 	= child->pid;
  lrec->status 	= WEXITSTATUS(child->status);
  lrec->endtime	= time(0);
  lrec->uid	= current_user.uid;
  lrec->euid	= current_user.euid;
  lrec->gid	= current_user.gid;
  lrec->egid	= current_user.egid;
  strncpy(lrec->name, current_user.user_name, UT_NAMESIZE);
  strncpy(lrec->tty, current_tty, UT_LINESIZE);
  strcpy(lrec->command, child->command);

  /* Ignore errors?? */
  mq_send(message_queue, (char *)lrec, cc, 0);
}

