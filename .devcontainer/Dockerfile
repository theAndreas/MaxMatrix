# Base image, overridable via build.args in a devcontainer.json variant so that
# site-specific base images can be used without touching this file.
#
# The tag has no hyphen. devcontainers/cpp publishes ubuntu-24.04 and ubuntu24.04 both,
# but from 26.04 only the unhyphenated form exists, so the older spelling looks like the
# newest release when it is not. The base image carries build-essential and gdb, which is
# what the host-side tests against an Arduino mock build with.
ARG BASE_IMAGE=mcr.microsoft.com/devcontainers/cpp:ubuntu26.04
FROM ${BASE_IMAGE}

# arduino-cli compiles the library and its examples with the real Arduino AVR core, which
# is the one check the host-side tests cannot replace: they build against a mock of
# Arduino.h, not against avr-gcc. Not gcc-avr from apt - the AVR core brings its own
# toolchain, and that one is what the Arduino IDE compiles a user's sketch with.
# A pinned release tarball with its published checksum, for the same reason as gh below:
# the trust stays anchored to something visible in this file, at the cost of bumping the
# version by hand.
ARG ARDUINO_CLI_VERSION=1.5.1
ARG ARDUINO_CLI_SHA256_AMD64=28a8e119c498a25607821c36cb2dc49e8463941b261a0d99091baa7bc692dd2b
ARG ARDUINO_CLI_SHA256_ARM64=1e69e077479f300614d4551334e0a33f08ee40b04315d83b8e7e0e94f0d0ee62
RUN ARCH="$(dpkg --print-architecture)" && \
    case "${ARCH}" in \
        amd64) ASSET=64bit; SHA256="${ARDUINO_CLI_SHA256_AMD64}" ;; \
        arm64) ASSET=ARM64; SHA256="${ARDUINO_CLI_SHA256_ARM64}" ;; \
        *) echo "arduino-cli: no pinned checksum for ${ARCH}; add one or drop arduino-cli" >&2; exit 1 ;; \
    esac && \
    curl -fsSL -o /tmp/arduino-cli.tar.gz \
        "https://github.com/arduino/arduino-cli/releases/download/v${ARDUINO_CLI_VERSION}/arduino-cli_${ARDUINO_CLI_VERSION}_Linux_${ASSET}.tar.gz" && \
    echo "${SHA256}  /tmp/arduino-cli.tar.gz" | sha256sum -c - && \
    tar -xzf /tmp/arduino-cli.tar.gz -C /usr/local/bin arduino-cli && \
    rm /tmp/arduino-cli.tar.gz

# arduino-lint checks library.properties, keywords.txt and the folder layout against the
# rules the Arduino Library Manager applies, so a broken library is found here rather than
# when the registry rejects it. Pinned the same way as arduino-cli.
ARG ARDUINO_LINT_VERSION=1.3.0
ARG ARDUINO_LINT_SHA256_AMD64=181671ca174988f2601e1cdf2b40a552682db8eaa047de33117e67e70338b63e
ARG ARDUINO_LINT_SHA256_ARM64=d886fd734762f43b90990ff0ec12d65e088f74516db1a1a9c3c2de6b7fffdada
RUN ARCH="$(dpkg --print-architecture)" && \
    case "${ARCH}" in \
        amd64) ASSET=64bit; SHA256="${ARDUINO_LINT_SHA256_AMD64}" ;; \
        arm64) ASSET=ARM64; SHA256="${ARDUINO_LINT_SHA256_ARM64}" ;; \
        *) echo "arduino-lint: no pinned checksum for ${ARCH}; add one or drop arduino-lint" >&2; exit 1 ;; \
    esac && \
    curl -fsSL -o /tmp/arduino-lint.tar.gz \
        "https://github.com/arduino/arduino-lint/releases/download/${ARDUINO_LINT_VERSION}/arduino-lint_${ARDUINO_LINT_VERSION}_Linux_${ASSET}.tar.gz" && \
    echo "${SHA256}  /tmp/arduino-lint.tar.gz" | sha256sum -c - && \
    tar -xzf /tmp/arduino-lint.tar.gz -C /usr/local/bin arduino-lint && \
    rm /tmp/arduino-lint.tar.gz

