/*
 * Parts of this program has been derived from mrouted.  It is covered
 * by the license in the accompanying file named "LICENSE".
 */

#include "defs.h"

/*
 * Private variables.
 */
typedef struct {
    struct listaddr *g;
    int    ifi;
    int    delay;
    int    num;
} cbk_t;

static int query_timerid = -1;

/*
 * Forward declarations.
 */
static void start_iface          (struct iface *uv);
static void stop_iface           (struct iface *uv);

static void send_query         (struct iface *v, uint32_t dst, int code, uint32_t group);
static void query_groups       (int timeout, void *arg);

static void router_timeout_cb  (int timeout, void *arg);

static void delete_group_cb    (int timeout, void *arg);
static int  delete_group_timer (int ifi, struct listaddr *g, int tmo);

static void send_query_cb      (int timeout, void *arg);
static int  send_query_timer   (int ifi, struct listaddr *g, int delay, int num);

static void group_version_cb   (int timeout, void *arg);
static int  group_version_timer(int ifi, struct listaddr *g);


void iface_init(void)
{
    struct iface *uv;

    config_iface_from_file();
    config_iface_from_kernel();

    for (uv = config_iface_iter(1); uv; uv = config_iface_iter(0)) {
	if (uv->uv_flags & VIFF_DISABLED) {
	    logit(LOG_INFO, 0, "%s is disabled; skipping", uv->uv_name);
	    continue;
	}

	if (uv->uv_flags & VIFF_DOWN) {
	    logit(LOG_INFO, 0, "%s is not yet up; skipping", uv->uv_name);
	    continue;
	}

	logit(LOG_DEBUG, 0, "starting %s; interface now in service", uv->uv_name);
	start_iface(uv);
    }

    /*
     * Periodically query for local group memberships on all subnets for
     * which this router is the elected querier.
     */
    if (query_timerid > 0)
	pev_timer_del(query_timerid);
    query_timerid = pev_timer_add(0, igmp_query_interval * 1000000, query_groups, NULL);
}

void iface_exit(void)
{
    struct listaddr *a, *tmp;
    struct phaddr *ph;
    struct iface *uv;

    pev_timer_del(query_timerid);

    for (uv = config_iface_iter(1); uv; uv = config_iface_iter(0)) {
	if (uv->uv_querier) {
	    free(uv->uv_querier);
	    uv->uv_querier = NULL;
	}

	TAILQ_FOREACH_SAFE(a, &uv->uv_groups, al_link, tmp) {
	    TAILQ_REMOVE(&uv->uv_groups, a, al_link);
	    free(a);
	}

	while (uv->uv_addrs) {
	    ph = uv->uv_addrs;
	    uv->uv_addrs = ph->pa_next;
	    free(ph);
	}
	uv->uv_addrs = NULL;

	free(uv);
    }
}

/*
 * Note: remember to re-init all relevant TAILQ's in iface_init()!
 */
void iface_zero(struct iface *uv)
{
    uv->uv_flags	= VIFF_DISABLED;
    uv->uv_curr_addr	= 0;
    uv->uv_name[0]	= '\0';
    TAILQ_INIT(&uv->uv_static);
    TAILQ_INIT(&uv->uv_groups);
    uv->uv_querier	= NULL;
    uv->uv_igmpv1_warn	= 0;
    uv->uv_addrs	= NULL;
}

/*
 * Restart IGMP Querier election
 *
 * Start by figuring out the best local address for the iface.  Check if
 * the current address is better (RFC), make sure an IPv4LL doesn't win.
 * Usually we want a real address if available.  0.0.0.0 is reserved for
 * proxy querys, which we cannot do on a plain UDP socket, and they must
 * never win an election.  (Proxy queries should be sent by the bridge.)
 */
