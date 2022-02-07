#!/bin/sh
# Verifies basic querier operation on two interfaces
# Checks for exactly one query from each interface,
# i.e., triggers error for too many queries
# TODO:
#       - verify source address of query
#       - verify query type/version and contents

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

# Requires ethtool to disable UDP checksum offloading
print "Check deps ..."
check_dep tshark

print "Creating world ..."
for iface in eth0 eth1 eth3 eth4; do
    ip link add $iface type dummy
    ip link set $iface up
    ip link set $iface multicast on
done
ip addr add 192.168.0.1/24 dev eth0
ip addr add 192.168.1.1/24 dev eth1
ip addr add 192.168.1.2/24 dev eth1
ip addr add 192.168.3.1/24 dev eth1
ip addr add 192.168.2.1/24 dev eth2

ip -br l
ip -br a

print "Creating config ..."
cat <<EOF > "/tmp/$NM/config"
iface eth0 enable igmpv3
iface eth1 enable igmpv3
iface eth2 enable
iface eth3 enable
EOF
cat "/tmp/$NM/config"

print "Starting collector(s) ..."
tshark -lni eth0 -w "/tmp/$NM/eth0.pcap" 2>/dev/null &
echo $! >> "/tmp/$NM/PIDs"
tshark -lni eth1 -w "/tmp/$NM/eth1.pcap" 2>/dev/null &
echo $! >> "/tmp/$NM/PIDs"
sleep 1

print "Starting querierd ..."
../src/querierd -f "/tmp/$NM/config" -p "/tmp/$NM/pid" -l debug -n &
echo $! >> "/tmp/$NM/PIDs"

sleep 2
kill_pids

print "Analyzing pcap ..."
lines1=$(tshark -n -r "/tmp/$NM/eth0.pcap" 2>/dev/null | grep "IGMPv3 50 Membership Query" | tee -a "/tmp/$NM/result" | wc -l)
lines2=$(tshark -n -r "/tmp/$NM/eth1.pcap" 2>/dev/null | grep "IGMPv3 50 Membership Query" | tee -a "/tmp/$NM/result" | wc -l)
cat "/tmp/$NM/result"

echo " => $lines1 IGMP Query on eth0, expected 1"
echo " => $lines2 IGMP Query on eth1, expected 1"
# shellcheck disable=SC2086 disable=SC2166
[ $lines1 -eq 1 -a $lines2 -eq 1 ] || FAIL

OK
