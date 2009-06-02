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
#include "bashhist.h"
#include "vyatta-restricted.h"

#define FILENAME_MODE "restricted-mode"
#define FILENAME_OP "allowed-op"
#define FILENAME_CFG "allowed-cfg"
#define FILENAME_PIPE "allowed-pipe"

static char *prev_cmdline = NULL;

/* allowed commands lists */
static char **allowed_op_cmds = NULL;
static char **allowed_cfg_cmds = NULL;
static char **allowed_pipe_cmds = NULL;
static int *pipe_cmd_args = NULL;

static char *vyatta_user_level_dir = NULL;

/* default restricted mode */
static int vyatta_default_output_restricted = 0;
static int vyatta_default_full_restricted = 0;

static char *expand_disable_cmds[] = { "_vyatta_op_run",
                                       "/opt/vyatta/sbin/my_set",
                                       "/opt/vyatta/sbin/my_delete",
                                       "/opt/vyatta/sbin/my_commit",
                                       NULL };

static int
is_expansion_disabled()
{
  char *exp = getenv("VYATTA_ENABLE_SHELL_EXPANSION");
  if (!exp) {
    return 1;
  }
  return 0;
}

void
vyatta_reset_hist_expansion()
{
#if defined (BANG_HISTORY)
  if (is_expansion_disabled()) {
    history_expansion_inhibited = 1;
  } else {
    history_expansion_inhibited = 0;
  }
#endif
}

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
  WORD_LIST *w = words;
  int count = 1;
  while (w = w->next) {
    count++;
  }
  if (words) {
    int i = 0;
    if (!allowed_pipe_cmds) {
      /* no restriction */
      return 1;
    }
    for (i = 0; allowed_pipe_cmds[i]; i++) {
      if (strcmp(words->word->word, allowed_pipe_cmds[i]) == 0
          && count == pipe_cmd_args[i]) {
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

/* this basically disables shell expansions for "simple" commands. */
/* full: do a "full" check (disallow env override && also check pipe). */
void
vyatta_check_expansion(COMMAND *cmd, int full)
{
  struct simple_com *cS;
  struct connection *cC;

  if (!cmd) {
    return;
  }
  if (!full && !is_expansion_disabled()) {
    /* enabled */
    return;
  }
 
  switch (cmd->type) {
  case cm_simple:
    cS = cmd->value.Simple;
    if (cS && !(cS->redirects)) {
      /* simple command, no redirects */
      if (cS->words && cS->words->word && 
	  is_in_command_list(cS->words->word->word, expand_disable_cmds)) {
        /* user command => quote all words */
        make_restricted_wordlist(cS->words);
      }
    }
    break;
  case cm_connection:
    cC = cmd->value.Connection;
    if ((cC->connector == '|') && (cC->first->type == cm_simple)) {
      struct simple_com *cS1 = cC->first->value.Simple;
      if (!(cS1->redirects)) {
        /* simple, no redirects */
        if (is_in_command_list(cS1->words->word->word, expand_disable_cmds)) {
          /* user command => quote all words */
          make_restricted_wordlist(cS1->words);
        }
      }
      if (full && (cC->second->type == cm_simple)) {
        struct simple_com *cS2 = cC->second->value.Simple;
        if (!(cS2->redirects)) {
          /* simple, no redirects */
          /* quote all words (not checking user command after pipe) */
          make_restricted_wordlist(cS2->words);
        }
      }
    }
    break;
  default:
    break;
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
  if (!allowed_cfg_cmds) {
    /* no restriction */
    return 1;
  }
  return is_in_command_list(cmd, allowed_cfg_cmds);
}

static int
is_vyatta_op_command(const char *cmd)
{
  if (!allowed_op_cmds) {
    /* no restriction */
    return 1;
  }
  return is_in_command_list(cmd, allowed_op_cmds);
}

int
is_vyatta_command(char *cmdline, COMMAND *cmd)
{
  char *cfg = getenv("_OFR_CONFIGURE");
  int in_cfg = (cfg) ? (strcmp(cfg, "ok") == 0) : 0;
  char *start = cmdline;
  char *end = NULL;
  char save = 0;
  int ret = 0;

  /* check expansions (full) */
  vyatta_check_expansion(cmd, 1);

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

static FILE *
fopen_level_file(char *file_name)
{
  FILE *f = NULL;
#define BUF_SIZE 1024
  char *buf = (char *) xmalloc(BUF_SIZE);
  if (!buf) {
    return NULL;
  }

  do {
    int r = snprintf(buf, BUF_SIZE, "%s/%s", vyatta_user_level_dir, file_name);
    if (r >= BUF_SIZE) {
      break;
    }

    f = fopen(buf, "r");
  } while (0);

  free(buf);
  return f;
}

static char *
fgets_level_file(char *buf, int size, FILE *lfile)
{
  if (fgets(buf, size, lfile)) {
    int end = 0;
    while (buf[end] && isprint(buf[end]) && !isspace(buf[end])) {
      end++;
    }
    buf[end] = 0;
    return buf;
  } else {
    return NULL;
  }
}

static void
set_default_mode()
{
  FILE *lfile = NULL;
  char *line = NULL;
  char buf[256];

  /* default to full restricted */
  vyatta_default_output_restricted = 0;
  vyatta_default_full_restricted = 1;

  if (!(lfile = fopen_level_file(FILENAME_MODE))) {
    return;
  }
  while (line = fgets_level_file(buf, 256, lfile)) {
    if (strcmp(line, "output") == 0) {
      vyatta_default_output_restricted = 1;
      vyatta_default_full_restricted = 0;
      break;
    }
    if (strcmp(line, "full") == 0) {
      vyatta_default_output_restricted = 0;
      vyatta_default_full_restricted = 1;
      break;
    }
  }
  fclose(lfile);
  return;
}

static void
set_allowed_op_cmds()
{
  FILE *lfile = NULL;
  char *line = NULL;
  char buf[256];
  int count = 1;

  if (allowed_op_cmds) {
    return;
  }

  if (!(lfile = fopen_level_file(FILENAME_OP))) {
    return;
  }
  while (line = fgets_level_file(buf, 256, lfile)) {
    count++;
  }
  fclose(lfile);

  /* count is 1 more than number of lines */
  allowed_op_cmds = (char **) xmalloc(sizeof(char *) * count);
  memset(allowed_op_cmds, 0, sizeof(char *) * count);

  if (!(lfile = fopen_level_file(FILENAME_OP))) {
    return;
  }
  count = 0;
  while (line = fgets_level_file(buf, 256, lfile)) {
    allowed_op_cmds[count] = strdup(line);
    count++;
  }
  fclose(lfile);
  return;
}

static void
set_allowed_cfg_cmds()
{
  FILE *lfile = NULL;
  char *line = NULL;
  char buf[256];
  int count = 1;

  if (allowed_cfg_cmds) {
    return;
  }

  if (!(lfile = fopen_level_file(FILENAME_CFG))) {
    return;
  }
  while (line = fgets_level_file(buf, 256, lfile)) {
    count++;
  }
  fclose(lfile);

  /* count is 1 more than number of lines */
  allowed_cfg_cmds = (char **) xmalloc(sizeof(char *) * count);
  memset(allowed_cfg_cmds, 0, sizeof(char *) * count);

  if (!(lfile = fopen_level_file(FILENAME_CFG))) {
    return;
  }
  count = 0;
  while (line = fgets_level_file(buf, 256, lfile)) {
    allowed_cfg_cmds[count] = strdup(line);
    count++;
  }
  fclose(lfile);
  return;
}

static void
set_allowed_pipe_cmds()
{
  FILE *lfile = NULL;
  char *line = NULL;
  char buf[256];
  int count = 0;

  if (allowed_pipe_cmds) {
    return;
  }

  if (!(lfile = fopen_level_file(FILENAME_PIPE))) {
    return;
  }
  while (line = fgets_level_file(buf, 256, lfile)) {
    count++;
  }
  fclose(lfile);

  count = (count / 2) + 2;
  /* count is 1 more than entries */
  allowed_pipe_cmds = (char **) xmalloc(sizeof(char *) * count);
  memset(allowed_pipe_cmds, 0, sizeof(char *) * count);
  pipe_cmd_args = (int *) xmalloc(sizeof(int *) * count);
  memset(pipe_cmd_args, 0, sizeof(int *) * count);

  if (!(lfile = fopen_level_file(FILENAME_PIPE))) {
    return;
  }
  count = 0;
  while (line = fgets_level_file(buf, 256, lfile)) {
    allowed_pipe_cmds[count] = strdup(line);
    if (!(line = fgets_level_file(buf, 256, lfile))) {
      free(allowed_pipe_cmds[count]);
      allowed_pipe_cmds[count] = NULL;
      break;
    }
    pipe_cmd_args[count] = atoi(line);
    /* limit to between 1 and 256 */
    if (pipe_cmd_args[count] < 1 || pipe_cmd_args[count] > 256) {
      free(allowed_pipe_cmds[count]);
      allowed_pipe_cmds[count] = NULL;
      pipe_cmd_args[count] = 0;
      break;
    }
    count++;
  }
  fclose(lfile);
  return;
}

static int
init_vyatta_restricted_mode()
{
  if (!(vyatta_user_level_dir = getenv("VYATTA_USER_LEVEL_DIR"))) {
    /* level dir not set, return failure */
    return 0;
  }

  /* set the default restricted mode based on level */
  set_default_mode();

  /* set the allowed commands */
  set_allowed_op_cmds();
  set_allowed_cfg_cmds();
  set_allowed_pipe_cmds();

  return 1;
}

int
in_vyatta_restricted_mode(enum vyatta_restricted_type type)
{
  char *rval = getenv("VYATTA_RESTRICTED_MODE");
  int output = 0;
  int full = 0;

  if (!vyatta_user_level_dir && !init_vyatta_restricted_mode()) {
    /* init failed, return false (not restricted) */
    return 0;
  }

  output = vyatta_default_output_restricted;
  full = vyatta_default_full_restricted;
  
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