void iface_check_election(struct iface *uv)
{
    in_addr_t curr = uv->uv_curr_addr;
    struct phaddr *pa;

    for (pa = uv->uv_addrs; pa; pa = pa->pa_next) {
	in_addr_t cand = pa->pa_addr;

	logit(LOG_DEBUG, 0, "    candidate address %s ...", inet_fmt(cand, s1, sizeof(s1)));
	if (curr) {
	    if (ntohl(cand) >= ntohl(curr))
		continue;
	    if (IN_LINKLOCAL(ntohl(cand)))
		continue;
	}

	curr = cand;
    }

    if (curr != uv->uv_curr_addr) {
	logit(LOG_INFO, 0, "Using %s address %s", uv->uv_name, inet_fmt(curr, s1, sizeof(s1)));
	uv->uv_prev_addr = uv->uv_curr_addr;
	uv->uv_curr_addr = curr;
    }

    if (uv->uv_querier) {
	uint32_t cur = uv->uv_querier->al_addr;

	if (ntohl(uv->uv_curr_addr) < ntohl(cur)) {
	    logit(LOG_DEBUG, 0, "New local querier on %s", uv->uv_name);
	    pev_timer_del(uv->uv_querier->al_timerid);
	    free(uv->uv_querier);
	    uv->uv_querier = NULL;
	    goto elected;
	}
    } else {
	if (uv->uv_prev_addr == 0)
	    goto elected;
    }

    return;

  elected:
    /*
     * Until (new) neighbors are discovered, assume responsibility for
     * sending periodic group membership queries to the subnet.  Send
     * the first query.
     */
    uv->uv_flags |= VIFF_QUERIER;
    logit(LOG_DEBUG, 0, "Assuming querier duties on interface %s", uv->uv_name);
    send_query(uv, allhosts_group, igmp_response_interval * IGMP_TIMER_SCALE, 0);
}

void iface_check(int ifi, unsigned int flags)
{
    struct iface *uv;

    uv = config_find_iface(ifi);
    if (!uv) {
	logit(LOG_DEBUG, 0, "Cannot find ifi %d in configuration, skipping ...", ifi);
	return;
    }

    if (uv->uv_flags & VIFF_DOWN) {
	if (flags & IFF_UP) {
	    logit(LOG_NOTICE, 0, "%s has come up; interface now in service", uv->uv_name);
	    uv->uv_flags &= ~VIFF_DOWN;
	    start_iface(uv);
	}
    } else {
	if (!(flags & IFF_UP)) {
	    logit(LOG_NOTICE, 0, "%s has gone down; interface out of service", uv->uv_name);
	    stop_iface(uv);
	    uv->uv_flags |= VIFF_DOWN;
	}
    }
}

/*
 * See if any interfaces have changed from up state to down, or vice versa,
 * including any non-multicast-capable interfaces that are in use as local
 * tunnel end-points.  Ignore interfaces that have been administratively
 * disabled.
 */
void iface_check_state(void)
{
    static int checking_iface = 0;
    struct ifreq ifr;
    struct iface *uv;

    if (checking_iface)
	return;

    checking_iface = 1;
    for (uv = config_iface_iter(1); uv; uv = config_iface_iter(0)) {
	if (uv->uv_flags & VIFF_DISABLED)
	    continue;

	memset(&ifr, 0, sizeof(ifr));
	memcpy(ifr.ifr_name, uv->uv_name, sizeof(ifr.ifr_name));
	if (ioctl(igmp_socket, SIOCGIFFLAGS, &ifr) < 0)
	    logit(LOG_ERR, errno, "Failed ioctl SIOCGIFFLAGS for %s", ifr.ifr_name);

	iface_check(uv->uv_ifindex, ifr.ifr_flags);
    }

    checking_iface = 0;
}

static void send_query(struct iface *v, uint32_t dst, int code, uint32_t group)
{
    int datalen = 4;

    if (!v->uv_curr_addr) {
	/*
	 * If we send with source address 0.0.0.0 on a UDP socket the
	 * kernel will go dumpster diving to find a "suitable" address
	 * from another interface.  Obviously we don't want that ... we
	 * would've liked to be able to send a proxy query, but that's
	 * not possible unless SOCK_RAW, so we delegate the proxy query
	 * mechanism to the bridge and bail out here.
	 */
//	logit(LOG_DEBUG, 0, "Skipping send of query on %s, no address yet.", v->uv_name);
	return;
    }

    /*
     * IGMP version to send depends on the compatibility mode of the
     * interface:
     *  - IGMPv2: routers MUST send Periodic Queries truncated at the
     *    Group Address field (i.e., 8 bytes long).
     *  - IGMPv1: routers MUST send Periodic Queries with a Max Response
     *    Time of 0
     */
    if (v->uv_flags & VIFF_IGMPV2) {
	datalen = 0;
    } else if (v->uv_flags & VIFF_IGMPV1) {
	datalen = 0;
	code = 0;
    }

    logit(LOG_DEBUG, 0, "Sending %squery on %s",
	  (v->uv_flags & VIFF_IGMPV1) ? "v1 " :
	  (v->uv_flags & VIFF_IGMPV2) ? "v2 " : "v3 ",
	  v->uv_name);

    send_igmp(v->uv_ifindex, v->uv_curr_addr, dst, IGMP_MEMBERSHIP_QUERY,
	      code, group, datalen);
}

