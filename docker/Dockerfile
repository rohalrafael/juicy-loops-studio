# Development sandbox for Juicy Loops Studio: everything needed to build, test and drive the app
# headlessly (Xvfb + xdotool), host plugins and use JACK/ALSA MIDI, without installing anything
# on the developer's machine. Arch Linux, to match the main developer's desktop.
# Built and used through tools/sandbox.sh; see docs/sandbox.md.

FROM archlinux:latest

# Toolchain, JUCE's Linux dependencies, headless UI testing, audio/MIDI tools, debugging,
# and test plugins.
RUN pacman -Syu --noconfirm --needed \
        base-devel cmake ninja clang ccache git github-cli python python-fonttools sudo which \
        alsa-lib freetype2 fontconfig libx11 libxext libxrandr libxinerama libxcursor \
        libxcomposite mesa \
        xorg-server-xvfb xdotool imagemagick \
        jack2 jack-example-tools alsa-utils \
        gdb \
        lsp-plugins-vst3 lsp-plugins-lv2 surge-xt \
    && pacman -Scc --noconfirm

# A user matching the host's, so files written to the mounted repo keep the right owner.
# Passwordless sudo: inside the sandbox, packages can be installed freely.
ARG USERNAME=dev
ARG UID=1000
ARG GID=1000
RUN groupadd -g "${GID}" "${USERNAME}" 2>/dev/null || true \
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
