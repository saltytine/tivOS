#!/usr/bin/env bash
set -x # show cmds
set -e # fail globally

# Know where we at :p
SCRIPT=$(realpath "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
cd "${SCRIPTPATH}"

TARGET_VERSION="9.2.3"
IMAGE_FILE="$HOME/Downloads/skibidi-toilet-porn.png"

fetchLimine() {
	rm -rf limine/
	git clone https://github.com/limine-bootloader/limine.git --branch=v9.x-binary --depth=1
	cd limine/
	make
	cd ../
	echo "$TARGET_VERSION" >.version
}

# Check if required image exists
if [[ ! -f "$IMAGE_FILE" ]]; then
	echo "Required image file not found at: $IMAGE_FILE"
	echo "Aborting boot process."
	exit 1
fi

# Proceed with Limine setup only if needed
if [[ ! -f ".version" ]] || ! grep -qi "$TARGET_VERSION" .version || [[ ! -f "limine/limine" ]]; then
	fetchLimine
fi
