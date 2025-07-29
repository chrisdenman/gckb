#!/bin/bash

if [ ! -d ".git" ]; then
  echo "'./.git' directory does not exist, are you in the home directory?" >&2
  exit 1
fi

printf "Enter semantic version: " && read -r VERSION
if ! echo "$VERSION" | grep --quiet --extended-regex "^([[:digit:]]{1,3}\.){2}[[:digit:]]{1,3}$"; then
  >&2 echo "'$VERSION' is nto a valid semantic version." &&  exit 1
fi
readonly VERSION

DATE=$(date +"%Y%m%d") || exit 1
readonly DATE

sed -i -E "s/badge\/gckb-([[:digit:]]{1,3}\.){2}[[:digit:]]{1,3}/badge\/gckb-$VERSION/" ./README.md
sed -i -E "s/.TH GCKB 1 [[:digit:]]{8} ([[:digit:]]{1,3}\.){2}[[:digit:]]{1,3}/.TH GCKB 1 $DATE $VERSION/" ./gckb.1
