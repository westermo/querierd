Bridge Querier Helper
=====================
[![License Badge][]][License] [![GitHub Status][]][GitHub] [![Coverity Status][]][Coverity Scan]

This daemon is a querier helper for the Linux bridge.  Currently IGMP
(IPv4) is supported, MLD (IPv6) querier support is planned.

The daemon comes with a little helper tool called `querierctl` which
can be used to check the status of IGMP per interface, but also to
dump the bridge's MDB in a more human-friendly format.  There is a
blog post describing how to set the bridge up and use `querierd`

  * https://westermo.github.io/2022/02/17/bridge-igmp-snooping/

For controlling `querierd` from another application, use the basic IPC
support that `querierctl` employs:

    echo "help" |socat - UNIX-CONNECT:/run/querierd.sock

> See `querierd -h` for help, e.g. to customize the IPC path.


Configuration
-------------

By default `querierd` is passive on all interfaces.  Use the following
settings to enable and tweak the defaults.  There is no way to configure
different IGMP/MLD settings per interface at the moment, only protocol
version.

    # /etc/querierd.conf syntax
    query-interval [1-1024]                   # default: 125 sec
    query-response-interval [1-1024]          # default: 10 sec
    query-last-member-interval [1-1024]       # default: 1
    robustness [2-10]                         # default: 2
    router-timeout [10-1024]                  # default: 255 sec
    
    iface IFNAME [enable] [igmpv2 | igmpv3]   # default: disable

Description:

  * `query-interval`: the interval between IGMP/MLD queries, when
    elected as querier for a LAN
  * `query-response-interval`: max response time to a query.  Can be
    used to control the burstiness of IGMP/MLD traffic
  * `query-last-member-interval`: time between group specific queries,
    the `robustness` setting controls the number of queries sent
  * `robustness`: controls the tolerance to loss of replies from end
    devices and the loss of elected queriers (above)
  * `router-timeout`: also known as *"other querier present interval"*,
    controls the timer used to detect when an elected querier stops
    sending queries.  When the timer expires `querierd` will initiate a
    query.  The default, when this is unset (commented out) is
    calculated based on: `robustness * query-interval +
    query-response-interval / 2`.  Setting this to any value overrides
    the RFC algorithm, which may be necessary in some scenarios, it is
    however strongly recommended to leave this setting commented out!

> **Note:** the daemon needs an address on interfaces to operate, it is
> expected that querierd runs on top of a bridge. Also, currently the
> daemon does not react automatically to IP address changes, so it needs
> to be SIGHUP'ed to use any new interface or address.


Motivation
----------

The Linux bridge recently gained support for per-VLAN IGMP/MLD snooping.
It stands fine on its own legs, with lots of new per-VLAN settings.  But
since it lives in the bridge, it cannot know about any VLAN interfaces
that may (or may not) sit on top of the bridge.  There are naming issues
(br0.1 vs vlan1), and the fact that an interface can have multiple IP
addresses assigned to it.  Therefore, the current bridge implementation
in `br_multicast.c` can only act as a proxy querier, i.e., for IGMP this
means it can only send queries (per-VLAN) with source IP 0.0.0.0.

To understand why this might be a problem there are two things to
consider:

  1. Some networks don't have a (dynamic) multicast router¹.  Usually the
     multicast router is the IGMP/MLD querier for a LAN, but some LANs
     consist only of (industrial) switches that try their best to limit
     the spread of multicast² on low-capacity links
  2. Some end-devices discard queries with source IP 0.0.0.0.  This is
     of course wrong, but good luck telling a PLC vendor they should
     change their embedded firmware of an aging product, or even the
     customer site to upgrade their locked-down system with a brand new
     firmware -- these people don't like change and sometimes because it
     comes with drawn-out re-certification processes

So, proxy queries are allowed per RFC, but that might not work with some
end-devices, and we can't disable proxy-query because some end-devices
don't do gratuitous join/leave.  Hence, we need a service to provide us
with an IGMP/MLD querier that can (at least) use one of the IP address
on the interfaces we have on top of the bridge.

Other limitations to be aware of in `br_multicast.c` (currently) is the
lack of IGMP/MLD version fallback.  I.e., per RFC, the LAN should start
out as IGMPv3 and when an end-device shows up with an IGMPv2 join then
the whole LAN should fall back to to IGMPv2, with a timer to return to
IGMPv3 if that end-device stops transmitting (IGMP control frames).  Of
course, optimizations can be done here, e.g., only falling back on the
given port, or not tracking (the timer) per individual end-devices.  It
is worth remembering, that the IGMP standard was written for old hubbed
networks, in those days everyone could see a single IGMPv2 message and
everyone could potentially confuse that end-device with a newer version.

_____
¹ E.g., Quagga/Frr PIM, or little mrouted/pimd/pimd-dense/pim6sd  
² remember, multicast acts like broadcast if it's not limited (by IGMP
  or MLD), so disabling IGMP/MLD snooping is not an option in these
  networks, they are what actually keep most of these sites afloat.


Origin & References
-------------------

This project is based on the [mrouted][] project, with DNA strands also from
the [pimd][] project.  It should be quite easy to also add MLD/MLDv2 querier
functionality from the [pim6sd][] project, because they all share a the same
ancestor (mrouted).

The [project][1] is maintained by Westermo Network Technologies, and due to
its origin, licensed under the same license as mrouted.

[1]:               https://github.com/westermo/querierd/
[GitHub]:          https://github.com/westermo/querierd/actions/workflows/build.yml/
[GitHub Status]:   https://github.com/westermo/querierd/actions/workflows/build.yml/badge.svg
[License]:         http://www.openbsd.org/cgi-bin/cvsweb/src/usr.sbin/mrouted/LICENSE
[License Badge]:   https://img.shields.io/badge/License-BSD%203--Clause-blue.svg
[Coverity Scan]:   https://scan.coverity.com/projects/24475
[Coverity Status]: https://scan.coverity.com/projects/24475/badge.svg
[mrouted]:         https://github.com/troglobit/mrouted/
[pimd]:            https://github.com/troglobit/pimd/
[pim6sd]:          https://github.com/troglobit/pim6sd/
