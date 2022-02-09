#!/bin/sh
# Verifies interfaces being created late and removed at runime

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

# Requires ethtool to disable UDP checksum offloading
print "Check deps ..."
check_dep socat

print "Creating world ..."
addif eth0 192.168.0.1/24

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

print "Analyzing ..."
echo "show"    |socat - UNIX-CONNECT:"/tmp/$NM/sock" | tee "/tmp/$NM/show"

grep "eth0" "/tmp/$NM/show" | grep -q "192.168.0.1" || FAIL

addif eth1 192.168.1.1/24
echo "show"    |socat - UNIX-CONNECT:"/tmp/$NM/sock" | tee "/tmp/$NM/show"
grep "eth0" "/tmp/$NM/show" | grep -q "192.168.0.1" || FAIL

delif eth0
echo "show"    |socat - UNIX-CONNECT:"/tmp/$NM/sock" | tee "/tmp/$NM/show"
grep "eth0" "/tmp/$NM/show" | grep -q "192.168.0.1" && FAIL

OK
