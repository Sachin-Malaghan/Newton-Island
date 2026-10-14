#!/usr/bin/env bash
# Registers one level file in island.json, verifies it with the solver harness, and (only if every check
# passes) commits and pushes it on its own.
#   bash Tools/SimHarness/add_level.sh w1_07_long_jump.json newton.w1.07 "Long Jump"
set -e
cd "$(dirname "$0")/../.."
FILE="$1"; ID="$2"; NAME="$3"
ISLAND=Content/Islands/Newton/island.json
WORLD="${FILE:0:2}"   # w1, w3, ...
if ! grep -q "levels/$FILE" "$ISLAND"; then
  # Append to the levels list of the world whose first level starts with the same prefix.
  perl -0pi -e "s|(\"levels/${WORLD}_01_[^\"]*\"(?:, \"levels/[^\"]*\")*)( \])|\$1, \"levels/$FILE\"\$2|" "$ISLAND"
fi
grep -q "levels/$FILE" "$ISLAND" || { echo "could not register $FILE"; exit 1; }
OUT=$(./Intermediate/SimHarness/cisim_harness.exe -root Content/Islands -level "$ID" 2>&1) || { echo "$OUT"; echo "LEVEL $ID FAILED - not committed"; exit 1; }
echo "$OUT" | grep -E "Solvable|setups win|NotTrivial|untouched|wins with|example setup|slot" | head -8
git add "$ISLAND" "Content/Islands/Newton/levels/$FILE"
git commit -q -m "Add level $ID: $NAME

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push -q 2>&1 | tail -1
echo "committed and pushed $ID"
