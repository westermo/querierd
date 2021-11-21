#!/bin/sh
# Verifies basic querier operation on a single interface

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

# Requires ethtool to disable UDP checksum offloading
print "Check deps ..."
check_dep tshark

print "Creating world ..."
for iface in eth0 eth1 eth3; do
    ip link add $iface type dummy
    ip link set $iface up
    ip link set $iface multicast on
done
ip addr add 192.168.1.1/24 dev eth0

ip -br l
ip -br a

print "Creating config ..."
cat <<EOF > "/tmp/$NM/config"
eth0
EOF

print "Starting collector(s) ..."
tshark -lni eth0 -w "/tmp/$NM/pcap" 2>/dev/null &
echo $! >> "/tmp/$NM/PIDs"
sleep 1

print "Starting bridged ..."
../src/bridged -f "/tmp/$NM/config" -l debug -n &
echo $! >> "/tmp/$NM/PIDs"

sleep 2
kill_pids

print "Analyzing pcap ..."
lines=$(tshark -n -r "/tmp/$NM/pcap" 2>/dev/null | grep "IGMPv3 50 Membership Query" | tee -a "/tmp/$NM/result" | wc -l)
cat "/tmp/$NM/result"

echo " => $lines IGMP Query, expected 1"
# shellcheck disable=SC2086 disable=SC2166
[ $lines -eq 1 ] || FAIL

OK
