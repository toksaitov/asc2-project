# The machine of the official precheck, as the GitHub action sets it up:
# Python 3.11 with the packages of the precheck, and KLayout and Magic from
# Nix in the versions that the precheck's own `default.nix` names. The build
# context is this file next to a copy of `precheck/` of tt-support-tools.
FROM nixos/nix:latest AS nix

FROM python:3.11-bookworm
COPY --from=nix /nix /nix
ENV PATH=/nix/var/nix/profiles/default/bin:$PATH NIX_SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt
RUN mkdir -p /etc/nix && printf 'build-users-group =\nsandbox = false\n' > /etc/nix/nix.conf

# One package of the precheck has no ready build for ARM and is built here
RUN apt-get update && apt-get install -y --no-install-recommends cmake libqhull-dev zlib1g-dev && rm -rf /var/lib/apt/lists/*

COPY precheck /tt/precheck
RUN pip install --no-cache-dir -r /tt/precheck/requirements.txt
RUN cd /tt/precheck && nix-build
