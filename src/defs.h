/*
 * Parts of this program has been derived from mrouted.  It is covered
 * by the license in the accompanying file named "LICENSE".
 */
#ifndef QUERIERD_DEFS_H_
#define QUERIERD_DEFS_H_

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#if defined(__bsdi__) || (defined(SunOS) && SunOS < 50)
#include <sys/sockio.h>
#endif /* bsdi || SunOS 4.x */
#include <time.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/igmp.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <osreldate.h>
#endif /* __FreeBSD__ */
#if defined(__bsdi__) || (defined(__FreeBSD__) && __FreeBSD_version >= 220000) || defined(__FreeBSD_kernel__)
#define rtentry kernel_rtentry
#include <net/route.h>
#undef rtentry
#endif /* bsdi or __FreeBSD_version >= 220000 */
#ifdef __linux__
#define _LINUX_IN_H             /* For Linux <= 2.6.25 */
#include <linux/types.h>
#include <linux/mroute.h>
#else
#include <netinet/ip_mroute.h>
#endif
#if defined(HAVE_STRLCPY)
#include <string.h>
#endif
#if defined(HAVE_STRTONUM)
#include <stdlib.h>
#endif
#if defined(HAVE_PIDFILE)
#if defined(OpenBSD) || defined(NetBSD)
#include <util.h>
#else
#include <libutil.h>
#endif
#endif

typedef void (*cfunc_t) (void*);
typedef void (*ihfunc_t) (int);

#include "dvmrp.h"
#include "igmpv2.h"
#include "igmpv3.h"
#include "vif.h"
#include "pathnames.h"

/*
 * Miscellaneous constants and macros.
 */

/* Older versions of UNIX don't really give us true raw sockets.
 * Instead, they expect ip_len and ip_off in host byte order, and also
 * provide them to us in that format when receiving raw frames.
 *
 * This list could probably be made longer, e.g., SunOS and __bsdi__
 */
#if defined(__NetBSD__) ||					\
    (defined(__FreeBSD__) && (__FreeBSD_version < 1100030)) ||	\
    (defined(__OpenBSD__) && (OpenBSD < 200311))
#define HAVE_IP_HDRINCL_BSD_ORDER
#endif

#define FALSE		0
#define TRUE		1

#define EQUAL(s1, s2)	(strcmp((s1), (s2)) == 0)
#define ARRAY_LEN(a)    (sizeof((a)) / sizeof((a)[0]))

#define TIMER_INTERVAL	2

#define	DEL_RTE_GROUP	0
#define	DEL_ALL_ROUTES	1
			    /* for Deleting kernel table entries */

#define JAN_1970	2208988800UL	/* 1970 - 1900 in seconds */

#if defined(_AIX) || (defined(BSD) && BSD >= 199103)
#define	HAVE_SA_LEN
#endif

/*
 * Extensions to errno for singaling to cfparse.y
 */
#define ENOTMINE 	2000
#define ELOOPBACK	2001
#define ERMTLOCAL	2002
#define EDUPLICATE	2003

/*
 * External declarations for global variables and functions.
 */
#define RECV_BUF_SIZE 8192
extern uint8_t		*recv_buf;
extern uint8_t		*send_buf;
extern int		igmp_socket;
extern int		router_alert;
extern uint32_t		router_timeout;
extern uint32_t		allhosts_group;
extern uint32_t		allrtrs_group;
extern uint32_t		allreports_group;
extern uint32_t		dvmrp_group;
extern uint32_t		dvmrp_genid;
extern uint32_t		igmp_response_interval;
extern uint32_t		igmp_query_interval;
extern uint32_t		igmp_last_member_interval;
extern uint32_t		igmp_robustness;
extern uint32_t		virtual_time;

extern int		loglevel;
extern int		use_syslog;
extern int		running;
extern int		haveterminal;
extern int		did_final_init;
extern time_t           mrouted_init_time;

extern int		routes_changed;
extern int		delay_change_reports;
extern unsigned		nroutes;

extern vifi_t		numvifs;
extern int		vifs_down;
extern int		udp_socket;

#define MAX_INET_BUF_LEN 19
extern char		s1[MAX_INET_BUF_LEN];
extern char		s2[MAX_INET_BUF_LEN];
extern char		s3[MAX_INET_BUF_LEN];
extern char		s4[MAX_INET_BUF_LEN];

#ifndef IGMP_PIM
#define	IGMP_PIM	0x14
#endif
#ifndef IPPROTO_IPIP
#define	IPPROTO_IPIP	4
#endif

/*
 * The original multicast releases defined
 * IGMP_HOST_{MEMBERSHIP_QUERY,MEMBERSHIP_REPORT,NEW_MEMBERSHIP_REPORT
 *   ,LEAVE_MESSAGE}.  Later releases removed the HOST and inserted
 * the IGMP version number.  NetBSD inserted the version number in
 * a different way.  mrouted uses the new names, so we #define them
 * to the old ones if needed.
 */
