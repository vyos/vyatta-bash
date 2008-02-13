/* vyatta-restricted.h -- header for Vyatta restricted mode functionality */

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

#if !defined(_VYATTA_RESTRICTED_H_)
#define _VYATTA_RESTRICTED_H_

#include <stdio.h>

#include "command.h"

enum vyatta_restricted_type { OUTPUT, FULL };
extern int in_vyatta_restricted_mode __P((enum vyatta_restricted_type));
extern int is_vyatta_command __P((char *, COMMAND *));
extern void vyatta_check_expansion __P((COMMAND *));
extern void vyatta_reset_hist_expansion();

#endif /* _VYATTA_RESTRICTED_H_ */

