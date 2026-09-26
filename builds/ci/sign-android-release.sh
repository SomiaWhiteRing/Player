#!/usr/bin/env bash
# Sign outside Gradle and outside the persistent build caches.
set -euo pipefail

: "${ANDROID_RELEASE_KEYSTORE_BASE64:?Missing Android release keystore}"
: "${ANDROID_RELEASE_STORE_PASSWORD:?Missing Android release store password}"
: "${ANDROID_RELEASE_KEY_ALIAS:?Missing Android release key alias}"
: "${ANDROID_RELEASE_KEY_PASSWORD:?Missing Android release key password}"
: "${ANDROID_SDK_ROOT:?Missing Android SDK}"

unsigned_apk="${1:?Missing unsigned APK path}"
signed_apk="${2:?Missing signed APK path}"
build_tools="$ANDROID_SDK_ROOT/build-tools/36.0.0"

badging="$("$build_tools/aapt2" dump badging "$unsigned_apk")"
if grep -q '^application-debuggable' <<< "$badging"; then
	echo "Refusing to sign a debuggable APK." >&2
	exit 1
fi

umask 077
signing_dir="$(mktemp -d)"
trap 'rm -rf -- "$signing_dir"' EXIT
printf '%s' "$ANDROID_RELEASE_KEYSTORE_BASE64" | base64 --decode > "$signing_dir/release.jks"
unset ANDROID_RELEASE_KEYSTORE_BASE64

"$build_tools/zipalign" -c -P 16 4 "$unsigned_apk"
"$build_tools/apksigner" sign \
	--ks "$signing_dir/release.jks" \
	--ks-key-alias "$ANDROID_RELEASE_KEY_ALIAS" \
	--ks-pass env:ANDROID_RELEASE_STORE_PASSWORD \
	--key-pass env:ANDROID_RELEASE_KEY_PASSWORD \
	--out "$signing_dir/signed.apk" \
	"$unsigned_apk"
"$build_tools/apksigner" verify --verbose --print-certs "$signing_dir/signed.apk"
cp "$signing_dir/signed.apk" "$signed_apk"
