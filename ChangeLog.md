Change Log
==========

All relevant, user visible, changes are documented in this file.


[v0.3][] - 2022-02-08
---------------------

### Changes
  - Add NETLINK support for link up/down and address add/del
    - Enables seamless operation on interfaces with, e.g., DHCP address
	- Allows for bringing up interfaces long after daemon has started
  - Querier timer now operates per interface, starts when interfaces are
    brought into operation -- configuration remains a global setting
  - Very basic IPC support for querying status from daemon
  - Massive refactor/rename of internal APIs
  - Support for multicast output interface without an address
  - Support for join/leave on interface without an address


[v0.2][] - 2022-02-04
---------------------

### Changes
  - Add proper /etc/querierd.conf support to change:
    - query interval (QI)
	- query response interval (QRI)
	- query last member interval
	- robustness (QRV)
    - router timeout
	- router alert
	- interface on/off with IGMP version
  - Add sample querierd.conf

### Fixes
  - Ignore proxy querys, they must never win elections
  - Query jitter problem of several seconds


v0.1 - 2021-12-01
-----------------

Initial public release.

Limited IGMPv1/v2/v3 querier with hard-coded query interval, etc.  Put
interfaces in a .conf file, whitespace separated to enable querier.

[UNRELEASED]: https://github.com/westermo/querierd/compare/v0.3...HEAD
[v0.3]:       https://github.com/westermo/querierd/compare/v0.2...v0.3
[v0.2]:       https://github.com/westermo/querierd/compare/v0.1...v0.2
