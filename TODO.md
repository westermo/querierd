Ideas for v1.0
==============

 - Graft MLD querier functionality from pim6sd
 - Proper conf file, support for:
   - granular enable/disable of IGMP/MLD
   - configurable query interval
   - static join
   - see proposal below
 - Update Makefile.am, include text files in dist


.conf file format
-----------------

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
#iface <IFNAME> [enable | disable] [igmpv1 | igmpv2 | igmpv3]
```