static void start_iface(struct iface *uv)
{
    struct listaddr *a;
    struct phaddr *p;

    /*
     * Join the ALL-ROUTERS multicast group on the interface.
     * This allows mtrace requests to loop back if they are run
     * on the multicast router.
     */
    k_join(allrtrs_group, uv->uv_ifindex);

    /* Join INADDR_ALLRPTS_GROUP to support IGMPv3 membership reports */
    k_join(allreports_group, uv->uv_ifindex);

    /* Check if we should assume the querier role */
    iface_check_election(uv);
}

static void stop_iface(struct iface *uv)
{
    struct listaddr *a, *tmp;

    /*
     * Discard all group addresses.  (No need to tell kernel;
     * the k_del_iface() call, below, will clean up kernel state.)
     */
    TAILQ_FOREACH_SAFE(a, &uv->uv_groups, al_link, tmp) {
	TAILQ_REMOVE(&uv->uv_groups, a, al_link);
	free(a);
    }
    /*
     * Depart from the ALL-ROUTERS multicast group on the interface.
     */
    k_leave(allrtrs_group, uv->uv_ifindex);

    /*
     * Depart from the ALL-REPORTS multicast group on the interface.
     */
    k_leave(allreports_group, uv->uv_ifindex);

    logit(LOG_DEBUG, 0, "Releasing querier duties on interface %s", uv->uv_name);
    uv->uv_flags &= ~VIFF_QUERIER;
}

/*
 * Send group membership queries on each interface for which I am querier.
 * Note that technically, there should be a timer per interface, as the
 * dynamics of querier election can cause the "right" time to send a
 * query to be different on different interfaces.  However, this simple
 * implementation only ever sends queries sooner than the "right" time,
 * so can not cause loss of membership (but can send more packets than
 * necessary)
 */
static void query_groups(int period, void *arg)
{
    struct iface *uv;
    int ifi;

    for (uv = config_iface_iter(1); uv; uv = config_iface_iter(0)) {
	if (uv->uv_flags & (VIFF_DOWN | VIFF_DISABLED))
	    continue;

	if (uv->uv_flags & VIFF_QUERIER)
	    send_query(uv, allhosts_group, igmp_response_interval * IGMP_TIMER_SCALE, 0);
    }
}

/*
 * Process an incoming host membership query.  Warn about
 * IGMP version mismatches, perform querier election, and
 * handle group-specific queries when we're not the querier.
 */
