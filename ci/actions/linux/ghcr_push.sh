#!/bin/bash
set -e

scripts="$PWD/ci"
if [[ "$GITHUB_WORKFLOW" = "Develop" ]]; then
    "$scripts"/custom-timeout.sh 30 docker push "ghcr.io/${GITHUB_REPOSITORY}/rsnano-env:base"
    "$scripts"/custom-timeout.sh 30 docker push "ghcr.io/${GITHUB_REPOSITORY}/rsnano-env:gcc"
    "$scripts"/custom-timeout.sh 30 docker push "ghcr.io/${GITHUB_REPOSITORY}/rsnano-env:clang"
else
    tags=$(docker images --format '{{.Repository}}:{{.Tag }}' | grep "ghcr.io" | grep -vE "env|none")
    for a in $tags; do
        "$scripts"/custom-timeout.sh 30 docker push "$a"
    done
fi
