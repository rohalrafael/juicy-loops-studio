# Development sandbox for Juicy Loops Studio: everything needed to build, test and drive the app
# headlessly (Xvfb + xdotool), host plugins and use JACK/ALSA MIDI, without installing anything
# on the developer's machine. Ubuntu LTS: binaries built here run on that release and every newer
# one (glibc is backward compatible), which covers the developer's desktop and most Ubuntu users.
# Built and used through tools/sandbox.sh; see docs/sandbox.md.

ARG UBUNTU_VERSION=26.04
FROM ubuntu:${UBUNTU_VERSION}

ARG DEBIAN_FRONTEND=noninteractive

# Toolchain, JUCE's Linux dependencies, headless UI testing, audio/MIDI tools, debugging,
# and test plugins (LSP from the archive; Surge XT from its official .deb, pinned).
ARG SURGE_XT_VERSION=1.3.4
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build clang clang-format ccache git gh python3 \
        python3-fonttools sudo curl ca-certificates pkg-config file less \
        libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev fonts-dejavu-core \
        libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev \
        libxi-dev libxrender-dev libgl1-mesa-dev \
        xvfb xdotool imagemagick \
        jackd2 jack-example-tools alsa-utils \
        gdb \
        lsp-plugins-vst3 lsp-plugins-lv2 \
    && curl -fsSL -o /tmp/surge-xt.deb \
        "https://github.com/surge-synthesizer/releases-xt/releases/download/${SURGE_XT_VERSION}/surge-xt-linux-x64-${SURGE_XT_VERSION}.deb" \
    && apt-get install -y --no-install-recommends /tmp/surge-xt.deb \
    && rm -rf /tmp/surge-xt.deb /var/lib/apt/lists/*

# A user matching the host's, so files written to the mounted repo keep the right owner. The
# image ships a "ubuntu" user with UID 1000, the usual host UID: remove it first.
# Passwordless sudo: inside the sandbox, packages can be installed freely.
ARG USERNAME=dev
ARG UID=1000
ARG GID=1000
RUN userdel -r ubuntu 2>/dev/null || true \
    && groupadd -g "${GID}" "${USERNAME}" 2>/dev/null || true \
    && useradd -m -u "${UID}" -g "${GID}" -G audio -s /bin/bash "${USERNAME}" \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD: ALL" > "/etc/sudoers.d/${USERNAME}"

# Tell scripts they're in the sandbox and keep its build apart from the host's build/.
ENV JLS_SANDBOX=1 \
    JLS_BUILD_DIR=build-docker \
    CCACHE_DIR=/home/${USERNAME}/.cache/ccache

USER ${USERNAME}
WORKDIR /home/${USERNAME}

# Claude Code, with the official installer, into ~/.local. The home directory is a persistent
# volume (copied from here when it's first created), so Claude keeps updating itself there.
RUN curl -fsSL https://claude.ai/install.sh | bash
ENV PATH=/home/${USERNAME}/.local/bin:${PATH}
CMD ["sleep", "infinity"]