void accept_membership_query(int ifi, uint32_t src, uint32_t dst, uint32_t group, int tmo, int ver)
{
    struct iface *uv;

    uv = config_find_iface(ifi);
    if (!uv)
	return;

    if ((ver == 3 && (uv->uv_flags & VIFF_IGMPV2)) ||
	(ver == 2 && (uv->uv_flags & VIFF_IGMPV1))) {
	int i;

	/*
	 * Exponentially back-off warning rate
	 */
	i = ++uv->uv_igmpv1_warn;
	while (i && !(i & 1))
	    i >>= 1;

	if (i == 1) {
	    logit(LOG_WARNING, 0, "Received IGMPv%d report from %s on %s, configured for IGMPv%d",
		  ver, inet_fmt(src, s1, sizeof(s1)), uv->uv_name, uv->uv_flags & VIFF_IGMPV1 ? 1 : 2);
	}
    }

    if (uv->uv_querier == NULL || uv->uv_querier->al_addr != src) {
	uint32_t cur = uv->uv_querier ? uv->uv_querier->al_addr : uv->uv_curr_addr;

	/*
	 * This might be:
	 * - A query from a new querier, with a lower source address
	 *   than the current querier (who might be me)
	 * - A query from a new router that just started up and doesn't
	 *   know who the querier is.
	 * - A proxy query (source address 0.0.0.0), never wins elections
	 */
	if (!ntohl(src)) {
	    logit(LOG_DEBUG, 0, "Ignoring proxy query on %s", uv->uv_name);
	    return;
	}

	if (ntohl(src) < ntohl(cur)) {
	    logit(LOG_DEBUG, 0, "New querier %s (was %s) on %s",
		  inet_fmt(src, s1, sizeof(s1)), uv->uv_querier
		  ? inet_fmt(uv->uv_querier->al_addr, s2, sizeof(s2)) : "me", uv->uv_name);

	    if (!uv->uv_querier) {
		uv->uv_querier = calloc(1, sizeof(struct listaddr));
		if (!uv->uv_querier)
		    logit(LOG_ERR, errno, "%s(): Failed allocating memory", __func__);

		uv->uv_querier->al_timerid = pev_timer_add(router_timeout * 1000000, 0, router_timeout_cb, uv);
		uv->uv_flags &= ~VIFF_QUERIER;
	    }

	    time(&uv->uv_querier->al_ctime);
	    uv->uv_querier->al_addr = src;
	} else {
#if 0
	    logit(LOG_DEBUG, 0, "Ignoring query from %s; querier on %s is still %s",
		  inet_fmt(src, s1, sizeof(s1)), uv->uv_name,
		  uv->uv_querier ? inet_fmt(uv->uv_querier->al_addr, s2, sizeof(s2)) : "me");
#endif
	    return;
	}
    }

    /*
     * Reset the timer since we've received a query.
     */
    if (uv->uv_querier && src == uv->uv_querier->al_addr)
	pev_timer_set(uv->uv_querier->al_timerid, router_timeout * 1000000);

    /*
     * If this is a Group-Specific query which we did not source,
     * we must set our membership timer to [Last Member Query Count] *
     * the [Max Response Time] in the packet.
     */
    if (!(uv->uv_flags & (VIFF_IGMPV1|VIFF_QUERIER))
	&& group != 0 && src != uv->uv_curr_addr) {
	struct listaddr *g;

	logit(LOG_DEBUG, 0, "Group-specific membership query for %s from %s on %s, timer %d",
	      inet_fmt(group, s2, sizeof(s2)),
	      inet_fmt(src, s1, sizeof(s1)), uv->uv_name, tmo);

	TAILQ_FOREACH(g, &uv->uv_groups, al_link) {
	    if (group == g->al_addr && g->al_query == 0) {
		if (g->al_timerid > 0)
		    g->al_timerid = pev_timer_del(g->al_timerid);

		if (g->al_query > 0)
		    g->al_query = pev_timer_del(g->al_query);

		/* setup a timeout to remove the group membership */
		g->al_timerid = delete_group_timer(uv->uv_ifindex, g, IGMP_LAST_MEMBER_QUERY_COUNT
						   * tmo / IGMP_TIMER_SCALE);

		logit(LOG_DEBUG, 0, "Timer for grp %s on %s set to %d",
		      inet_fmt(group, s2, sizeof(s2)), uv->uv_name, pev_timer_get(g->al_timerid) / 1000);
		break;
	    }
	}
    }
}

static void group_debug(struct listaddr *g, char *s, int is_change)
{
    logit(LOG_DEBUG, 0, "%sIGMP v%d compatibility mode for group %s",
	  is_change ? "Change to " : "", g->al_pv, s);
}

/*
 * Process an incoming group membership report.
 */