#if !defined(IGMP_MEMBERSHIP_QUERY) && defined(IGMP_HOST_MEMBERSHIP_QUERY)
#define	IGMP_MEMBERSHIP_QUERY		IGMP_HOST_MEMBERSHIP_QUERY
#define	IGMP_V2_LEAVE_GROUP		IGMP_HOST_LEAVE_MESSAGE
#endif
#ifndef	IGMP_V1_MEMBERSHIP_REPORT
#ifdef	IGMP_HOST_MEMBERSHIP_REPORT
#define	IGMP_V1_MEMBERSHIP_REPORT	IGMP_HOST_MEMBERSHIP_REPORT
#define	IGMP_V2_MEMBERSHIP_REPORT	IGMP_HOST_NEW_MEMBERSHIP_REPORT
#endif
#ifdef	IGMP_v1_HOST_MEMBERSHIP_REPORT
#define	IGMP_V1_MEMBERSHIP_REPORT	IGMP_v1_HOST_MEMBERSHIP_REPORT
#define	IGMP_V2_MEMBERSHIP_REPORT	IGMP_v2_HOST_MEMBERSHIP_REPORT
#endif
#endif
#if defined(__FreeBSD__)		/* From FreeBSD 8.x */
#define IGMP_V3_MEMBERSHIP_REPORT       IGMP_v3_HOST_MEMBERSHIP_REPORT
#else
#define IGMP_V3_MEMBERSHIP_REPORT	0x22	/* Ver. 3 membership report */
#endif

/*
 * NetBSD also renamed the mtrace types.
 */
#if !defined(IGMP_MTRACE_RESP) && defined(IGMP_MTRACE_REPLY)
#define	IGMP_MTRACE_RESP		IGMP_MTRACE_REPLY
#define	IGMP_MTRACE			IGMP_MTRACE_QUERY
#endif

/* main.c */
extern char	       *ident;
extern char	       *prognm;
extern char	       *config_file;
extern const char      *versionstring;
extern int		cache_lifetime;
extern int		prune_lifetime;
extern int		mrt_table_id;
extern int              debug_list(int, char *, size_t);
extern int              debug_parse(char *);
extern void             restart(void);
extern char *		scaletime(time_t);
extern int		register_input_handler(int, ihfunc_t);
extern void		deregister_input_handler(int);

/* log.c */
extern void             log_init(char *);
extern int		log_str2lvl(char *);
extern const char *	log_lvl2str(int);
extern int		log_list(char *, size_t);
extern void		logit(int, int, const char *, ...);
extern void             resetlogging(void *);

/* igmp.c */
extern void		igmp_init(void);
extern void		igmp_exit(void);
extern void		accept_igmp(int, size_t);
extern size_t		build_igmp(uint32_t, uint32_t, int, int, uint32_t, int);
extern void		send_igmp(uint32_t, uint32_t, int, int, uint32_t, int);
extern char *		igmp_packet_kind(uint32_t, uint32_t);
extern int		igmp_debug_kind(uint32_t, uint32_t);

/* timer.c */
extern void		timer_init(void);
extern void		timer_exit(void);
extern void		timer_stop_all(void);
extern void		timer_age_queue(time_t);
extern int		timer_next_delay(void);
extern int		timer_set(time_t, cfunc_t, void *);
extern int		timer_get(int);
extern int		timer_clear(int);

/* vif.c */
extern void		init_vifs(void);
extern void		zero_vif(struct uvif *, int);
extern int		install_uvif(struct uvif *);
extern void		check_vif_state(void);
extern struct uvif     *find_uvif(vifi_t);
extern vifi_t		find_vif(int);
extern void		age_vifs(void);
extern void		stop_all_vifs(void);
extern void		accept_group_report(int, uint32_t, uint32_t, uint32_t, int);
extern void		query_groups(void *);
extern void		accept_leave_message(int, uint32_t, uint32_t, uint32_t);
extern void		accept_membership_query(int, uint32_t, uint32_t, uint32_t, int, int);
extern void             accept_membership_report(int, uint32_t, uint32_t, struct igmpv3_report *, ssize_t);

/* config.c */
extern void		config_set_ifflag(uint32_t flag);
extern struct uvif     *config_find_ifname(char *nm);
extern struct uvif     *config_find_ifaddr(in_addr_t addr);
extern struct uvif     *config_init_tunnel(in_addr_t lcl_addr, in_addr_t rmt_addr, uint32_t flags);
extern void		config_vifs_correlate(void);
extern void		config_vifs_from_kernel(void);

/* cfparse.y */
extern void		config_vifs_from_file(void);

/* inet.c */
extern int		inet_valid_group(uint32_t);
extern int		inet_valid_host(uint32_t);
extern int		inet_valid_mask(uint32_t);
extern int		inet_valid_subnet(uint32_t, uint32_t);
extern char            *inet_name(uint32_t, int);
extern char            *inet_fmt(uint32_t, char *, size_t);
extern char            *inet_fmts(uint32_t, uint32_t, char *, size_t);
extern uint32_t		inet_parse(char *, int);
extern int		inet_cksum(uint16_t *, uint32_t);

/* kern.c */
extern int              curttl;

extern void		k_set_rcvbuf(int, int);
extern void		k_hdr_include(int);
extern void		k_set_pktinfo(int);
extern void		k_set_ttl(int);
extern void		k_set_loop(int);
extern void		k_set_if(uint32_t);
extern void		k_join(uint32_t, uint32_t);
extern void		k_leave(uint32_t, uint32_t);

#ifndef HAVE_STRLCPY
extern size_t		strlcpy(char *dst, const char *src, size_t siz);
#endif

#ifndef HAVE_STRLCAT
extern size_t		strlcat(char *dst, const char *src, size_t siz);
#endif

#ifndef HAVE_STRTONUM
extern long long	strtonum(const char *numstr, long long minval, long long maxval, const char **errstrp);
#endif

#ifndef HAVE_PIDFILE
extern int		pidfile(const char *basename);
#endif

#endif /* QUERIERD_DEFS_H_ */
