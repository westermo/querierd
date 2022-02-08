/*
 * Parts of this program has been derived from mrouted.  It is covered
 * by the license in the accompanying file named "LICENSE".
 */

#include <ifaddrs.h>
#include "defs.h"

/*
 * Exported variables.
 */
TAILQ_HEAD(ifaces, iface) ifaces = TAILQ_HEAD_INITIALIZER(ifaces);

void config_set_ifflag(uint32_t flag)
{
    struct iface *uv;

    TAILQ_FOREACH(uv, &ifaces, uv_link)
	uv->uv_flags |= flag;
}

struct iface *config_iface_iter(int first)
{
    static struct iface *next = NULL;
    struct iface *uv;

    if (first)
	uv = TAILQ_FIRST(&ifaces);
    else
	uv = next;

    if (uv)
	next = TAILQ_NEXT(uv, uv_link);

    return uv;
}

struct iface *config_find_ifname(char *nm)
{
    struct iface *uv;

    if (!nm) {
	errno = EINVAL;
	return NULL;
    }

    TAILQ_FOREACH(uv, &ifaces, uv_link) {
        if (!strcmp(uv->uv_name, nm))
            return uv;
    }

    return NULL;
}

struct iface *config_find_ifaddr(in_addr_t addr)
{
    struct iface *uv;

    TAILQ_FOREACH(uv, &ifaces, uv_link) {
	if (addr == uv->uv_curr_addr)
            return uv;
    }

    return NULL;
}

struct iface *config_find_iface(int ifi)
{
    struct iface *uv;

    TAILQ_FOREACH(uv, &ifaces, uv_link) {
	if (ifi == uv->uv_ifindex)
            return uv;
    }

    return NULL;
}

struct iface *config_iface_add(char *ifname)
{
    struct iface *uv;
    int ifi;

    ifi = if_nametoindex(ifname);
    if (!ifi) {
	logit(LOG_WARNING, errno, "Failed reading ifindex for %s, skipping", ifname);
	return NULL;
    }

    uv = calloc(1, sizeof(struct iface));
    if (!uv) {
	logit(LOG_ERR, errno, "failed allocating memory for iflist");
	return NULL;
    }

    iface_zero(uv);
    uv->uv_ifindex = ifi;
    strlcpy(uv->uv_name, ifname, sizeof(uv->uv_name));

    TAILQ_INSERT_TAIL(&ifaces, uv, uv_link);

    return uv;
}

static struct iface *addr_add(int ifi, struct sockaddr *sa, unsigned int flags)
{
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;
    struct phaddr *pa;
    struct iface *uv;

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

    uv = config_find_iface(ifi);
    if (!uv)
	return NULL;

    /* kernel promotes secondary addresses, we know all addrs already */
    TAILQ_FOREACH(pa, &uv->uv_addrs, pa_link) {
	if (pa->pa_addr == sin->sin_addr.s_addr)
	    return NULL;	/* Already have it */
    }

    pa = calloc(1, sizeof(*pa));
    if (!pa) {
	logit(LOG_ERR, errno, "Failed allocating address for %s", uv->uv_name);
	return NULL;
    }

    pa->pa_addr  = sin->sin_addr.s_addr;
    TAILQ_INSERT_TAIL(&uv->uv_addrs, pa, pa_link);

    if (!(flags & IFF_UP))
	uv->uv_flags |= VIFF_DOWN;

    logit(LOG_DEBUG, 0, "New address %s for %s flags %p",
	  inet_fmt(pa->pa_addr, s1, sizeof(s1)), uv->uv_name, flags);

    return uv;
}

static struct iface *addr_del(int ifi, struct sockaddr *sa)
{
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;
    struct phaddr *pa, *tmp;
    struct iface *uv;

    /*
     * Ignore any interface for an address family other than IP
     */
    if (!sa || sa->sa_family != AF_INET)
	return NULL;

    uv = config_find_iface(ifi);
    if (!uv)
	return NULL;

    TAILQ_FOREACH_SAFE(pa, &uv->uv_addrs, pa_link, tmp) {
	if (pa->pa_addr != sin->sin_addr.s_addr)
	    continue;

	TAILQ_REMOVE(&uv->uv_addrs, pa, pa_link);
	logit(LOG_DEBUG, 0, "Drop address %s for %s", inet_fmt(pa->pa_addr, s1, sizeof(s1)),
	      uv->uv_name);
	free(pa);
	return uv;
    }

    return NULL;
}

void config_iface_addr_add(int ifi, struct sockaddr *sa, unsigned int flags)
{
    struct iface *uv;

    uv = addr_add(ifi, sa, flags);
    if (uv) {
	if (uv->uv_flags & VIFF_DISABLED) {
	    logit(LOG_DEBUG, 0, "    %s disabled, no election", uv->uv_name);
	    return;
	}
	if (uv->uv_flags & VIFF_DOWN) {
	    logit(LOG_DEBUG, 0, "    %s down, no election", uv->uv_name);
	    return;
	}

	iface_check_election(uv);
    }
}

void config_iface_addr_del(int ifi, struct sockaddr *sa)
{
    struct iface *uv;

    uv = addr_del(ifi, sa);
    if (uv) {
	if (uv->uv_flags & VIFF_DISABLED) {
	    logit(LOG_DEBUG, 0, "    %s disabled, no election", uv->uv_name);
	    return;
	}
	if (uv->uv_flags & VIFF_DOWN) {
	    logit(LOG_DEBUG, 0, "    %s down, no election", uv->uv_name);
	    return;
	}

	iface_check_election(uv);
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
	int ifi;

	ifi = if_nametoindex(ifa->ifa_name);
	if (!ifi)
	    continue;

	addr_add(ifi, ifa->ifa_addr, ifa->ifa_flags);
    }

    freeifaddrs(ifap);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "cc-mode"
 * End:
 */