void accept_group_report(int ifi, uint32_t src, uint32_t dst, uint32_t group, int r_type)
{
    struct listaddr *g;
    struct iface *uv;

    inet_fmt(src, s1, sizeof(s1));
    inet_fmt(dst, s2, sizeof(s2));
    inet_fmt(group, s3, sizeof(s3));

    /* Do not filter LAN scoped groups */
    if (ntohl(group) <= INADDR_MAX_LOCAL_GROUP) { /* group <= 224.0.0.255? */
	logit(LOG_DEBUG, 0, "    %-16s LAN scoped group, skipping.", s3);
	return;
    }

    uv = config_find_iface(ifi);
    if (!uv)
	return;

    logit(LOG_INFO, 0, "Accepting group membership report: src %s, dst %s, grp %s", s1, s2, s3);

    /*
     * Look for the group in our group list; if found, reset its timer.
     */
    TAILQ_FOREACH(g, &uv->uv_groups, al_link) {
	int old_report = 0;

	if (group == g->al_addr) {
	    if (g->al_flags & NBRF_STATIC_GROUP) {
		logit(LOG_DEBUG, 0, "Ignoring IGMP JOIN for static group %s on %s.", s3, s1);
		return;
	    }

	    switch (r_type) {
	    case IGMP_V1_MEMBERSHIP_REPORT:
		old_report = 1;
		if (g->al_pv > 1) {
		    g->al_pv = 1;
		    group_debug(g, s3, 1);
		}
		break;

	    case IGMP_V2_MEMBERSHIP_REPORT:
		old_report = 1;
		if (g->al_pv > 2) {
		    g->al_pv = 2;
		    group_debug(g, s3, 1);
		}
		break;

	    default:
		break;
	    }

	    g->al_reporter = src;

	    /** delete old timers, set a timer for expiration **/
	    if (g->al_query > 0)
		g->al_query = pev_timer_del(g->al_query);

	    if (g->al_timerid > 0)
		g->al_timerid = pev_timer_del(g->al_timerid);

	    g->al_timerid = delete_group_timer(uv->uv_ifindex, g, IGMP_GROUP_MEMBERSHIP_INTERVAL);

	    /*
	     * Reset timer for switching version back every time an older
	     * version report is received
	     */
	    if (g->al_pv < 3 && old_report) {
		if (g->al_pv_timerid)
		    g->al_pv_timerid = pev_timer_del(g->al_pv_timerid);

		g->al_pv_timerid = group_version_timer(uv->uv_ifindex, g);
	    }
	    break;
	}
    }

    /*
     * If not found, add it to the list and update kernel cache.
     */
    if (!g) {
	g = calloc(1, sizeof(struct listaddr));
	if (!g) {
	    logit(LOG_ERR, errno, "Failed allocating memory in %s:%s()", __FILE__, __func__);
	    return;
	}

	g->al_addr = group;

	switch (r_type) {
	case IGMP_V1_MEMBERSHIP_REPORT:
	    g->al_pv = 1;
	    break;

	case IGMP_V2_MEMBERSHIP_REPORT:
	    g->al_pv = 2;
	    break;

	default:
	    g->al_pv = 3;
	    break;
	}

	group_debug(g, s3, 0);

	/** set a timer for expiration **/
        g->al_query	= 0;
	g->al_reporter	= src;
	g->al_timerid	= delete_group_timer(uv->uv_ifindex, g, IGMP_GROUP_MEMBERSHIP_INTERVAL);

	/*
	 * Set timer for swithing version back if an older version
	 * report is received
	 */
	if (g->al_pv < 3)
	    g->al_pv_timerid = group_version_timer(uv->uv_ifindex, g);

	TAILQ_INSERT_TAIL(&uv->uv_groups, g, al_link);
	time(&g->al_ctime);
    }
}

/*
 * Process an incoming IGMPv2 Leave Group message, an IGMPv3 BLOCK(), or
 * IGMPv3 TO_IN({}) membership report.  Handles older version hosts.
 *
 * We detect IGMPv3 by the dst always being 0.
 */
