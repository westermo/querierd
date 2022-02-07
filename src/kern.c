/*
 * Parts of this program has been derived from mrouted.  It is covered
 * by the license in the accompanying file named "LICENSE".
 */

#include "defs.h"

int curttl = 0;

/*
 * Set the socket receiving buffer. `bufsize` is the preferred size,
 * `minsize` is the smallest acceptable size.
 */
void k_set_rcvbuf(int bufsize, int minsize)
{
    int delta = bufsize / 2;

    /*
     * Set the socket buffer.  If we can't set it as large as we want, search around
     * to try to find the highest acceptable value.  The highest acceptable value
     * being smaller than minsize is a fatal error.
     */
    if (setsockopt(igmp_socket, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0) {
        bufsize -= delta;
        while (1) {
            if (delta > 1)
                delta /= 2;

            if (setsockopt(igmp_socket, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0) {
                bufsize -= delta;
            } else {
                if (delta < 1024)
                    break;
                bufsize += delta;
            }
        }
        if (bufsize < minsize) {
            logit(LOG_ERR, 0, "OS-allowed recv buffer size %u < app min %u", bufsize, minsize);
            /*NOTREACHED*/
        }
    }
}


/*
 * Set/reset the IP_HDRINCL option. My guess is we don't need it for raw
 * sockets, but having it here won't hurt. Well, unless you are running
 * an older version of FreeBSD (older than 2.2.2). If the multicast
 * raw packet is bigger than 208 bytes, then IP_HDRINCL triggers a bug
 * in the kernel and "panic". The kernel patch for netinet/ip_raw.c
 * coming with this distribution fixes it.
 */
void k_hdr_include(int bool)
{
#ifdef IP_HDRINCL
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_HDRINCL, &bool, sizeof(bool)) < 0)
        logit(LOG_ERR, errno, "Failed setting socket IP_HDRINCL %u", bool);
#endif
}


/*
 * For IGMP reports we need to know incoming interface since proxy reporters
 * may use source IP 0.0.0.0, so we cannot rely on find_vif_direct().
 */
void k_set_pktinfo(int val)
{
#ifdef IP_PKTINFO
    if (setsockopt(igmp_socket, SOL_IP, IP_PKTINFO, &val, sizeof(val)) < 0)
	logit(LOG_ERR, errno, "Failed setting socket IP_PKTINFO to %d", val);
#endif
}


/*
 * Set the default TTL for the multicast packets outgoing from this socket.
 */
void k_set_ttl(int t)
{
    uint8_t ttl;

    ttl = t;
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
	logit(LOG_ERR, errno, "Failed setting IP_MULTICAST_TTL %u", ttl);

    curttl = t;
}

/*
 * Set the IP_MULTICAST_IF option on local interface ifa.
 */
void k_set_if(uint32_t ifa)
{
    struct in_addr adr;

    adr.s_addr = ifa;
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_MULTICAST_IF, &adr, sizeof(adr)) < 0) {
        if (errno == EADDRNOTAVAIL || errno == EINVAL)
            return;
        logit(LOG_ERR, errno, "Failed setting IP_MULTICAST_IF to %s",
              inet_fmt(ifa, s1, sizeof(s1)));
    }
}


/*
 * Join a multicast group.
 */
void k_join(uint32_t grp, int ifindex)
{
    struct ip_mreqn mreq;

    mreq.imr_multiaddr.s_addr = grp;
    mreq.imr_ifindex = ifindex;

    if (setsockopt(igmp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
	switch (errno) {
	    case ENOBUFS:
		logit(LOG_ERR, 0, "Maxed out groups per socket, please adjust "
		      "/proc/sys/net/ipv4/igmp_max_memberships\n"
		      "You need at least 3x the number of VIFs you want to run"
		      "mrouted on; 3 x 32 = 96.  Default: 20");
		break;

	    default:
		logit(LOG_WARNING, errno, "Cannot join group %s on ifindex %d",
		      inet_fmt(grp, s1, sizeof(s1)), ifindex);
		break;
	}
    }
}


/*
 * Leave a multicast group.
 */
void k_leave(uint32_t grp, int ifindex)
{
    struct ip_mreqn mreq;

    mreq.imr_multiaddr.s_addr = grp;
    mreq.imr_ifindex = ifindex;

    if (setsockopt(igmp_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
	logit(LOG_WARNING, errno, "Cannot leave group %s on ifindex %d",
	      inet_fmt(grp, s1, sizeof(s1)), ifindex);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "cc-mode"
 * End:
 */
