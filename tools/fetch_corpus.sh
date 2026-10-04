#!/usr/bin/env bash
# Fetch the ADM corpus (the EBU's ADM test materials, redistributed under
# CC BY 4.0 at https://github.com/tsln-lab/adm-test-corpus) into a
# directory, verifying each file's SHA-256 against source/corpus/assets.txt.
# Files already present with the right checksum are kept.
#
#   tools/fetch_corpus.sh <directory>
#   EARMAX_ADM_CORPUS=<directory> ctest --test-dir build -R adm_corpus
set -eu
dir="${1:?usage: fetch_corpus.sh <directory>}"
release="https://github.com/tsln-lab/adm-test-corpus/releases/download/v1.0.0"
here="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$dir"
if command -v sha256sum >/dev/null; then sum='sha256sum'; else sum='shasum -a 256'; fi
while read -r name digest; do
    case "$name" in ''|'#'*) continue ;; esac
    file="$dir/$name.wav"
    if [ -f "$file" ] && [ "$($sum "$file" | cut -d' ' -f1)" = "$digest" ]; then
        echo "kept    $name.wav"
        continue
    fi
    echo "fetching $name.wav"
    curl -sSL --fail --retry 3 -o "$file" "$release/$name"
    actual="$($sum "$file" | cut -d' ' -f1)"
    if [ "$actual" != "$digest" ]; then
        echo "checksum mismatch for $name: $actual" >&2
        rm -f "$file"
        exit 1
    fi
done < "$here/source/corpus/assets.txt"
echo "corpus ready in $dir"
