
#include <string.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include "defs.h"

static int id;
static int sd;


static void netlink_read(int sd, void *arg)
{
    char buffer[4096];
    ssize_t len;

    while ((len = recv(sd, buffer, sizeof(buffer), MSG_DONTWAIT)) > 0) {
	struct nlmsghdr *nlh = (struct nlmsghdr *)buffer;

        while ((NLMSG_OK(nlh, len)) && (nlh->nlmsg_type != NLMSG_DONE)) {
            if (nlh->nlmsg_type == RTM_NEWADDR || nlh->nlmsg_type == RTM_DELADDR) {
                struct ifaddrmsg *ifa = (struct ifaddrmsg *)NLMSG_DATA(nlh);
                struct rtattr *rth = IFA_RTA(ifa);
                int rtl = IFA_PAYLOAD(nlh);

                while (rtl && RTA_OK(rth, rtl)) {
                    if (rth->rta_type == IFA_LOCAL) {
#if 1
			struct in_addr *ina = (struct in_addr *)RTA_DATA(rth);
			struct sockaddr_in sin = { 0 };
			int flags;

			flags = IFF_UP | IFF_MULTICAST;
			sin.sin_family = ifa->ifa_family;
			sin.sin_addr = *ina;

			if (nlh->nlmsg_type == RTM_NEWADDR)
			    config_iface_addr_add(ifa->ifa_index, (struct sockaddr *)&sin, flags);
#else // debug
                        char name[IFNAMSIZ];
                        char addr[INET_ADDRSTRLEN];

			if_indextoname(ifa->ifa_index, name);
			inet_ntop(ifa->ifa_family, RTA_DATA(rth), addr, sizeof(addr));
			if (nlh->nlmsg_type == RTM_NEWADDR)
			    logit(LOG_DEBUG, 0, "ADD address for %s addr: %s", name, addr);
			else
			    logit(LOG_DEBUG, 0, "DEL address for %s addr: %s", name, addr);
#endif
                    }

                    rth = RTA_NEXT(rth, rtl);
                }
            }
            if (nlh->nlmsg_type == RTM_NEWLINK || nlh->nlmsg_type == RTM_DELLINK) {
                struct ifinfomsg *ifi = (struct ifinfomsg *)NLMSG_DATA(nlh);
#if 1
		iface_check(ifi->ifi_index, ifi->ifi_flags);
#else // debug
		if (nlh->nlmsg_type == RTM_NEWLINK)
		    logit(LOG_DEBUG, 0, "Link UP %d flags %p", ifi->ifi_index, ifi->ifi_flags);
		else
		    logit(LOG_DEBUG, 0, "Link DN %d flags %p", ifi->ifi_index, ifi->ifi_flags);
#endif
	    }
            nlh = NLMSG_NEXT(nlh, len);
        }
    }
}

void netlink_init(void)
{
    struct sockaddr_nl addr;

    sd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sd == -1) {
	logit(LOG_ERR, errno, "Failed opening NETLINK socket");
	return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_IPV4_IFADDR | RTMGRP_LINK;
    if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
	logit(LOG_ERR, errno, "Failed binding NETLINK socket");
	close(sd);
        return;
    }

    id = pev_sock_add(sd, netlink_read, NULL);
    if (id == -1)
	logit(LOG_ERR, errno, "Failed registering NETLINK handler");
}

void netlink_exit(void)
{
    pev_sock_del(id);
    close(sd);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "cc-mode"
 * End:
 */
