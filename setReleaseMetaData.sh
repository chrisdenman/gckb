#!/bin/bash

readonly SORTABLE_DATE_PATTERN="[[:digit:]]{8}"
readonly SEMANTIC_VERSION_PATTERN="([[:digit:]]{1,3}\.){2}[[:digit:]]{1,3}"

if [ ! -d ".git" ]; then
  echo "'./.git' directory does not exist, are you in the home directory?" >&2
  exit 1
fi

printf "Enter semantic, release version: " && read -r RELEASE_VERSION
if ! echo "$RELEASE_VERSION" | grep --quiet --extended-regex "^${SEMANTIC_VERSION_PATTERN}$"; then
  >&2 echo "'$RELEASE_VERSION' is not a valid semantic version." &&  exit 1
fi
readonly RELEASE_VERSION

DATE=$(date +"%Y%m%d") || exit 1
readonly DATE

sed --in-place --regexp-extended \
  --expression "s/badge\/gckb-${SEMANTIC_VERSION_PATTERN}/badge\/gckb-${RELEASE_VERSION}/" \
  --expression "s/badge\/released-${SORTABLE_DATE_PATTERN}/badge\/released-${DATE}/" \
  ./README.md
