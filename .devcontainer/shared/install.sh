#!/bin/sh
# The editor metadata in devcontainer-feature.json is the whole of this feature; the
# toolchain itself is installed by ../Dockerfile, and the Arduino AVR core by the
# postCreateCommand, because it lives in a volume rather than in the image.
set -e

echo "MaxMatrix shared configuration: nothing to install."
