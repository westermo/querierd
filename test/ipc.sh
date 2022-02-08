#!/bin/sh
# Verifies basic IPC operation

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

# Requires ethtool to disable UDP checksum offloading
print "Check deps ..."
check_dep socat

print "Creating world ..."
for iface in eth0 eth1 eth2; do
    ip link add $iface type dummy
    ip link set $iface up
    ip link set $iface multicast on
done
ip addr add 192.168.0.1/24 dev eth0
ip addr add 192.168.1.1/24 dev eth1
ip addr add 192.168.1.2/24 dev eth1
ip addr add 169.254.0.1/16 dev eth1
ip addr add 192.168.3.1/24 dev eth1

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

print "Starting querierd ..."
../src/querierd -f "/tmp/$NM/config" -p "/tmp/$NM/pid" -l debug -n -u "/tmp/$NM/sock" &
echo $! >> "/tmp/$NM/PIDs"
sleep 2

print "IPC output ..."
echo "help"    |socat - UNIX-CONNECT:"/tmp/$NM/sock"
echo "version" |socat - UNIX-CONNECT:"/tmp/$NM/sock"
echo
echo "status"  |socat - UNIX-CONNECT:"/tmp/$NM/sock"
echo "show"    |socat - UNIX-CONNECT:"/tmp/$NM/sock" | tee "/tmp/$NM/show"

print "Analyzing ..."
kill_pids

grep "eth0" "/tmp/$NM/show" | grep -q "192.168.0.1" || FAIL
grep "eth1" "/tmp/$NM/show" | grep -q "192.168.1.1" || FAIL
grep "eth2" "/tmp/$NM/show" | grep -q "0.0.0.0"     || FAIL

OK
