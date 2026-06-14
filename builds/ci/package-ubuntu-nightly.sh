#!/usr/bin/env bash
set -euo pipefail

WEB_ASSET_NAME="${WEB_ASSET_NAME:-EasyRPG-Player-Kai-nightly-web.zip}"
ANDROID_ASSET_NAME="${ANDROID_ASSET_NAME:-EasyRPG-Player-Kai-nightly-android-debug.apk}"
ANDROID_ABIS="${ANDROID_ABIS:-armeabi-v7a,arm64-v8a,x86,x86_64}"
ANDROID_NDK_VERSION="${ANDROID_NDK_VERSION:-28.2.13676358}"
BUILDSCRIPTS_REF="${BUILDSCRIPTS_REF:-e32f89b02864dbbca81d317575839668f18e7f57}"
ARTIFACT_DIR="${ARTIFACT_DIR:-build/artifacts}"

log() {
	printf '\n==> %s\n' "$*"
}

usage() {
	cat <<'EOF'
Usage: bash ./builds/ci/package-ubuntu-nightly.sh [web|android|all]

Builds the Ubuntu-hosted nightly artifacts with the same entrypoint used by
GitHub Actions and local Docker Desktop packaging.

Windows Player.exe is built by builds/ci/package-windows-nightly.ps1 on the
windows-2022 runner, because that artifact depends on MSVC and the EasyRPG
Windows vcpkg toolchain.
EOF
}

ensure_buildscripts_clone() {
	local buildscripts_dir="$1"
	local clone_mode="$2"

	mkdir -p "$(dirname "$buildscripts_dir")"
	if [ ! -d "$buildscripts_dir/.git" ]; then
		rm -rf "$buildscripts_dir"
		if [ "$clone_mode" = "shallow" ]; then
			git clone --depth 1 https://github.com/EasyRPG/buildscripts.git "$buildscripts_dir"
		else
			git clone https://github.com/EasyRPG/buildscripts.git "$buildscripts_dir"
		fi
	fi
}

package_web() {
	local build_dir="${BUILD_DIR:-build/emscripten-release}"
	local buildscripts_dir="${WEB_BUILDSCRIPTS_DIR:-${BUILDSCRIPTS_DIR:-$PWD/external/local-docker/web/buildscripts}}"
	local emscripten_work_dir="${EMSCRIPTEN_WORK_DIR:-$buildscripts_dir/emscripten}"
	local package_dir="${PACKAGE_DIR:-build/package/nightly-web}"

	log "Cloning EasyRPG buildscripts for Web"
	ensure_buildscripts_clone "$buildscripts_dir" shallow

	log "Preparing EasyRPG Emscripten toolchain"
	(
		cd "$emscripten_work_dir"
		if [ ! -x emsdk-portable/upstream/emscripten/emcc ] || [ ! -f lib/cmake/liblcf/liblcf-config.cmake ]; then
			BUILD_LIBLCF=1 bash ./0_build_everything.sh
		fi
	)

	log "Configuring Web build"
	(
		# shellcheck disable=SC1091
		source "$emscripten_work_dir/emsdk-portable/emsdk_env.sh"
		local version_suffix="(nightly, $(date -u +%Y-%m-%d))"
		cmake -S . -B "$build_dir" -G Ninja \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_TOOLCHAIN_FILE="$emscripten_work_dir/emsdk-portable/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" \
			-DPLAYER_PREFIX_PATH_APPEND="$emscripten_work_dir" \
			-DPLAYER_FIND_ROOT_PATH_APPEND=ON \
			-DPLAYER_JS_BUILD_SHELL=ON \
			-DPLAYER_ENABLE_TESTS=OFF \
			-DPLAYER_TARGET_PLATFORM=SDL3 \
			-DPLAYER_VERSION_APPEND="$version_suffix"

		log "Building Web"
		cmake --build "$build_dir" --parallel
	)

	log "Verifying Web outputs"
	for file in easyrpg-player.html easyrpg-player.js easyrpg-player.wasm; do
		test -s "$build_dir/$file" || {
			echo "Missing build output: $build_dir/$file"
			exit 1
		}
	done
	test -s "$build_dir/easyrpg-player.data" || {
		echo "Missing build output: $build_dir/easyrpg-player.data"
		exit 1
	}
	grep -q "createEasyRpgPlayer" "$build_dir/easyrpg-player.html"

	log "Packaging Web artifact"
	BUILD_DIR="$build_dir" \
		ARTIFACT_DIR="$ARTIFACT_DIR" \
		PACKAGE_DIR="$package_dir" \
		ASSET_NAME="$WEB_ASSET_NAME" \
		bash ./builds/package-web.sh

	log "Web artifact: $ARTIFACT_DIR/$WEB_ASSET_NAME"
	if [ -n "${GITHUB_OUTPUT:-}" ]; then
		echo "artifact_path=$ARTIFACT_DIR/$WEB_ASSET_NAME" >> "$GITHUB_OUTPUT"
	fi
}

run_sdkmanager() {
	set +o pipefail
	yes | android-sdk/cmdline-tools/latest/bin/sdkmanager --sdk_root="$ANDROID_SDK_ROOT" "$@"
	local sdkmanager_status=${PIPESTATUS[1]}
	set -o pipefail
	return "$sdkmanager_status"
}

is_android_toolchain_complete() {
	local abi="$1"
	[ -f "$abi-toolchain/lib/cmake/liblcf/liblcf-config.cmake" ]
}

