/*
 * Parts of this program has been derived from mrouted.  It is covered
 * by the license in the accompanying file named "LICENSE".
 */
#ifndef QUERIERD_IFACE_H_
#define QUERIERD_IFACE_H_

#include <stdint.h>
#include "queue.h"

struct iface {
    TAILQ_ENTRY(iface) uv_link;		/* link to next/prev interface       */
    TAILQ_HEAD(,listaddr) uv_static;    /* list of static groups (phyints)   */
    TAILQ_HEAD(,listaddr) uv_groups;    /* list of local groups  (phyints)   */
    TAILQ_HEAD(,phaddr) uv_addrs;	/* Secondary addresses               */
    uint32_t	     uv_flags;	        /* VIFF_ flags defined below         */
    char	     uv_name[IFNAMSIZ]; /* interface name                    */
    int		     uv_ifindex;        /* Primarily for Linux systems       */
    uint32_t	     uv_curr_addr;      /* Current address of this interface */
    uint32_t	     uv_prev_addr;      /* Previous address of this interace */
    struct listaddr *uv_querier;        /* IGMP querier (one or none)        */
    int		     uv_igmpv1_warn;    /* To rate-limit IGMPv1 warnings     */
};

#define VIFF_DOWN		0x000100	/* kernel state of interface */
#define VIFF_DISABLED		0x000200	/* administratively disabled */
#define VIFF_QUERIER		0x000400	/* I am the subnet's querier */
#define VIFF_IGMPV1		0x000800	/* Act as an IGMPv1 Router   */
#define	VIFF_IGMPV2		0x001000	/* Act as an IGMPv2 Router   */

struct phaddr {
    TAILQ_ENTRY(phaddr) pa_link;
    uint32_t	        pa_addr;
};

struct listaddr {
    TAILQ_ENTRY(listaddr) al_link;	/* link to next/prev addr           */
    uint32_t	     al_addr;		/* local group or neighbor address  */
    uint32_t	     al_mtime;		/* mtime from virtual_time, for IPC */
    time_t	     al_ctime;		/* entry creation time		    */
    uint32_t	     al_reporter;	/* a host which reported membership */
    int		     al_timerid;	/* timer for group membership	    */
    int		     al_query;		/* timer for repeated leave query   */
    uint8_t	     al_pv;		/* group/router protocol version    */
    int 	     al_pv_timerid;	/* timer for version switch         */
    uint16_t	     al_flags;		/* flags related to neighbor/group  */
};

#define	NBRF_STATIC_GROUP	0x4000	/* Static group entry		    */

#endif /* QUERIERD_IFACE_H_ */

/**
 * Local Variables:
 *  c-file-style: "cc-mode"
 * End:
 */