void accept_leave_message(int ifi, uint32_t src, uint32_t dst, uint32_t group)
{
    struct listaddr *g;
    struct iface *uv;

    inet_fmt(src, s1, sizeof(s1));
    inet_fmt(group, s3, sizeof(s3));

    uv = config_find_iface(ifi);
    if (!uv)
	return;

    if (!(uv->uv_flags & VIFF_QUERIER) || (uv->uv_flags & VIFF_IGMPV1)) {
	logit(LOG_DEBUG, 0, "Ignoring group leave, not querier or interface in IGMPv1 mode.");
	return;
    }

    /*
     * Look for the group in our group list in order to set up a short-timeout
     * query.
     */
    TAILQ_FOREACH(g, &uv->uv_groups, al_link) {
	if (group != g->al_addr)
	    continue;

	if (g->al_flags & NBRF_STATIC_GROUP) {
	    logit(LOG_DEBUG, 0, "Ignoring IGMP LEAVE for static group %s on %s.", s3, s1);
	    return;
	}

	/* Ignore IGMPv2 LEAVE in IGMPv1 mode, RFC3376, sec. 7.3.2. */
	if (g->al_pv == 1) {
	    logit(LOG_DEBUG, 0, "Ignoring IGMP LEAVE for %s on %s, IGMPv1 host exists.", s3, s1);
	    return;
	}

	/* Ignore IGMPv3 BLOCK in IGMPv2 mode, RFC3376, sec. 7.3.2. */
	if (g->al_pv == 2 && dst == 0) {
	    logit(LOG_DEBUG, 0, "Ignoring IGMP BLOCK/TO_IN({}) for %s on %s, IGMPv2 host exists.", s3, s1);
	    return;
	}

	/* still waiting for a reply to a query, ignore the leave */
	if (g->al_query) {
	    logit(LOG_DEBUG, 0, "Ignoring IGMP LEAVE for %s on %s, pending group-specific query.", s3, s1);
	    return;
	}

	/** delete old timer set a timer for expiration **/
	if (g->al_timerid > 0)
	    g->al_timerid = pev_timer_del(g->al_timerid);

	/** send a group specific query **/
	g->al_query = send_query_timer(uv->uv_ifindex, g, igmp_last_member_interval,
				       IGMP_LAST_MEMBER_QUERY_COUNT);
	g->al_timerid = delete_group_timer(uv->uv_ifindex, g, igmp_last_member_interval
					   * (IGMP_LAST_MEMBER_QUERY_COUNT + 1));

	logit(LOG_DEBUG, 0, "Accepted group leave for %s on %s", s3, s1);
	return;
    }

    /*
     * We only get here when we couldn't find the group, or when there
     * still is a group-specific query pending, or when the group is in
     * older version compat, RFC3376.
     */
    logit(LOG_DEBUG, 0, "Ignoring IGMP LEAVE/BLOCK for %s on %s, group not found.", s3, s1);
}


/*
 * Loop through and process all sources in a v3 record.
 *
 * Parameters:
 *     r_type   Report type of IGMP message
 *     src      Src address of IGMP message
 *     dst      Multicast group
 *     sources  Pointer to the beginning of sources list in the IGMP message
 *     canary   Pointer to the end of IGMP message
 *
 * Returns:
 *     POSIX OK (0) if succeeded, non-zero on failure.
 */
int accept_sources(int ifi, int r_type, uint32_t src, uint32_t dst, uint8_t *sources,
    uint8_t *canary, int rec_num_sources)
{
    uint8_t *ptr;
    int j;

    for (j = 0, ptr = sources; j < rec_num_sources; ++j, src += 4) {
	struct in_addr *ina = (struct in_addr *)ptr;

        if ((ptr + 4) > canary) {
	    logit(LOG_DEBUG, 0, "Invalid IGMPv3 report, too many sources, would overflow.");
            return 1;
        }

	logit(LOG_DEBUG, 0, "Add source (%s,%s)", inet_fmt(ina->s_addr, s2, sizeof(s2)),
	      inet_fmt(dst, s1, sizeof(s1)));

        accept_group_report(ifi, src, ina->s_addr, dst, r_type);
    }

    return 0;
}


/*
 * Handle IGMP v3 membership reports (join/leave)
 */
