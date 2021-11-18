/*
 * Parts of this program has been derived from mrouted.  It is covered
 * by the license in the accompanying file named "LICENSE".
 */
#ifndef BRIDGED_PATHNAMES_H_
#define BRIDGED_PATHNAMES_H_

#include <paths.h>

#define _PATH_BRIDGED_CONF	SYSCONFDIR   "/%s.conf"
#define _PATH_BRIDGED_GENID	PRESERVEDIR  "/%s.genid"
#define _PATH_BRIDGED_RUNDIR    RUNSTATEDIR
#define _PATH_BRIDGED_SOCK	RUNSTATEDIR  "/%s.sock"

#endif /* BRIDGED_PATHNAMES_H_ */
