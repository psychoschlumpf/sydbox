#!/usr/bin/env bash
# vim: set sw=4 et sts=4 tw=80 :
# Copyright 2009 Ali Polatel <polatel@gmail.com>
# Distributed under the terms of the GNU General Public License v2

. test-lib.bash

clean_files+=( "lucifer.sam" )

start_test "t22-renameat-second-atfdcwd-deny"
SANDBOX_WRITE="${cwd}/see.emily.play" sydbox -- ./t22_renameat_second_atfdcwd
if [[ 0 == $? ]]; then
    die "failed to deny rename"
elif [[ -f lucifer.sam ]]; then
    die "file exists, failed to deny rename"
fi
end_test

start_test "t22-renameat-second-atfdcwd-predict"
SANDBOX_WRITE="${cwd}/see.emily.play" SANDBOX_PREDICT="${cwd}" sydbox -- ./t22_renameat_second_atfdcwd
if [[ 0 != $? ]]; then
    die "failed to predict rename"
elif [[ -f lucifer.sam ]]; then
    die "predict allowed access"
fi
end_test

start_test "t22-renameat-second-atfdcwd-write"
SANDBOX_WRITE="${cwd}" sydbox -- ./t22_renameat_second_atfdcwd
if [[ 0 != $? ]]; then
    die "failed to allow renameat"
elif [[ ! -f lucifer.sam ]]; then
    die "file doesn't exist, failed to allow renameat"
fi
end_test
