Rough Plan for querierd
=======================

Planned for v0.4
----------------

 - Automatically detect interfaces being added/removed from system
   When starting up (early) the system may not yet have created all its
   interfaces.  E.g., VLAN interfaces on top of a bridge.
 - querierctl tool to display status and show igmp queriers
 - Look into timeout value for queriers in status output
 - Fix "show" IPC command, should be "show igmp"


Planned for v1.0
----------------

 - Graft MLD querier functionality from pim6sd
 - Proper conf file, support for:
   - granular enable/disable of IGMP/MLD
   - static join per interface, remember socket filter!

