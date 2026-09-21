# Setup file for Kilo Code AI to enable build of guardrail code
# Source this file
current_dir=$(pwd)
echo "Current dir = ${current_dir}"

cd /exp/dune/app/users/esnider/code-spack/larsoft3/mpdtest
source spack/setup-env.sh
spack mpd select mpddev
spack env activate mpddev/local
cd ${current_dir}
