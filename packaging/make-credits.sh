#!/usr/bin/env bash
#
#  Builds the CREDITS-AND-LICENCE.txt that ships with every download.
#
#      ./packaging/make-credits.sh dist/CREDITS-AND-LICENCE.txt
#
#  The licence texts are concatenated from the files in the repository rather
#  than copied by hand. MIT and OFL both require the licence to travel with
#  the binary, and the AGPL requires the same. Generating from the source is
#  what keeps the distributed file matching what is actually in the code.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/dist/CREDITS-AND-LICENCE.txt}"
VERSION="$(sed -n 's/^project(Pakku VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"

mkdir -p "$(dirname "$OUT")"

rule() { printf '%s\n' "--------------------------------------------------------------------------"; }

{
cat <<HEADER
==========================================================================

  P A K K U   $VERSION
  Multiband transient shaper

==========================================================================

  Audio Unit and VST3
  macOS 11 or later (universal) — Windows 10 or later (64-bit)

  Made in Brazil by Daniel Luiz Alves, under Kyantech Labs.

  Source, releases and issue tracker
      https://github.com/danielalves96/pakku-vst

  Pakku is free and it stays free. If it earns a place in your chain,
  you can support the next release
      https://github.com/sponsors/danielalves96

HEADER

rule
cat <<'CREDITS'

  CREDITS

    Production        Kyantech Labs
    Plugin developer  Daniel Luiz Alves

  WHAT IS IN THIS DOWNLOAD

    The installer for your system
    Pakku-Manual.pdf ....... the full user manual
    CREDITS-AND-LICENCE.txt  this file

  THIRD-PARTY COMPONENTS

    JUCE framework ......... AGPLv3 (juce.com)
    Phosphor Icons ......... MIT (phosphoricons.com)
    Michroma typeface ...... SIL Open Font License 1.1

  The full text of every licence follows. Pakku itself is released under
  the GNU Affero General Public License v3.0; the JUCE framework it is
  built on is dual-licensed, and Pakku takes the AGPLv3 route, which is
  why the whole work carries it.

CREDITS

rule
printf '\n\n  PAKKU — GNU AFFERO GENERAL PUBLIC LICENSE VERSION 3\n\n\n'
cat "$ROOT/LICENSE"

printf '\n\n'
rule
printf '\n\n  PHOSPHOR ICONS — MIT LICENSE\n\n\n'
cat "$ROOT/resources/icons/LICENSE-phosphor.txt"

printf '\n\n'
rule
printf '\n\n  MICHROMA — SIL OPEN FONT LICENSE 1.1\n\n\n'
cat "$ROOT/resources/fonts/OFL-michroma.txt"

printf '\n'
} > "$OUT"

echo "escrito: $OUT ($(wc -l < "$OUT" | tr -d ' ') linhas)"
