#!/bin/sh
# ssh to the Miyoo running Onion. Onion's dropbear accepts root with a blank password, which
# OpenSSH only supplies through an askpass helper, hence the SSH_ASKPASS dance.
# Usage: tools/miyoo-ssh.sh <ip> [command...]
ip="$1"; shift
askpass=$(mktemp)
printf '#!/bin/sh\necho ""\n' > "$askpass"
chmod +x "$askpass"
SSH_ASKPASS="$askpass" SSH_ASKPASS_REQUIRE=force DISPLAY=dummy \
	ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=8 \
	    -o PreferredAuthentications=password -o NumberOfPasswordPrompts=1 "root@$ip" "$@"
rc=$?
rm -f "$askpass"
exit $rc
