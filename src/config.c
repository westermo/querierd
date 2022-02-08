/*
 * Parts of this program has been derived from mrouted.  It is covered
 * by the license in the accompanying file named "LICENSE".
 */

#include <ifaddrs.h>
#include "defs.h"

/*
 * Exported variables.
 */
TAILQ_HEAD(ifaces, ifi) ifaces = TAILQ_HEAD_INITIALIZER(ifaces);

void config_set_ifflag(uint32_t flag)
{
    struct ifi *ifi;

    TAILQ_FOREACH(ifi, &ifaces, ifi_link)
	ifi->ifi_flags |= flag;
}

struct ifi *config_iface_iter(int first)
{
    static struct ifi *next = NULL;
    struct ifi *ifi;

    if (first)
	ifi = TAILQ_FIRST(&ifaces);
    else
	ifi = next;

    if (ifi)
	next = TAILQ_NEXT(ifi, ifi_link);

    return ifi;
}

struct ifi *config_find_ifname(char *nm)
{
    struct ifi *ifi;

    if (!nm) {
	errno = EINVAL;
	return NULL;
    }

    TAILQ_FOREACH(ifi, &ifaces, ifi_link) {
        if (!strcmp(ifi->ifi_name, nm))
            return ifi;
    }

    return NULL;
}

struct ifi *config_find_ifaddr(in_addr_t addr)
{
    struct ifi *ifi;

    TAILQ_FOREACH(ifi, &ifaces, ifi_link) {
	if (addr == ifi->ifi_curr_addr)
            return ifi;
    }

    return NULL;
}

struct ifi *config_find_iface(int ifindex)
{
    struct ifi *ifi;

    TAILQ_FOREACH(ifi, &ifaces, ifi_link) {
	if (ifindex == ifi->ifi_ifindex)
            return ifi;
    }

    return NULL;
}

struct ifi *config_iface_add(char *ifname)
{
    struct ifi *ifi;
    int ifindex;

    ifindex = if_nametoindex(ifname);
    if (!ifindex) {
	logit(LOG_WARNING, errno, "Failed reading ifindex for %s, skipping", ifname);
	return NULL;
    }

    ifi = calloc(1, sizeof(struct ifi));
    if (!ifi) {
	logit(LOG_ERR, errno, "failed allocating memory for iflist");
	return NULL;
    }

    iface_zero(ifi);
    ifi->ifi_ifindex = ifindex;
    strlcpy(ifi->ifi_name, ifname, sizeof(ifi->ifi_name));

    TAILQ_INSERT_TAIL(&ifaces, ifi, ifi_link);

    return ifi;
}

static struct ifi *addr_add(int ifindex, struct sockaddr *sa, unsigned int flags)
{
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;
    struct phaddr *pa;
    struct ifi *ifi;

    /*
     * Ignore any interface for an address family other than IP
     */
    if (!sa || sa->sa_family != AF_INET)
	return NULL;

    /*
     * Ignore loopback interfaces and interfaces that do not support
     * multicast.
     */
    if ((flags & (IFF_LOOPBACK|IFF_MULTICAST)) != IFF_MULTICAST)
	return NULL;

    ifi = config_find_iface(ifindex);
    if (!ifi)
	return NULL;

    /* kernel promotes secondary addresses, we know all addrs already */
    TAILQ_FOREACH(pa, &ifi->ifi_addrs, pa_link) {
	if (pa->pa_addr == sin->sin_addr.s_addr)
	    return NULL;	/* Already have it */
    }

    pa = calloc(1, sizeof(*pa));
    if (!pa) {
	logit(LOG_ERR, errno, "Failed allocating address for %s", ifi->ifi_name);
	return NULL;
    }

    pa->pa_addr  = sin->sin_addr.s_addr;
    TAILQ_INSERT_TAIL(&ifi->ifi_addrs, pa, pa_link);

    if (!(flags & IFF_UP))
	ifi->ifi_flags |= IFIF_DOWN;

    logit(LOG_DEBUG, 0, "New address %s for %s flags %p",
	  inet_fmt(pa->pa_addr, s1, sizeof(s1)), ifi->ifi_name, flags);

    return ifi;
}

static struct ifi *addr_del(int ifindex, struct sockaddr *sa)
{
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;
    struct phaddr *pa, *tmp;
    struct ifi *ifi;

    /*
     * Ignore any interface for an address family other than IP
     */
    if (!sa || sa->sa_family != AF_INET)
	return NULL;

    ifi = config_find_iface(ifindex);
    if (!ifi)
	return NULL;

    TAILQ_FOREACH_SAFE(pa, &ifi->ifi_addrs, pa_link, tmp) {
	if (pa->pa_addr != sin->sin_addr.s_addr)
	    continue;

	TAILQ_REMOVE(&ifi->ifi_addrs, pa, pa_link);
	logit(LOG_DEBUG, 0, "Drop address %s for %s", inet_fmt(pa->pa_addr, s1, sizeof(s1)),
	      ifi->ifi_name);
	free(pa);
	return ifi;
    }

    return NULL;
}

void config_iface_addr_add(int ifindex, struct sockaddr *sa, unsigned int flags)
{
    struct ifi *ifi;

    ifi = addr_add(ifindex, sa, flags);
    if (ifi) {
	if (ifi->ifi_flags & IFIF_DISABLED) {
	    logit(LOG_DEBUG, 0, "    %s disabled, no election", ifi->ifi_name);
	    return;
	}
	if (ifi->ifi_flags & IFIF_DOWN) {
	    logit(LOG_DEBUG, 0, "    %s down, no election", ifi->ifi_name);
	    return;
	}

	iface_check_election(ifi);
    }
}

void config_iface_addr_del(int ifindex, struct sockaddr *sa)
{
    struct ifi *ifi;

    ifi = addr_del(ifindex, sa);
    if (ifi) {
	if (ifi->ifi_flags & IFIF_DISABLED) {
	    logit(LOG_DEBUG, 0, "    %s disabled, no election", ifi->ifi_name);
	    return;
	}
	if (ifi->ifi_flags & IFIF_DOWN) {
	    logit(LOG_DEBUG, 0, "    %s down, no election", ifi->ifi_name);
	    return;
	}

	iface_check_election(ifi);
    }
}

/*
 * Query the kernel to find network interfaces that are multicast-capable
 */
void config_iface_from_kernel(void)
{
    struct ifaddrs *ifa, *ifap;

    if (getifaddrs(&ifap) < 0)
	logit(LOG_ERR, errno, "getifaddrs");

    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
	int ifindex;

	ifindex = if_nametoindex(ifa->ifa_name);
	if (!ifindex)
	    continue;

	addr_add(ifindex, ifa->ifa_addr, ifa->ifa_flags);
    }

    freeifaddrs(ifap);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "cc-mode"
 * End:
 */
