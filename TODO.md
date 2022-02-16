Rough Plan for querierd
=======================

Planned for v0.4
----------------

 - Write mini-HowTo: Setting up the Linux bridge with snooping per VLAN


Planned for v1.0
----------------

 - Graft MLD querier functionality from pim6sd
 - Add "show mdb [vid NUM]" command to dump bridge's mdb in WeOS alt view
 - Add support for optional view of reserved FRNT VLANs.  In the igmp app
   this was handled by a separate flag `-f` that activated filtering.  We
   currently always filter those VLANs.
 - Proper conf file, support for:
   - granular enable/disable of IGMP/MLD
   - static join per interface, remember socket filter!

