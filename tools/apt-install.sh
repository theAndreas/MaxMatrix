#!/bin/bash
# Installs Debian packages on a CI runner, so that a mirror that trickles cannot hold a job up to its time limit.
#
#   tools/apt-install.sh PACKAGE...
#
# The runner's mirror delivers 20 MB/s as a rule, but now and then one connection to it delivers 35 kB/s for minutes
# without ever stalling: 18 MB took eight minutes in the simavr job of #53, and the host tests of #52 ran into their
# limit of ten. apt's own timeout waits only for a connection that says nothing, and its retries only follow an error,
# so neither ends such a download. Here every download gets a limit of its own and is started again on new
# connections, where apt goes on with the files it has already fetched. Only once every package is there are they
# installed, without a limit, as dpkg stopped halfway would leave the runner's packages half set up.
set -eu

Attempts=3
# the largest download of the workflows, 34 MB, takes two seconds at the usual speed
Seconds=120

# Runs apt-get with ARGUMENTS under the limit, and once more for every attempt that ran past it or failed.
fetch() {
    for Attempt in $(seq 1 "$Attempts"); do
        sudo timeout "$Seconds" apt-get "$@" && return 0
        echo "::warning::apt-get $1 took longer than $Seconds s or failed, attempt $Attempt of $Attempts"
    done
    return 1
}

fetch update
fetch install -y --no-install-recommends --download-only "$@"
sudo apt-get install -y --no-install-recommends "$@"