inspect_android_packaging_inputs() {
	cd builds/android
	echo "APK outputs:"
	find app/build/outputs/apk -maxdepth 4 -type f -print -exec ls -lh {} \; 2>/dev/null || true
	echo "Compressed assets:"
	find app/build/intermediates/compressed_assets -type f -name '*recommended*' -print -exec unzip -lv {} \; 2>/dev/null || true
	echo "Native libraries:"
	find app/build/intermediates -path '*native_libs*' -type f \( -name '*.so' -o -name '*.json' \) -print -exec ls -lh {} \; 2>/dev/null || true
}

package_android() {
	local buildscripts_dir="${ANDROID_BUILDSCRIPTS_DIR:-${BUILDSCRIPTS_DIR:-$PWD/external/local-docker/android/buildscripts}}"
	local android_work_dir="${ANDROID_WORK_DIR:-$buildscripts_dir/android}"
	local version_code="${VERSION_CODE_OVERRIDE:-$((9778000 + ${GITHUB_RUN_NUMBER:-0}))}"

	export ANDROID_WORK_DIR="$android_work_dir"
	export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$android_work_dir/android-sdk}"
	export ANDROID_HOME="${ANDROID_HOME:-$android_work_dir/android-sdk}"

	log "Cloning EasyRPG buildscripts for Android"
	ensure_buildscripts_clone "$buildscripts_dir" full
	git -C "$buildscripts_dir" fetch --depth 1 origin "$BUILDSCRIPTS_REF"
	git -C "$buildscripts_dir" checkout --force FETCH_HEAD
	sed -i \
		-e '/extras;android;m2repository/d' \
		-e '/extras;google;m2repository/d' \
		"$android_work_dir/1_download_library.sh"
	sed -i \
		-e "s/ndk;[0-9.][0-9.]*/ndk;$ANDROID_NDK_VERSION/g" \
		-e "s#ndk/[0-9.][0-9.]*#ndk/$ANDROID_NDK_VERSION#g" \
		"$android_work_dir/1_download_library.sh" \
		"$android_work_dir/2_build_toolchain.sh"

	log "Preparing EasyRPG Android toolchain"
	(
		cd "$android_work_dir"

		local needs_toolchain=0
		local abi
		for abi in armeabi-v7a arm64-v8a x86 x86_64; do
			if ! is_android_toolchain_complete "$abi"; then
				echo "Android dependency toolchain is missing required files for $abi; rebuilding."
				needs_toolchain=1
			fi
		done

		if [ ! -x android-sdk/cmdline-tools/latest/bin/sdkmanager ] || [ "$needs_toolchain" -eq 1 ]; then
			rm -rf android-sdk \
				armeabi-v7a-toolchain \
				arm64-v8a-toolchain \
				x86-toolchain \
				x86_64-toolchain
			BUILD_LIBLCF=1 bash ./1_download_library.sh
			needs_toolchain=1
		fi

		run_sdkmanager --licenses >/dev/null || true
		run_sdkmanager \
			"platforms;android-36" \
			"build-tools;36.0.0"

		if [ "$needs_toolchain" -eq 1 ]; then
			BUILD_LIBLCF=1 bash ./2_build_toolchain.sh
			bash ./3_cleanup.sh
		fi

		for abi in armeabi-v7a arm64-v8a x86 x86_64; do
			if ! is_android_toolchain_complete "$abi"; then
				echo "Android dependency toolchain is incomplete for $abi after build."
				find "$abi-toolchain" -maxdepth 4 -type f -name 'liblcf-config.cmake' -print || true
				exit 1
			fi
		done
	)

	log "Building Android APK"
	(
		cd builds/android
		chmod +x ./gradlew
		./gradlew \
			-PtoolchainDirs="$android_work_dir" \
			-PABI_FILTERS_DEBUG="$ANDROID_ABIS" \
			-PVERSION_CODE_OVERRIDE="$version_code" \
			-PcmakeOptions="-DPLAYER_TARGET_PLATFORM=SDL3" \
			assembleDebug \
			--stacktrace
	)

	log "Inspecting Android packaging inputs"
	(inspect_android_packaging_inputs)

	log "Staging Android artifact"
	local artifact_dir="$ARTIFACT_DIR"
	local asset_path="$artifact_dir/$ANDROID_ASSET_NAME"
	local apk_path="builds/android/app/build/outputs/apk/debug/app-debug.apk"

	test -s "$apk_path" || {
		echo "Missing build output: $apk_path"
		exit 1
	}
	local abi
	for abi in armeabi-v7a arm64-v8a x86 x86_64; do
		unzip -l "$apk_path" "lib/$abi/libeasyrpg_android.so" | grep -q "lib/$abi/libeasyrpg_android.so" || {
			echo "Missing Android native library for $abi in $apk_path"
			exit 1
		}
	done
	unzip -l "$apk_path" "assets/builtin/recommended.sf2" | grep -q "assets/builtin/recommended.sf2" || {
		echo "Missing built-in SoundFont asset in $apk_path"
		exit 1
	}

	mkdir -p "$artifact_dir"
	cp "$apk_path" "$asset_path"
	log "Android artifact: $asset_path"
	if [ -n "${GITHUB_OUTPUT:-}" ]; then
		echo "artifact_path=$asset_path" >> "$GITHUB_OUTPUT"
	fi
}

main() {
	local target="${1:-all}"

	case "$target" in
		web)
			package_web
			;;
		android)
			package_android
			;;
		all)
			package_web
			package_android
			;;
		-h|--help|help)
			usage
			;;
		*)
			usage >&2
			exit 2
			;;
	esac
}

main "$@"
