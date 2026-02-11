# Docker image for running clang-format on C++ source files
FROM ubuntu:22.04

# Install clang-format and necessary tools
RUN apt-get update && apt-get install -y \
    clang-format \
    bash \
    findutils \
    diffutils \
    && rm -rf /var/lib/apt/lists/*

# Verify clang-format is installed
RUN which clang-format && clang-format --version

# Set working directory
WORKDIR /workspace

# Copy the format script
COPY format.sh /usr/local/bin/format.sh
RUN chmod +x /usr/local/bin/format.sh

# Set the entrypoint
ENTRYPOINT ["/usr/local/bin/format.sh"]
