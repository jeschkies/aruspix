#!/usr/bin/env bash
# Fetch, patch, and build IM 3.15 (https://www.tecgraf.puc-rio.br/im/) into
# third_party/im so the root CMakeLists.txt can find libim, libim_process,
# and libim_fftw3.
#
# IM ships its own build system (Tecmake) and is not packaged in apt, brew,
# vcpkg, or Conan, so we drive it locally. This script is platform-aware
# (macOS / Linux) and idempotent — re-running it just verifies/rebuilds.

set -euo pipefail

IM_VERSION="3.15"
IM_TARBALL="im-${IM_VERSION}_Sources.tar.gz"
IM_URL="https://sourceforge.net/projects/imtoolkit/files/${IM_VERSION}/Docs%20and%20Sources/${IM_TARBALL}/download"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
THIRD_PARTY="${REPO_ROOT}/third_party"
IM_DIR="${THIRD_PARTY}/im"

mkdir -p "${THIRD_PARTY}"
cd "${THIRD_PARTY}"

if [[ ! -d "${IM_DIR}" ]]; then
    if [[ ! -f "${IM_TARBALL}" ]]; then
        echo "==> Downloading ${IM_TARBALL}"
        curl -L -o "${IM_TARBALL}" "${IM_URL}"
    fi
    echo "==> Extracting ${IM_TARBALL}"
    # IM tarball has a few self-referential hardlinks under dox/ that bsdtar
    # warns about but doesn't fail on; ignore the warnings.
    tar xzf "${IM_TARBALL}" || true
fi

cd "${IM_DIR}"

# Patch 1: case-insensitive filesystems (APFS) make these `VERSION` files
# shadow the C++ <version> header during stdlib includes. The files are
# documentation only; safe to remove.
rm -f src/lz4/VERSION src/libtiff/VERSION src/libpng/VERSION

# Patch 2: macOS libpng. Upstream config.mak only handles Linux (system
# package) and Windows (bundled). Add a macOS branch that uses Homebrew.
if ! grep -q 'MacOS, $(TEC_SYSNAME)' src/config.mak; then
    python3 - <<'PY'
import re, pathlib
p = pathlib.Path("src/config.mak")
text = p.read_text()
old = (
    "ifneq ($(findstring Win, $(TEC_SYSNAME)), )\n"
    "  SRC += $(SRCPNG) \n"
    "  INCLUDES += libpng\n"
    "else\n"
    "  # In Linux, use the installed files in the system (package libpng-dev)\n"
    "  # If using GTK, then must use the same libpng they use\n"
    "  INCLUDES += /usr/include/libpng\n"
    "endif"
)
new = (
    "ifneq ($(findstring Win, $(TEC_SYSNAME)), )\n"
    "  SRC += $(SRCPNG)\n"
    "  INCLUDES += libpng\n"
    "else ifneq ($(findstring MacOS, $(TEC_SYSNAME)), )\n"
    "  # macOS: use Homebrew libpng\n"
    "  ifneq ($(wildcard /opt/homebrew/include/png.h),)\n"
    "    INCLUDES += /opt/homebrew/include\n"
    "  else\n"
    "    INCLUDES += /usr/local/include\n"
    "  endif\n"
    "else\n"
    "  # In Linux, use the installed files in the system (package libpng-dev)\n"
    "  # If using GTK, then must use the same libpng they use\n"
    "  INCLUDES += /usr/include/libpng\n"
    "endif"
)
assert old in text, "config.mak did not match expected upstream content; was the IM source pre-patched or upstream changed?"
p.write_text(text.replace(old, new))
PY
fi

# Patch 3: macOS FFTW. Same idea as libpng — add Homebrew include + lib path.
if ! grep -q 'MacOS, $(TEC_SYSNAME)' src/im_fftw3.mak; then
    python3 - <<'PY'
import pathlib
p = pathlib.Path("src/im_fftw3.mak")
text = p.read_text()
old = (
    "ifneq ($(findstring Win, $(TEC_SYSNAME)), )\n"
    "  # Windows use local\n"
    "  FFTW = $(TECTOOLS_HOME)/fftw3\n"
    "  ifneq ($(findstring _64, $(TEC_UNAME)), )\n"
    "    LDIR = $(FFTW)/lib/Win64\n"
    "  else\n"
    "    LDIR = $(FFTW)/lib/Win32\n"
    "  endif\n"
    "  INCLUDES += $(FFTW)/include\n"
    "  LIBS += libfftw3f-3 libfftw3-3\n"
    "else  \n"
    "  # Linux use system\n"
    "  LIBS += fftw3f fftw3\n"
    "endif"
)
new = (
    "ifneq ($(findstring Win, $(TEC_SYSNAME)), )\n"
    "  # Windows use local\n"
    "  FFTW = $(TECTOOLS_HOME)/fftw3\n"
    "  ifneq ($(findstring _64, $(TEC_UNAME)), )\n"
    "    LDIR = $(FFTW)/lib/Win64\n"
    "  else\n"
    "    LDIR = $(FFTW)/lib/Win32\n"
    "  endif\n"
    "  INCLUDES += $(FFTW)/include\n"
    "  LIBS += libfftw3f-3 libfftw3-3\n"
    "else ifneq ($(findstring MacOS, $(TEC_SYSNAME)), )\n"
    "  # macOS use Homebrew\n"
    "  ifneq ($(wildcard /opt/homebrew/include/fftw3.h),)\n"
    "    INCLUDES += /opt/homebrew/include\n"
    "    LDIR += /opt/homebrew/lib\n"
    "  else\n"
    "    INCLUDES += /usr/local/include\n"
    "    LDIR += /usr/local/lib\n"
    "  endif\n"
    "  LIBS += fftw3f fftw3\n"
    "else\n"
    "  # Linux use system\n"
    "  LIBS += fftw3f fftw3\n"
    "endif"
)
assert old in text, "im_fftw3.mak did not match expected upstream content"
p.write_text(text.replace(old, new))
PY
fi

# Build the three libraries we link against.
echo "==> Building libim"
( cd src && make im )
echo "==> Building libim_process"
( cd src && make im_process )
echo "==> Building libim_fftw3"
( cd src && make im_fftw3 )

# tecmake also produces a Mach-O *bundle* named libim.so on macOS that breaks
# the linker (it's neither a real .dylib nor a .o); the .a static archive is
# what we want to link against.
if [[ "$(uname)" == "Darwin" ]]; then
    find lib -name 'libim.so' -delete
fi

echo
echo "==> IM build complete. Outputs:"
find lib -maxdepth 2 -type f \( -name '*.a' -o -name '*.dylib' \) | sort
