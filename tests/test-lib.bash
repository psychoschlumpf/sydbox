#!/bin/bash
# vim: set sw=4 et sts=4 tw=80 :

# Reset environment
export LANG=C
export LC_ALL=C
export TZ=UTC

unset SANDBOX_PHASE
unset SANDBOX_WRITE
unset SANDBOX_PREDICT
unset SANDBOX_NET
unset SANDBOX_CONFIG
unset SANDBOX_NO_COLOUR
unset SANDBOX_LOG

cwd="$(readlink -f .)"

old_umask=$(umask)
umask 0022 && touch arnold.layne && umask $old_umask
trap 'rm -f arnold.layne' EXIT

# FIXME
sydbox() {
    ../src/sydbox "$@"
}

say() {
    echo "* $@"
}

die() {
    echo "FAIL: $@" >&2
    exit 1
}