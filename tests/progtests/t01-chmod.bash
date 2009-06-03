#!/usr/bin/env bash
# vim: set sw=4 et sts=4 tw=80 :
# Copyright 2009 Ali Polatel <polatel@gmail.com>
# Distributed under the terms of the GNU General Public License v2

. test-lib.bash

start_test "t01-chmod-deny"
sydbox -- ./t01_chmod
if [[ 0 == $? ]]; then
    die "failed to deny chmod"
fi
end_test

start_test "t01-chmod-predict"
SANDBOX_PREDICT="${cwd}" sydbox -- ./t01_chmod
if [[ 0 != $? ]]; then
    die "failed to predict chmod"
fi
perms=$(ls -l arnold.layne | cut -d' ' -f1)
if [[ "${perms}" != '-rw-r--r--' ]]; then
    die "predict allowed access"
fi
end_test

start_test "t01-chmod-write"
SANDBOX_WRITE="${cwd}" sydbox -- ./t01_chmod
if [[ 0 != $? ]]; then
    die "failed to allow chmod"
fi
perms=$(ls -l arnold.layne | cut -d' ' -f1)
if [[ "${perms}" != '----------' ]]; then
    die "write didn't allow access"
fi
end_test

# Tests dealing with too long paths
perm_toolong() {
    local fname perl perms

    # bash fails to do it so use perl instead...
    fname="$1"
    perl="$(find_perl_or_skip)"
    "$perl" \
        -e "use Fcntl ':mode';" \
        -e 'my $dir = '$long_dir';' \
        -e 'foreach my $i (1..64) {' \
        -e '    chdir($dir) or die "$!"' \
        -e '}' \
        -e '($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,' \
        -e '    $atime,$mtime,$ctime,$blksize,$blocks) = stat("'$tmpfile'") or die "$!";' \
        -e 'printf(S_IMODE($mode));'
}

tmpfile="$(mkstemp_long)"

start_test "t01-chmod-deny-toolong"
sydbox -- ./t01_chmod_toolong "$long_dir" "$tmpfile"
if [[ 0 == $? ]]; then
    die "failed to deny chmod"
fi
end_test

start_test "t01-chmod-predict-toolong"
SANDBOX_PREDICT="$cwd"/$long_dir sydbox -- ./t01_chmod_toolong "$long_dir" "$tmpfile"
if [[ 0 != $? ]]; then
    die "failed to predict chmod"
fi
perms=$(perm_toolong "$tmpfile")
if [[ -z "$perms" ]]; then
    say skip "failed to get permissions of the file, skipping test"
    exit 0
elif [[ "$perms" == 0 ]]; then
    die "predict allowed access"
fi
end_test

start_test "t01-chmod-allow-toolong"
SANDBOX_WRITE="$cwd"/$long_dir sydbox -- ./t01_chmod_toolong "$long_dir" "$tmpfile"
if [[ 0 != $? ]]; then
    die "failed to allow chmod"
fi
perms=$(perm_toolong "$tmpfile")
if [[ -z "$perms" ]]; then
    say skip "failed to get permissions of the file, skipping test"
    exit 0
elif [[ "$perms" != 0 ]]; then
    die "write didn't allow access"
fi
end_test
