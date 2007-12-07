/* vyatta-restricted.c -- Vyatta restricted mode functionality */

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
   Portions created by Vyatta are Copyright (C) 2007 Vyatta, Inc. */

#include "shell.h"
#include "vyatta-restricted.h"

static int
is_in_command_list(const char *cmd, char *cmds[])
{
  int idx = 0;
  for (idx = 0; cmds[idx]; idx++) {
    if (strcmp(cmd, cmds[idx]) == 0) {
      return 1;
    }
  }
  return 0;
}

static int
is_vyatta_restricted_pipe_command(WORD_LIST *words)
{
  char *allowed_commands[] = { "more", NULL };
  if (words) {
    if (!words->next) {
      /* only 1 word */
      if (is_in_command_list(words->word->word, allowed_commands)) {
        /* allowed */
        return 1;
      }
    }
  }
  /* not allowed */
  return 0;
}

static void
make_restricted_word(WORD_DESC *word)
{
  char *c, *ns, *n;
  int sq_count = 0;
  char *uqs = string_quote_removal(word->word, 0);

  for (c = uqs; *c; c++) {
    if (*c == '\'') {
      sq_count++;
    }
  }

  /* strlen + start/end quotes + \0 + extra "'\''" */
  ns = (char *) xmalloc(strlen(uqs) + 2 + 1 + (3 * sq_count));
  n = ns;
  *n = '\'';
  n++;
  for (c = uqs; *c; c++) {
    if (*c == '\'') {
      *n = '\'';
      *(n + 1) = '\\';
      *(n + 2) = '\'';
      *(n + 3) = '\'';
      n += 4;
    } else {
      *n = *c;
      n++;
    }
  }
  *n = '\'';
  *(n + 1) = '\0';

  free(word->word);
  free(uqs);
  word->word = ns;
  word->flags = W_QUOTED;
}

static void
make_restricted_wordlist(WORD_LIST *words)
{
  WORD_LIST *l = words->next; /* skip the first word */
  for (; l; l = l->next) {
    make_restricted_word(l->word);
  }
}

static int
is_vyatta_restricted_command(COMMAND *cmd)
{
  struct simple_com *cS;
  struct connection *cC;

  if (!cmd) {
    return 1;
  }

  switch (cmd->type) {
  case cm_simple:
    cS = cmd->value.Simple;
    if (!(cS->redirects)) {
      /* simple command, no redirects */
      /* make sure the words are allowed */
      make_restricted_wordlist(cS->words);
      return 1;
    }
    break;
  case cm_connection:
    cC = cmd->value.Connection;
    if (cC->connector == '|') {
      if ((cC->first->type == cm_simple) && (cC->second->type == cm_simple)) {
        struct simple_com *cS1 = cC->first->value.Simple;
        struct simple_com *cS2 = cC->second->value.Simple;
        if (!(cS1->redirects) && !(cS2->redirects)) {
          /* both are simple and no redirects */
          /* make sure the words are allowed */
          make_restricted_wordlist(cS1->words);
          make_restricted_wordlist(cS2->words);
          if (is_vyatta_restricted_pipe_command(cS2->words)) {
            /* pipe command is allowed => allowed */
            return 1;
          }
        }
      }
    }
    break;
  default:
    break;
  }
  /* not allowed */
  return 0;
}

static int
is_vyatta_cfg_command(const char *cmd)
{
  char *valid_commands[] = { "set", "delete", "commit", "save", "load",
                             "show", "exit", "edit", "run", NULL };
  return is_in_command_list(cmd, valid_commands);
}

static int
is_vyatta_op_command(const char *cmd)
{
  char *dir = getenv("vyatta_op_templates");
  DIR *dp = NULL;
  struct dirent *dent = NULL;
  char *restrict_exclude_commands[]
    = { "clear", "configure", "init-floppy", "install-system", "no",
        "reboot", "set", "telnet", NULL };
  char *other_commands[] = { "exit", NULL };
  int ret = 0;

  if (dir == NULL || (dp = opendir(dir)) == NULL) {
    return 0;
  }

  /* FIXME this assumes FULL == "users" */
  if (in_vyatta_restricted_mode(FULL)
      && is_in_command_list(cmd, restrict_exclude_commands)) {
    /* command not allowed in "full" restricted mode */
    return 0;
  }

  while (dent = readdir(dp)) {
    if (strncmp(dent->d_name, ".", 1) == 0) {
      continue;
    }
    if (strcmp(dent->d_name, cmd) == 0) {
      ret = 1;
      break;
    }
  }
  closedir(dp);
  return (ret) ? 1 : is_in_command_list(cmd, other_commands);
}

