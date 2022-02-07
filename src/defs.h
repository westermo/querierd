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
#define _LINUX_IN_H             /* For Linux <= 2.6.25 */
#include <linux/types.h>
#include <linux/mroute.h>
#if defined(HAVE_STRLCPY)
#include <string.h>
#endif
#if defined(HAVE_STRTONUM)
#include <stdlib.h>
#endif
#if defined(HAVE_PIDFILE)
#include <libutil.h>
#endif

typedef void (*cfunc_t) (void*);
typedef void (*ihfunc_t) (int);

#include "igmpv2.h"
#include "igmpv3.h"
#include "vif.h"
#include "pathnames.h"
#include "pev.h"

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
extern uint32_t		igmp_response_interval;
extern uint32_t		igmp_query_interval;
extern uint32_t		igmp_last_member_interval;
extern uint32_t		igmp_robustness;

extern int		loglevel;
extern int		use_syslog;
extern int		running;
extern int		haveterminal;
extern int		did_final_init;

#define MAX_INET_BUF_LEN 19
extern char		s1[MAX_INET_BUF_LEN];
extern char		s2[MAX_INET_BUF_LEN];
extern char		s3[MAX_INET_BUF_LEN];
extern char		s4[MAX_INET_BUF_LEN];

/*
 * Limit on length of route data
 */
#define MAX_IP_PACKET_LEN	576
#define MIN_IP_HEADER_LEN	20
#define IP_HEADER_RAOPT_LEN	(router_alert ? 24 : 20)
#define MAX_IP_HEADER_LEN	60
#define MAX_DVMRP_DATA_LEN \
		( MAX_IP_PACKET_LEN - MAX_IP_HEADER_LEN - IGMP_MINLEN )

/* NetBSD 6.1, for instance, does not have IPOPT_RA defined. */
#ifndef IPOPT_RA
#define IPOPT_RA		148
#endif

/*
 * The IGMPv2 <netinet/in.h> defines INADDR_ALLRTRS_GROUP, but earlier
 * ones don't, so we define it conditionally here.
 */
#ifndef INADDR_ALLRTRS_GROUP
					/* address for multicast mtrace msg */
#define INADDR_ALLRTRS_GROUP	(uint32_t)0xe0000002	/* 224.0.0.2 */
#endif

#ifndef INADDR_ALLRPTS_GROUP
#define INADDR_ALLRPTS_GROUP    ((in_addr_t)0xe0000016) /* 224.0.0.22, IGMPv3 */
#endif

#ifndef INADDR_MAX_LOCAL_GROUP
#define INADDR_MAX_LOCAL_GROUP	(uint32_t)0xe00000ff	/* 224.0.0.255 */
#endif

/*
 * Checks if addr is IPv4LL
 */
#ifndef IN_LINKLOCAL
#define IN_LINKLOCALNETNUM 0xa9fe0000
#define IN_LINKLOCAL(addr) ((addr & IN_CLASSB_NET) == IN_LINKLOCALNETNUM)
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
extern void		send_igmp(int, uint32_t, uint32_t, int, int, uint32_t, int);
extern char *		igmp_packet_kind(uint32_t, uint32_t);
extern int		igmp_debug_kind(uint32_t, uint32_t);

/* vif.c */
extern void		init_vifs(void);
extern void		zero_vif(struct uvif *);
extern void		check_vif_state(void);
extern void		router_timeout_cb(int, void *);
extern void		stop_all_vifs(void);
extern void		accept_group_report(int, uint32_t, uint32_t, uint32_t, int);
extern void		query_groups(int, void *);
extern void		accept_leave_message(int, uint32_t, uint32_t, uint32_t);
extern void		accept_membership_query(int, uint32_t, uint32_t, uint32_t, int, int);
extern void             accept_membership_report(int, uint32_t, uint32_t, struct igmpv3_report *, ssize_t);

/* config.c */
extern void		config_set_ifflag(uint32_t flag);
extern struct uvif     *config_iface_iter(int first);
extern struct uvif     *config_iface_add(char *ifname);
extern struct uvif     *config_find_ifname(char *nm);
extern struct uvif     *config_find_ifaddr(in_addr_t addr);
extern struct uvif     *config_find_iface(int ifindex);
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
extern void		k_set_if(int);
extern void		k_join(uint32_t, int);
extern void		k_leave(uint32_t, int);

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
