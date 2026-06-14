FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
		autoconf \
		automake \
		bash \
		build-essential \
		ca-certificates \
		ccache \
		cmake \
		curl \
		git \
		libtool \
		make \
		meson \
		ninja-build \
		openjdk-17-jdk-headless \
		patch \
		perl \
		pkg-config \
		python3 \
		ruby \
		unzip \
		zip \
	&& rm -rf /var/lib/apt/lists/*

ENV JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64

WORKDIR /workspace
