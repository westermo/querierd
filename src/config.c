/*
 * Parts of this program has been derived from mrouted.  It is covered
 * by the license in the accompanying file named "LICENSE".
 */

#include <ifaddrs.h>
#include "defs.h"

/*
 * Exported variables.
 */
TAILQ_HEAD(vifs, uvif) vifs = TAILQ_HEAD_INITIALIZER(vifs);

void config_set_ifflag(uint32_t flag)
{
    struct uvif *uv;

    TAILQ_FOREACH(uv, &vifs, uv_link)
	uv->uv_flags |= flag;
}

struct uvif *config_iface_iter(int first)
{
    static struct uvif *next = NULL;
    struct uvif *uv;

    if (first)
	uv = TAILQ_FIRST(&vifs);
    else
	uv = next;

    if (uv)
	next = TAILQ_NEXT(uv, uv_link);

    return uv;
}

struct uvif *config_find_ifname(char *nm)
{
    struct uvif *uv;

    if (!nm) {
	errno = EINVAL;
	return NULL;
    }

    TAILQ_FOREACH(uv, &vifs, uv_link) {
        if (!strcmp(uv->uv_name, nm))
            return uv;
    }

    return NULL;
}

struct uvif *config_find_ifaddr(in_addr_t addr)
{
    struct uvif *uv;

    TAILQ_FOREACH(uv, &vifs, uv_link) {
	if (addr == uv->uv_lcl_addr)
            return uv;
    }

    return NULL;
}

struct uvif *config_find_iface(int ifi)
{
    struct uvif *uv;

    TAILQ_FOREACH(uv, &vifs, uv_link) {
	if (ifi == uv->uv_ifindex)
            return uv;
    }

    return NULL;
}

/*
 * Ignore any kernel interface that is disabled, or connected to the
 * same subnet as one already installed.
 */
static int check_vif(struct uvif *v)
{
    struct uvif *uv;
    vifi_t vifi;

    TAILQ_FOREACH(uv, &vifs, uv_link) {
	if (v == uv)
	    continue;

	if (uv->uv_flags & VIFF_DISABLED) {
	    logit(LOG_DEBUG, 0, "Skipping %s, disabled in configuration", uv->uv_name);
	    return -1;
	}

	if (((v->uv_lcl_addr & uv->uv_subnetmask) == uv->uv_subnet && uv->uv_subnet) ||
	    ((uv->uv_subnet  &  v->uv_subnetmask) ==  v->uv_subnet &&  v->uv_subnet)) {
	    logit(LOG_WARNING, 0, "ignoring %s on %s, same subnet as %s",
		  inet_fmt(v->uv_lcl_addr, s1, sizeof(s1)), v->uv_name, uv->uv_name);
	    return -1;
	}

	/*
	 * Same interface, but cannot have multiple VIFs on the same
	 * interface so add as secondary IP address (altnet) for RPF
	 */
	if (strcmp(v->uv_name, uv->uv_name) == 0) {
	    struct phaddr *ph;

	    ph = calloc(1, sizeof(*ph));
	    if (!ph) {
		logit(LOG_ERR, errno, "Failed allocating altnet on %s", uv->uv_name);
		break;
	    }

	    logit(LOG_INFO, 0, "Installing %s subnet %s as an altnet on %s",
		  v->uv_name,
		  inet_fmts(v->uv_subnet, v->uv_subnetmask, s2, sizeof(s2)),
		  uv->uv_name);

	    ph->pa_subnet      = v->uv_subnet;
	    ph->pa_subnetmask  = v->uv_subnetmask;
	    ph->pa_subnetbcast = v->uv_subnetbcast;

	    ph->pa_next = uv->uv_addrs;
	    uv->uv_addrs = ph;
	    return -1;
	}
    }

    return 0;
}

void config_vifs_correlate(void)
{
    struct listaddr *al, *al_tmp;
    struct uvif *uv, *v, *tmp;
    vifi_t vifi;

    TAILQ_FOREACH_SAFE(v, &vifs, uv_link, tmp) {
	if (check_vif(v)) {
	    logit(LOG_DEBUG, 0, "Dropping interface %s ...", v->uv_name);
	    TAILQ_REMOVE(&vifs, v, uv_link);
	    free(v);
	    continue;
	}

	logit(LOG_INFO, 0, "Registered %s (%s on subnet %s) ifi %d",
	      v->uv_name, inet_fmt(v->uv_lcl_addr, s1, sizeof(s1)),
	      inet_fmts(v->uv_subnet, v->uv_subnetmask, s2, sizeof(s2)), v->uv_ifindex);
    }
}

struct uvif *config_iface_add(char *ifname)
{
    struct uvif *uv;
    int ifi;

    ifi = if_nametoindex(ifname);
    if (!ifi) {
	logit(LOG_WARNING, errno, "Failed reading ifindex for %s, skipping", ifname);
	return NULL;
    }

    uv = calloc(1, sizeof(struct uvif));
    if (!uv) {
	logit(LOG_ERR, errno, "failed allocating memory for iflist");
	return NULL;
    }

    zero_vif(uv);
    uv->uv_ifindex = ifi;
    strlcpy(uv->uv_name, ifname, sizeof(uv->uv_name));

    TAILQ_INSERT_TAIL(&vifs, uv, uv_link);

    return uv;
}

/*
 * Query the kernel to find network interfaces that are multicast-capable
 */
void config_vifs_from_kernel(void)
{
    in_addr_t addr, mask, subnet;
    struct ifaddrs *ifa, *ifap;
    struct uvif *uv;
    vifi_t vifi;
    int flags;

    if (getifaddrs(&ifap) < 0)
	logit(LOG_ERR, errno, "getifaddrs");

    /*
     * Loop through all of the interfaces.
     */
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
	/*
	 * Ignore any interface for an address family other than IP.
	 */
	if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
	    continue;

	/*
	 * Ignore loopback interfaces and interfaces that do not support
	 * multicast.
	 */
	flags = ifa->ifa_flags;
	if ((flags & (IFF_LOOPBACK|IFF_MULTICAST)) != IFF_MULTICAST)
	    continue;

	uv = config_find_ifname(ifa->ifa_name);
	if (!uv)
	    continue;

	/*
	 * Perform some sanity checks on the address and subnet, ignore any
	 * interface whose address and netmask do not define a valid subnet.
	 */
	addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
	mask = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr.s_addr;
	subnet = addr & mask;
	if (!inet_valid_subnet(subnet, mask) || (addr != subnet && addr == (subnet & ~mask))) {
	    logit(LOG_WARNING, 0, "ignoring %s, has invalid address (%s) and/or mask (%s)",
		  ifa->ifa_name, inet_fmt(addr, s1, sizeof(s1)), inet_fmt(mask, s2, sizeof(s2)));
	    continue;
	}

	uv->uv_lcl_addr    = addr;
	uv->uv_subnet      = subnet;
	uv->uv_subnetmask  = mask;
	uv->uv_subnetbcast = subnet | ~mask;

	if (!(flags & IFF_UP))
	    uv->uv_flags |= VIFF_DOWN;
    }

    freeifaddrs(ifap);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "cc-mode"
 * End:
 */
