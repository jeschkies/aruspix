# Fast local dev/test image for reproducing bugs on Linux without
# touching the host system (see the karsten/opencv branch's IM -> OpenCV
# migration and tests/test_aximage_loadfile.cpp).
#
# This is a lightweight throwaway environment for compiling and running
# individual test binaries against Fedora's system wxGTK/OpenCV/doctest
# packages via wx-config/pkg-config. It intentionally does NOT mirror the
# project's real Conan-based build (see conanfile.py and
# .github/workflows/build.yml), which builds wxWidgets' full GTK/X11/
# Wayland stack from source and is what actually produces release/CI
# binaries. Use that (via `conan install` + `cmake --preset conan-release`)
# when you need to build the full `aruspix` target.
#
# Build:
#   docker build -t aruspix-dev .
#
# Use (mount the repo, compile+run a test directly):
#   docker run --rm -v "$(pwd)":/work:Z aruspix-dev bash -lc '
#     clang++ -std=c++17 -g \
#       -DAX_SUP_FIXTURES_DIR=\"/work/tests/fixtures/sup_pages\" \
#       $(wx-config --cxxflags) $(pkg-config --cflags opencv4) \
#       -I/work/src/app \
#       /work/src/app/aximage.cpp \
#       /work/tests/test_aximage_loadfile.cpp \
#       /work/tests/test_aximage_loadfile_main.cpp \
#       $(wx-config --libs) $(pkg-config --libs opencv4) \
#       -o /tmp/aximage_loadfile_tests
#     /tmp/aximage_loadfile_tests
#   '

FROM fedora:44

RUN dnf install -y \
        clang \
        cmake \
        ninja-build \
        make \
        wxGTK-devel \
        opencv-devel \
        doctest-devel \
        pkgconf-pkg-config \
    && dnf clean all

WORKDIR /work
