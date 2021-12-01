Bridge Querier Helper
=====================
[![License Badge][]][License] [![GitHub Status][]][GitHub]

This daemon is an IGMP querier helper for the Linux bridge.


Configuration
-------------

```ApacheConf
# Global settings followed by per-interface enable/disable of features

# Query interval can be [1,1024], default 125.  Recommended not go below 10
#igmp-query-interval 125

# Last member query interval [1,1024], default 1.  The igmp-robustness
# setting controls the last member query count.
#igmp-query-last-member-interval 1

# Robustness can be [2,10], default 2.  Recommended to use 2
#igmp-robustness 2

# IP Option Router Alert is enabled by default
#no router-alert

# Disble all, or enable plus one of the IGMP versions, wich is the startup
# version, with fallback to older versions if older clients appear.
# iface <IFNAME> [enable | disable] [igmpv1 | igmpv2 | igmpv3]
```


Origin & References
-------------------

This project is based on the [mrouted][] project, with DNA strands also from
the [pimd][] project.  It should be quite easy to also add MLD/MLDv2 querier
functionality from the [pim6sd][] project, because they all share a the same
ancestor (mrouted).

The [project][1] is maintained by Westermo Network Technologies, and due to
its origin, licensed under the same license as mrouted.

[1]:             https://github.com/westermo/querierd/
[GitHub]:        https://github.com/westermo/querierd/actions/workflows/build.yml/
[GitHub Status]: https://github.com/westermo/querierd/actions/workflows/build.yml/badge.svg
[mrouted]:       https://github.com/troglobit/mrouted/
[pimd]:          https://github.com/troglobit/pimd/
[pim6sd]:        https://github.com/troglobit/pim6sd/
[License]:       http://www.openbsd.org/cgi-bin/cvsweb/src/usr.sbin/mrouted/LICENSE
[License Badge]: https://img.shields.io/badge/License-BSD%203--Clau
