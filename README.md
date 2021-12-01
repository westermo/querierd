Bridge Querier Helper
=====================
[![License Badge][]][License] [![GitHub Status][]][GitHub]

This daemon is an IGMP querier helper for the Linux bridge.


Configuration
-------------

By default `querierd` is passive on all interfaces.  To activate IGMPv3
querier, add whitespace separated interface names to the .conf file.

> **Note:** this is a temporary file format, a later release will include
>           more advanced settings, without being backwards compatible!


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
[License]:       http://www.openbsd.org/cgi-bin/cvsweb/src/usr.sbin/mrouted/LICENSE
[License Badge]: https://img.shields.io/badge/License-BSD%203--Clause-blue.svg
[mrouted]:       https://github.com/troglobit/mrouted/
[pimd]:          https://github.com/troglobit/pimd/
[pim6sd]:        https://github.com/troglobit/pim6sd/
