#!/usr/bin/env bash
# Fetch the ADM corpus (the EBU's ADM test materials and excerpts of Netflix's
# Dolby Atmos masters, redistributed under CC BY 4.0 at
# https://github.com/tsln-lab/adm-test-corpus) into a directory, verifying
# each file's SHA-256 against source/corpus/assets.txt, which names the
# release each file comes from. Files already present with the right
# checksum are kept.
#
#   tools/fetch_corpus.sh <directory>
#   EARMAX_ADM_CORPUS=<directory> ctest --test-dir build -R adm_corpus
set -eu
dir="${1:?usage: fetch_corpus.sh <directory>}"
releases="https://github.com/tsln-lab/adm-test-corpus/releases/download"
release=""
here="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$dir"
if command -v sha256sum >/dev/null; then sum='sha256sum'; else sum='shasum -a 256'; fi
while read -r name digest; do
    case "$name" in
        ''|'#'*) continue ;;
        release) release="$releases/$digest"; continue ;;
    esac
    if [ -z "$release" ]; then
        echo "assets.txt: a 'release <tag>' line must precede $name" >&2
        exit 1
    fi
    file="$dir/${name%.wav}.wav"    # EBU assets have no extension, the Netflix ones do
    if [ -f "$file" ] && [ "$($sum "$file" | cut -d' ' -f1)" = "$digest" ]; then
        echo "kept    $(basename "$file")"
        continue
    fi
    echo "fetching $(basename "$file")"
    curl -sSL --fail --retry 3 -o "$file" "$release/$name"
    actual="$($sum "$file" | cut -d' ' -f1)"
    if [ "$actual" != "$digest" ]; then
        echo "checksum mismatch for $name: $actual" >&2
        rm -f "$file"
        exit 1
    fi
done < "$here/source/corpus/assets.txt"
echo "corpus ready in $dir"
