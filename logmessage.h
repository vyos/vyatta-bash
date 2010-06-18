/* Format of messages sent on /vbash message queue.

   This code was originally developed by Vyatta, Inc.
   Copyright (C) 2010 Vyatta, Inc.
 */

struct command_log {
	pid_t	pid;
	int	status;
	time_t	endtime;
	uid_t	uid, euid;
	gid_t	gid, egid;
	char	name[UT_NAMESIZE];
	char	tty[UT_LINESIZE];
	char	command[0];
};