void accept_membership_report(int ifi, uint32_t src, uint32_t dst, struct igmpv3_report *report, ssize_t reportlen)
{
    uint8_t *canary = (uint8_t *)report + reportlen;
    struct igmpv3_grec *record;
    int num_groups, i;

    num_groups = ntohs(report->ngrec);
    if (num_groups < 0) {
	logit(LOG_INFO, 0, "Invalid Membership Report from %s: num_groups = %d",
	      inet_fmt(src, s1, sizeof(s1)), num_groups);
	return;
    }

    logit(LOG_DEBUG, 0, "IGMP v3 report, %zd bytes, from %s to %s with %d group records.",
	  reportlen, inet_fmt(src, s1, sizeof(s1)), inet_fmt(dst, s2, sizeof(s2)), num_groups);

    record = &report->grec[0];

    for (i = 0; i < num_groups; i++) {
	struct in_addr  rec_group;
	uint8_t        *sources;
	int             rec_type;
	int             rec_auxdatalen;
	int             rec_num_sources;
	int             j, rc;
	int             record_size = 0;

	rec_num_sources = ntohs(record->grec_nsrcs);
	rec_auxdatalen = record->grec_auxwords;
	record_size = sizeof(struct igmpv3_grec) + sizeof(uint32_t) * rec_num_sources + rec_auxdatalen;
	if ((uint8_t *)record + record_size > canary) {
	    logit(LOG_INFO, 0, "Invalid group report %p > %p", (uint8_t *)record + record_size, canary);
	    return;
	}

	rec_type = record->grec_type;
	rec_group.s_addr = (in_addr_t)record->grec_mca;
	sources = (uint8_t *)record->grec_src;

	switch (rec_type) {
	    case IGMP_MODE_IS_EXCLUDE:
	    case IGMP_CHANGE_TO_EXCLUDE_MODE:
		if (rec_num_sources == 0) {
		    /* RFC 5790: TO_EX({}) can be interpreted as a (*,G)
		     *           join, i.e., to include all sources.
		     */
		    accept_group_report(ifi, src, 0, rec_group.s_addr, report->type);
		} else {
		    /* RFC 5790: LW-IGMPv3 does not use TO_EX({x}),
		     *           i.e., filter with non-null source.
		     */
		    logit(LOG_DEBUG, 0, "IS_EX/TO_EX({x}), not unsupported, RFC5790.");
		}
		break;

	    case IGMP_MODE_IS_INCLUDE:
	    case IGMP_CHANGE_TO_INCLUDE_MODE:
		if (rec_num_sources == 0) {
		    /* RFC5790: TO_IN({}) can be interpreted as an
		     *          IGMPv2 (*,G) leave.
		     */
		    accept_leave_message(ifi, src, 0, rec_group.s_addr);
		} else {
		    /* RFC5790: TO_IN({x}), regular RFC3376 (S,G)
		     *          join with >= 1 source, 'S'.
		     */
		    rc = accept_sources(ifi, report->type, src, rec_group.s_addr,
					sources, canary, rec_num_sources);
		    if (rc)
			return;
		}
		break;

	    case IGMP_ALLOW_NEW_SOURCES:
		/* RFC5790: Same as TO_IN({x}) */
		rc = accept_sources(ifi, report->type, src, rec_group.s_addr,
				    sources, canary, rec_num_sources);
		if (rc)
		    return;
		break;

	    case IGMP_BLOCK_OLD_SOURCES:
		/* RFC5790: Instead of TO_EX({x}) */
		for (j = 0; j < rec_num_sources; j++) {
		    uint8_t *gsrc = (uint8_t *)&record->grec_src[j];
		    struct in_addr *ina = (struct in_addr *)gsrc;

		    if (gsrc > canary) {
			logit(LOG_INFO, 0, "Invalid group record");
			return;
		    }

		    logit(LOG_DEBUG, 0, "Remove source[%d] (%s,%s)", j,
			  inet_fmt(ina->s_addr, s2, sizeof(s2)), inet_ntoa(rec_group));
		    accept_leave_message(ifi, src, 0, rec_group.s_addr);
		}
		break;

	    default:
		/* RFC3376: Unrecognized Record Type values MUST be silently ignored. */
		break;
	}

	record = (struct igmpv3_grec *)((uint8_t *)record + record_size);
    }
}

/*
 * When an active querier times out we assume the role here.
 */
static void router_timeout_cb(int timeout, void *arg)
{
    struct iface *uv = (struct iface *)arg;

    logit(LOG_DEBUG, 0, "Querier %s timed out", inet_fmt(uv->uv_querier->al_addr, s1, sizeof(s1)));
    pev_timer_del(uv->uv_querier->al_timerid);
    free(uv->uv_querier);
    uv->uv_querier = NULL;

    uv->uv_flags |= VIFF_QUERIER;
    send_query(uv, allhosts_group, igmp_response_interval * IGMP_TIMER_SCALE, 0);
}

/*
 * Time out old version compatibility mode
 */