static char *prev_cmdline = NULL;

int
is_vyatta_command(char *cmdline, COMMAND *cmd)
{
  char *cfg = getenv("_OFR_CONFIGURE");
  int in_cfg = (cfg) ? (strcmp(cfg, "ok") == 0) : 0;
  char *start = cmdline;
  char *end = NULL;
  char save = 0;
  int ret = 0;

  if (!prev_cmdline) {
    prev_cmdline = strdup("");
  }
  if (strcmp(cmdline, prev_cmdline) == 0) {
    /* still at the same line. not checking. */
    return 1;
  }
  if (!is_vyatta_restricted_command(cmd)) {
    return 0;
  }

  while (*start && (whitespace(*start) || *start == '\n')) {
    start++;
  }
  if (*start == 0) {
    /* empty command line is valid */
    free(prev_cmdline);
    prev_cmdline = strdup(cmdline);
    return 1;
  }
  end = start;
  while (*end && (!whitespace(*end) && *end != '\n')) {
    end++;
  }
  save = *end;
  *end = 0;

  if (in_cfg) {
    ret = is_vyatta_cfg_command(start);
  } else {
    ret = is_vyatta_op_command(start);
  }
  *end = save;

  if (ret) {
    /* valid command */
    free(prev_cmdline);
    prev_cmdline = strdup(cmdline);
  }
  return ret;
}

static int
vyatta_user_in_group(uid_t ruid, char *grp_name)
{
  int ret = 0;
  struct passwd pw;
  struct passwd *pwp = NULL;
  struct group grp;
  struct group *grpp = NULL;
  char *pbuf = NULL, *gbuf = NULL;
  long psize = 0, gsize = 0;

  if (!grp_name) {
    return 0;
  }

  do {
    psize = sysconf(_SC_GETPW_R_SIZE_MAX);
    pbuf = (char *) xmalloc(psize);
    if (!pbuf) {
      break;
    }

    gsize = sysconf(_SC_GETGR_R_SIZE_MAX);
    gbuf = (char *) xmalloc(gsize);
    if (!gbuf) {
      break;
    }

    ret = getpwuid_r(ruid, &pw, pbuf, psize, &pwp);
    if (!pwp) {
      break;
    }

    ret = getgrnam_r(grp_name, &grp, gbuf, gsize, &grpp);
    if (!grpp) {
      break;
    }

    {
      int i = 0;
      for (i = 0; grp.gr_mem[i]; i++) {
        if (strcmp(pw.pw_name, grp.gr_mem[i]) == 0) {
          ret = 1;
          break;
        }
      }
    }
  } while (0);

  if (pbuf) {
    free(pbuf);
  }
  if (gbuf) {
    free(gbuf);
  }
  return ret;
}

static int vyatta_default_output_restricted = 0;
static int vyatta_default_full_restricted = 0;

#define VYATTA_OUTPUT_RESTRICTED_GROUP "vyattacfg"

void
set_vyatta_restricted_mode()
{
  uid_t ruid = getuid();
  if (vyatta_user_in_group(ruid, VYATTA_OUTPUT_RESTRICTED_GROUP)) {
    vyatta_default_output_restricted = 1;
    vyatta_default_full_restricted = 0;
  } else {
    /* if not in the output restricted group, default to full */
    vyatta_default_output_restricted = 0;
    vyatta_default_full_restricted = 1;
  }
}

int
in_vyatta_restricted_mode(enum vyatta_restricted_type type)
{
  char *rval = getenv("VYATTA_RESTRICTED_MODE");
  int output = vyatta_default_output_restricted;
  int full = vyatta_default_full_restricted;

  /* environment var overrides default */
  if (rval) {
    output = (strcmp(rval, "output") == 0);
    full = (strcmp(rval, "full") == 0);
  }

  if (type == OUTPUT && (output || full)) {
    return 1;
  }
  if (type == FULL && full) {
    return 1;
  }

  return 0;
}

