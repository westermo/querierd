# For replacement functions in lib/
AUTOMAKE_OPTIONS = subdir-objects

# -D_GNU_SOURCE Use GNU extensions, where possible
# -D_BSD_SOURCE Use functions derived from 4.3 BSD Unix rather than POSIX.1
#               In GLIBC >= v2.20 this is replaced with -D_DEFAULT_SOURCE,
#               but to build on older GLIBC systems we now need both ...
AM_CPPFLAGS      = -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CPPFLAGS     += -DSYSCONFDIR=\"@sysconfdir@\" -DRUNSTATEDIR=\"@runstatedir@\" -DPRESERVEDIR=\"@localstatedir@/lib\"
AM_CFLAGS        = -W -Wall -Wextra -Wno-unused -Wno-unused-parameter

sbin_PROGRAMS    = querierd
querierd_SOURCES  = main.c cfparse.y config.c		\
		   defs.h dvmrp.h			\
		   iface.c iface.h netlink.c		\
		   igmp.c igmpv2.h igmpv3.h 		\
		   inet.c ipc.c kern.c log.c 		\
		   pev.c pev.h				\
		   pathnames.h queue.h
querierd_CPPFLAGS = $(AM_CPPFLAGS)
querierd_LDADD    = $(LIBS) $(LIBOBJS)

