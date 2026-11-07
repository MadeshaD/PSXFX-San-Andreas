#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
task_build_dir="$(mktemp -d)"
trap 'rm -rf "$task_build_dir"' EXIT
for test_name in common_atomic native_shader native_state vehicle_materials raster_state atmosphere texture_override ui_snap; do
 g++ -std=c++17 -O2 -Wall -Wextra -Werror "tests/test_${test_name}.cpp" -o "$task_build_dir/$test_name"
 "$task_build_dir/$test_name" > "docs/${test_name}_results.txt"
 cat "docs/${test_name}_results.txt"
done

g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation tests/test_low_resolution.cpp -o "$task_build_dir/low_resolution"
"$task_build_dir/low_resolution" > docs/low_resolution_results.txt
cat docs/low_resolution_results.txt

g++ -std=c++17 -O2 -Wall -Wextra -Werror tests/test_pair_scan.cpp -o "$task_build_dir/pair_scan"
"$task_build_dir/pair_scan" > docs/pair_scan.txt
cat docs/pair_scan.txt

g++ -std=c++17 -O2 -Wall -Wextra -Werror tests/test_face_contact.cpp -o "$task_build_dir/face_contact"
"$task_build_dir/face_contact" > docs/face_contact.txt
cat docs/face_contact.txt
g++ -std=c++17 -O2 -Wall -Wextra -Werror tests/test_stitch_identity.cpp -o "$task_build_dir/stitch_identity"
"$task_build_dir/stitch_identity" > docs/stitch_identity.txt
cat docs/stitch_identity.txt
g++ -std=c++17 -O2 -Wall -Wextra -Werror tests/test_seam_visibility.cpp -o "$task_build_dir/seam_visibility"
"$task_build_dir/seam_visibility"
