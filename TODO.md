Rough Plan for querierd
=======================

Planned for v0.6
----------------

 - Look for better IP than 169 for each interface
 - Add basic bridge per-VLAN setup instead of setupd


Planned for v1.0
----------------

 - Graft MLD querier functionality from pim6sd
 - Add "show mdb [vid NUM]" command to dump bridge's mdb in WeOS alt view
 - Add support for optional view of reserved FRNT VLANs.  In the igmp app
   this was handled by a separate flag `-f` that activated filtering.  We
   currently always filter those VLANs.
 - Bridge configuration support, e.g. snooping on/off, query interval etc.
 - Proper conf file, support for:
   - granular enable/disable of IGMP/MLD
   - static join per interface, remember socket filter!

