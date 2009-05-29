#!/usr/bin/env bash
# vim: set sw=4 et sts=4 tw=80 :
# Copyright 2009 Ali Polatel <polatel@gmail.com>
# Distributed under the terms of the GNU General Public License v2

. test-lib.bash

start_test "t24-linkat-first-atfdcwd-deny"
sydbox -- ./t24_linkat_first_atfdcwd
if [[ 0 == $? ]]; then
    die "failed to deny linkat"
elif [[ -f its.not.the.same ]]; then
    die "file exists, failed to deny linkat"
fi
end_test

start_test "t24-linkat-first-atfdcwd-predict"
SANDBOX_PREDICT="${cwd}" sydbox -- ./t24_linkat_first_atfdcwd
if [[ 0 != $? ]]; then
    die "failed to predict linkat"
elif [[ -f its.not.the.same ]]; then
    die "predict allowed access"
fi
end_test

start_test "t24-linkat-first-atfdcwd-write"
SANDBOX_WRITE="${cwd}" sydbox -- ./t24_linkat_first_atfdcwd
if [[ 0 != $? ]]; then
    die "failed to allow linkat"
elif [[ ! -f its.not.the.same ]]; then
    die "file doesn't exist, failed to allow linkat"
fi
end_test