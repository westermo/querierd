#!/bin/sh
# Verifies querier operation on three interfaces, one which has a late
# address, another that wakes up a bit later.
#
# Test runs for 15 seconds with a query interval of 5 sec.
#
# 1st interface behaves as "normal" one static address and up from the
# start, we expect three queries from this interface.  However, we lose
# the primary address, so expect to see .0.2 promoted.
#
# 2nd interface has a later arrival of an IP address, we expect two Q,
# but might get three due to global query timer currently.
#
# 3rd interfae is down at start, comes up with one Q for last interval
#

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

#DEBUG="-l debug"

# Requires ethtool to disable UDP checksum offloading
print "Check deps ..."
check_dep tshark

print "Creating world ..."
for iface in eth0 eth1 eth2; do
    ip link add $iface type dummy
    ip link set $iface up
    ip link set $iface multicast on
done

ip addr add 192.168.0.1/24 dev eth0
ip addr add 192.168.0.2/24 dev eth0
# no ip address at startup for eth1
ip addr add 192.168.2.1/24 dev eth2

# down at startup
ip link set eth2 down

ip -br l
ip -br a

print "Creating config ..."
cat <<EOF > "/tmp/$NM/config"
query-interval 5
iface eth0 enable igmpv3
iface eth1 enable igmpv3
iface eth2 enable igmpv3
EOF
cat "/tmp/$NM/config"

print "Starting collector(s) ..."
tshark -lni eth0 -w "/tmp/$NM/eth0.pcap" 2>/dev/null &
echo $! >> "/tmp/$NM/PIDs"
tshark -lni eth1 -w "/tmp/$NM/eth1.pcap" 2>/dev/null &
echo $! >> "/tmp/$NM/PIDs"
sleep 1

print "Starting querierd ..."
# shellcheck disable=SC2086
../src/querierd -f "/tmp/$NM/config" -p "/tmp/$NM/pid" $DEBUG -n &
echo $! >> "/tmp/$NM/PIDs"

sleep 4
ip addr del 192.168.0.1/24 dev eth0
ip addr add 192.168.1.1/24 dev eth1
sleep 4
ip link set dev eth2 up
tshark -lni eth2 -w "/tmp/$NM/eth2.pcap" 2>/dev/null &
echo $! >> "/tmp/$NM/PIDs"
sleep 5
kill_pids

print "Analyzing pcap ..."
#echo "__[eth0]_______________________________________________________________________"
#tshark -n -r "/tmp/$NM/eth0.pcap" 2>/dev/null
#echo "__[eth1]_______________________________________________________________________"
#tshark -n -r "/tmp/$NM/eth1.pcap" 2>/dev/null
#echo "__[eth2]_______________________________________________________________________"
#tshark -n -r "/tmp/$NM/eth2.pcap" 2>/dev/null
#echo "_______________________________________________________________________________"
lines1=$(tshark -n -r "/tmp/$NM/eth0.pcap" 2>/dev/null | grep "IGMPv3 50 Membership Query" | tee -a "/tmp/$NM/result" | wc -l)
lines2=$(tshark -n -r "/tmp/$NM/eth1.pcap" 2>/dev/null | grep "IGMPv3 50 Membership Query" | tee -a "/tmp/$NM/result" | wc -l)
lines3=$(tshark -n -r "/tmp/$NM/eth2.pcap" 2>/dev/null | grep "IGMPv3 50 Membership Query" | tee -a "/tmp/$NM/result" | wc -l)
cat "/tmp/$NM/result"

echo " => $lines1 IGMP Query on eth0, expected 3"
echo " => $lines2 IGMP Query on eth1, expected 2"
echo " => $lines3 IGMP Query on eth1, expected 1"
# shellcheck disable=SC2086 disable=SC2166
[ $lines1 -eq 3 -a $lines2 -ge 2 -a $lines3 -eq 1 ] || FAIL

OK