static void group_version_cb(int timeout, void *arg)
{
    cbk_t *cbk = (cbk_t *)arg;
    struct iface *uv;

    uv = config_find_iface(cbk->ifi);
    if (!uv)
	return;

    if (cbk->g->al_pv < 3)
	cbk->g->al_pv++;

    logit(LOG_INFO, 0, "Switching IGMP compatibility mode from v%d to v%d for group %s on %s",
	  cbk->g->al_pv - 1, cbk->g->al_pv, inet_fmt(cbk->g->al_addr, s1, sizeof(s1)), uv->uv_name);

    if (cbk->g->al_pv < 3)
	pev_timer_set(cbk->g->al_pv_timerid, IGMP_GROUP_MEMBERSHIP_INTERVAL * 1000000);
    else {
	pev_timer_del(cbk->g->al_pv_timerid);
	free(cbk);
    }
}

/*
 * Set a timer to switch version back on an interface.
 */
static int group_version_timer(int ifi, struct listaddr *g)
{
    cbk_t *cbk;

    cbk = calloc(1, sizeof(cbk_t));
    if (!cbk) {
	logit(LOG_ERR, errno, "%s(): Failed allocating memory", __func__);
	return -1;
    }

    cbk->ifi = ifi;
    cbk->g   = g;

    return pev_timer_add(IGMP_GROUP_MEMBERSHIP_INTERVAL * 1000000, 0, group_version_cb, cbk);
}

/*
 * Time out record of a group membership on an interface.
 */
static void delete_group_cb(int timeout, void *arg)
{
    cbk_t *cbk = (cbk_t *)arg;
    struct listaddr *g = cbk->g;
    struct iface *uv;

    uv = config_find_iface(cbk->ifi);
    if (!uv)
	goto done;

    logit(LOG_DEBUG, 0, "Group membership timeout for %s on %s",
	  inet_fmt(cbk->g->al_addr, s1, sizeof(s1)), uv->uv_name);

    pev_timer_del(g->al_timerid);

    if (g->al_query > 0)
	g->al_query = pev_timer_del(g->al_query);

    if (g->al_pv_timerid > 0)
	g->al_pv_timerid = pev_timer_del(g->al_pv_timerid);

    TAILQ_REMOVE(&uv->uv_groups, g, al_link);
    free(g);
  done:
    free(cbk);
}

/*
 * Set a timer to delete the record of a group membership on an interface.
 */
static int delete_group_timer(int ifi, struct listaddr *g, int tmo)
{
    cbk_t *cbk;

    cbk = calloc(1, sizeof(cbk_t));
    if (!cbk) {
	logit(LOG_ERR, errno, "%s(): Failed allocating memory", __func__);
	return -1;
    }

    cbk->g = g;
    cbk->ifi = ifi;

    /* Record mtime for IPC "show igmp" */
//    g->al_mtime = virtual_time;

    return pev_timer_add(tmo * 1000000, 0, delete_group_cb, cbk);
}

/*
 * Send a group-specific query.
 */
static void send_query_cb(int timeout, void *arg)
{
    cbk_t *cbk = (cbk_t *)arg;
    struct iface *uv;

    uv = config_find_iface(cbk->ifi);
    if (!uv)
	goto end;

    send_query(uv, cbk->g->al_addr, cbk->delay * IGMP_TIMER_SCALE, cbk->g->al_addr);
    if (--cbk->num > 0) {
	pev_timer_set(cbk->g->al_query, cbk->delay * 1000000);
	return;
    }

  end:
    /* we're done, clear us from group */
    cbk->g->al_query = pev_timer_del(cbk->g->al_query);
    free(cbk);
}

/*
 * Set a timer to send a group-specific query.
 */
static int send_query_timer(int ifi, struct listaddr *g, int delay, int num)
{
    cbk_t *cbk;

    cbk = calloc(1, sizeof(cbk_t));
    if (!cbk) {
	logit(LOG_ERR, errno, "%s(): Failed allocating memory", __func__);
	return -1;
    }

    cbk->ifi   = ifi;
    cbk->g     = g;
    cbk->delay = delay;
    cbk->num   = num;

    return pev_timer_add(delay * 1000000, 0, send_query_cb, cbk);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "cc-mode"
 * End:
 */