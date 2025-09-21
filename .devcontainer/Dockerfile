FROM espressif/idf:latest

# Install only once at build time (faster opens)
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      udev usbutils coreutils python3 clangd && \
    rm -rf /var/lib/apt/lists/*

# Ensure udevadm is on PATH in case /sbin isn’t used
RUN if ! command -v udevadm >/dev/null 2>&1 && [ -x /usr/bin/udevadm ]; then \
      ln -sf /usr/bin/udevadm /sbin/udevadm ; \
    fi

# Make new interactive shells source ESPTOOL_PORT if present
RUN set -eux; \
    for rc in /etc/bash.bashrc /root/.bashrc; do \
      [ -f "$rc" ] && (grep -q 'esptool_port.sh' "$rc" || \
      echo 'source /etc/profile.d/esptool_port.sh 2>/dev/null || true' >> "$rc"); \
    done

# If you plan to use a non-root user later, add to dialout here (optional)
# RUN usermod -a -G dialout vscode || true