# gh turns branch and pull request housekeeping into something doable from the container
# rather than by hand in a browser.
# A pinned release .deb, not the universe package - that one is 2.46, whose `gh pr edit`
# still asks for the retired Projects-classic `projectCards` field and therefore fails
# outright. dpkg rather than apt because the package Depends on git alone, which the base
# image carries - so this needs no package lists.
ARG GH_VERSION=2.97.0
ARG GH_SHA256_AMD64=7c7fa3bb890db0934baf65910d97b8c0fa437b2e590f7f7daf6bdf82c5c486d7
ARG GH_SHA256_ARM64=0ba7a76739c865d82ebde24667d875d9b8caa55db47c7597c24accdd4defd2bb
RUN ARCH="$(dpkg --print-architecture)" && \
    case "${ARCH}" in \
        amd64) SHA256="${GH_SHA256_AMD64}" ;; \
        arm64) SHA256="${GH_SHA256_ARM64}" ;; \
        *) echo "gh: no pinned checksum for ${ARCH}; add one or drop gh" >&2; exit 1 ;; \
    esac && \
    curl -fsSL -o /tmp/gh.deb \
        "https://github.com/cli/cli/releases/download/v${GH_VERSION}/gh_${GH_VERSION}_linux_${ARCH}.deb" && \
    echo "${SHA256}  /tmp/gh.deb" | sha256sum -c - && \
    dpkg -i /tmp/gh.deb && \
    rm /tmp/gh.deb

# The Arduino data directory is a named volume: the AVR core and its toolchain are
# downloaded on first use, and the volume is what keeps a rebuild from fetching them again.
# An empty named volume takes its ownership from the path it covers, so the path has to
# exist and belong to the remote user first, or arduino-cli cannot write into it.
RUN install -d -o vscode -g vscode -m 0755 /home/vscode/.arduino15

# The same arrangement for the two logins a rebuild would otherwise throw away: Codex, whose
# extension the shared feature installs, and gh, which every push and every pull request
# goes through. Mode 0700 rather than 0755, because what these hold is a credential and not
# a cache - and gh refuses to read a config directory others can enter.
RUN install -d -o vscode -g vscode -m 0700 /home/vscode/.codex && \
    install -d -o vscode -g vscode -m 0700 /home/vscode/.config/gh

# GitHub's SSH host key, so that the first git operation in a fresh container verifies
# against something rather than failing with "Host key verification failed", because
# ~/.ssh belongs to the container and is empty after every rebuild.
#
# Written out rather than fetched with ssh-keyscan at build time: a scan believes whatever
# answers it, which is precisely what a known host key is there to prevent. The fingerprint
# below is the one GitHub publishes under "GitHub's SSH key fingerprints", and
# `ssh-keygen -lf /etc/ssh/ssh_known_hosts` prints it back out of the file.
#
# Three names for one key: github.com, ssh.github.com for SSH on port 443 in networks that
# block 22, and the bracketed form, because an entry is keyed by host *and* port.
ARG GITHUB_HOST_KEY="ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIOMqqnkVzrm0SdG6UOoqKLsabgH5C9okWi0dh2l9GKJl"
RUN printf '# SHA256:+DiY3wvvV6TuJJhbpZisF/zLDA0zPMSvHdkr4UvCOqU\n%s %s\n%s %s\n%s %s\n' \
        'github.com' "${GITHUB_HOST_KEY}" \
        'ssh.github.com' "${GITHUB_HOST_KEY}" \
        '[ssh.github.com]:443' "${GITHUB_HOST_KEY}" \
        >> /etc/ssh/ssh_known_hosts && \
    chmod 0644 /etc/ssh/ssh_known_hosts

ENV TZ=Europe/Berlin

# Extensions, editor settings and lifecycle commands live in the local feature
# in shared/, which every devcontainer.json variant references. A
# devcontainer.metadata label set here would not work: clients read image
# metadata from the image the final FROM points at, then overwrite the label on
# the image they build from this file.
