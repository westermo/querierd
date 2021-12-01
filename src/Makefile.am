# For replacement functions in lib/
AUTOMAKE_OPTIONS = subdir-objects

# -D_GNU_SOURCE Use GNU extensions, where possible
# -D_BSD_SOURCE Use functions derived from 4.3 BSD Unix rather than POSIX.1
#               In GLIBC >= v2.20 this is replaced with -D_DEFAULT_SOURCE,
#               but to build on older GLIBC systems we now need both ...
AM_CPPFLAGS      = -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CPPFLAGS     += -DSYSCONFDIR=\"@sysconfdir@\" -DRUNSTATEDIR=\"@runstatedir@\" -DPRESERVEDIR=\"@localstatedir@/lib\"
if LINUX
AM_CPPFLAGS     += -DIOCTL_OK_ON_RAW_SOCKET
endif
AM_CFLAGS        = -W -Wall -Wextra -Wno-unused -Wno-unused-parameter

sbin_PROGRAMS    = querierd
querierd_SOURCES  = main.c config.c			\
		   defs.h dvmrp.h			\
		   igmp.c igmpv2.h igmpv3.h 		\
		   inet.c kern.c log.c 			\
		   pathnames.h queue.h 			\
		   timer.c 				\
		   vif.c vif.h
querierd_CPPFLAGS = -DREGISTER_HANDLER $(AM_CPPFLAGS)
querierd_LDADD    = $(LIBS) $(LIBOBJS)

