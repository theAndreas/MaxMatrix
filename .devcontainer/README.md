# MaxMatrix development containers

VS Code offers two configurations when running **Dev Containers: Reopen in
Container**:

- **MaxMatrix (Linux)** for native Linux and remote environments. It mounts the
  host's SSH agent.
- **MaxMatrix (WSL2)** when VS Code runs in WSL2. It relies on VS Code's own
  SSH-agent forwarding and mounts nothing.

## What the container carries

| Tool | What for |
|---|---|
| `arduino-cli` with the `arduino:avr` core | compiling the library and its examples with the real AVR toolchain |
| `arduino-lint` | checking the library layout against the Arduino Library Manager rules |
| `g++`, `gdb` (from the base image) | host-side tests against a mock of `Arduino.h` |
| `cmake`, `ninja`, GoogleTest (apt) | building and running those tests: `cmake --workflow --preset default` |
| `gh` | branches and pull requests from the container |

Compile an example for an Arduino Uno:

```sh
arduino-cli compile --fqbn arduino:avr:uno --library . examples/ScrollText
```

Check the library:

```sh
arduino-lint --library-manager submit
```

**Flashing is not covered.** The container can build; getting the USB serial adapter into
it is another matter, and under WSL2 it needs `usbipd-win` on the Windows side before the
container can see anything at all. Build here, flash from the host.

## The Arduino data directory

The AVR core and its toolchain are not part of the image. The shared feature's
`postCreateCommand` installs them into `~/.arduino15`, which is the named volume
`maxmatrix-arduino15`, so a rebuild does not download them again. The `Dockerfile` creates
that path as the remote user first: an empty named volume takes its ownership from the path
it covers, and a path that does not exist in the image yields a volume owned by root that
`arduino-cli` cannot write.

## Logins that survive a rebuild

Two named volumes carry a login across a rebuild: `maxmatrix-codex` at `/home/vscode/.codex`
and `maxmatrix-gh` at `/home/vscode/.config/gh`. They are declared by the **shared
feature**, so every variant gets them. The `Dockerfile` creates both directories as the
remote user at mode 0700, because a credential is not a cache, and `gh` refuses to read a
config directory that others can enter.

Both volumes hold credentials: do not share, export or remove one unless the saved login
should be discarded.

## SSH agent on Linux

The Linux variant mounts the host's `SSH_AUTH_SOCK` at the fixed path `/tmp/ssh-agent`,
because the host socket carries a random name that would otherwise change under the
container on every host login. Private key files are never copied into the container.

Before opening the container, the host agent must be running and hold the key:

```sh
ssh-add -l
```

If no agent is running, the mount would resolve to an empty `source=` and container
creation would fail on a docker error that never mentions SSH — an `initializeCommand`
checks for the socket first and says so instead.

## GitHub CLI

`gh` is a **pinned release `.deb`**, verified against its published SHA-256, rather than
the `universe` package: Ubuntu ships 2.46, whose `gh pr edit` fails on the retired
Projects-classic `projectCards` field. `arduino-cli` and `arduino-lint` are pinned the same
way. Bumping a version means bumping its checksums in the `Dockerfile` as well:

```sh
V=$(gh api repos/cli/cli/releases/latest --jq .tag_name | tr -d v)
curl -sSL "https://github.com/cli/cli/releases/download/v$V/gh_${V}_checksums.txt" |
    grep -E 'linux_(amd64|arm64)\.deb$'
```

`gh auth login` has to run **in a terminal** — the browser and device flows both need to
prompt. `gh auth setup-git` afterwards lets `git push` over HTTPS use the same token.

`gh` is pointed at `github.com` by the shared feature, because an internal base image may
set `GH_HOST` to a company GitHub, where every call for this repository answers `HTTP 401`.
The feature's `containerEnv` is not enough with the internal Ignite image: its
`/etc/environment` carries the company host, and the VS Code server already starts with that
value, before any shell. Such a variant also needs `"remoteEnv": {"GH_HOST": "github.com"}`
in its `devcontainer.json`, which VS Code applies last.

**The host key is in the image.** The `Dockerfile` writes GitHub's published ed25519 key
into `/etc/ssh/ssh_known_hosts` for `github.com`, `ssh.github.com` and
`[ssh.github.com]:443`, so the first SSH operation in a fresh container does not fail with
`Host key verification failed`.

## Shared editor configuration

Extensions and lifecycle commands live in the local dev container feature in `shared/`,
which every configuration references:

```jsonc
"features": { "./../shared": {} }
```

Feature metadata is merged with the referencing `devcontainer.json`, so a variant can add
its own extensions on top without repeating the shared list.

Feature metadata is baked into the image at build time, so editing
`shared/devcontainer-feature.json` requires **Dev Containers: Rebuild Container**.
Reopening the folder keeps the previous metadata, which looks like a newly added extension
being ignored.

## Site-specific variants

An additional configuration directory next to `linux/` and `wsl2/` shows up as another
entry in the same picker, which is the place for settings that must not be committed
(internal base images, registry-hosted features, credentials). The `Dockerfile` takes a
`BASE_IMAGE` build argument for that purpose:

```jsonc
"build": {
    "dockerfile": "../Dockerfile",
    "context": "..",
    "args": { "BASE_IMAGE": "registry.example.internal/base:latest" }
}
```

Add such a directory to `.gitignore` to keep it local; `/.devcontainer/vector/` is already
listed there.

Behind a **TLS-intercepting proxy** the public base image cannot build: it does not trust
the proxy's certificate, so the first download in the `Dockerfile` fails with `curl` exit
code 60. A base image that carries the proxy's CA, set through `BASE_IMAGE` in such a
variant, fixes that.
