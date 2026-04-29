"""Conan recipe for the IM Toolkit (https://www.tecgraf.puc-rio.br/im/).

IM ships a hand-rolled Make-based build system (Tecmake). The recipe
downloads the upstream tarball, applies the same patches as our previous
``scripts/build-im.sh``, then drives Tecmake to produce static archives
for the three libraries Aruspix links against: ``im``, ``im_process``,
and ``im_fftw3``.
"""

import os
import shutil

from conan import ConanFile
from conan.tools.files import download, copy, replace_in_file


class ImConan(ConanFile):
    name = "im"
    version = "3.15"
    description = "IM Toolkit for Digital Imaging from Tecgraf/PUC-Rio"
    license = "Tecgraf"
    homepage = "https://www.tecgraf.puc-rio.br/im/"
    url = "https://sourceforge.net/projects/imtoolkit/"
    settings = "os", "compiler", "build_type", "arch"
    package_type = "static-library"

    def requirements(self):
        # IM links against an external libpng (its own libpng sources are
        # only used on Windows), libtiff (bundled in upstream IM but
        # patched out below so we don't collide with consumers like
        # wxWidgets that bring their own libtiff), and FFTW for
        # im_fftw3.
        self.requires("libpng/[>=1.6 <2]", transitive_headers=True)
        self.requires("libtiff/[>=4.0 <5]", transitive_headers=True)
        self.requires("fftw/[>=3.3 <4]")

    def source(self):
        tarball = "im-3.15_Sources.tar.gz"
        url = (
            "https://sourceforge.net/projects/imtoolkit/files/"
            "3.15/Docs%20and%20Sources/im-3.15_Sources.tar.gz/download"
        )
        download(self, url, tarball)
        # IM's tarball contains a few self-referential hardlinks under dox/
        # that bsdtar warns about; ignore the non-zero exit.
        self.run(f"tar xzf {tarball} || true")
        os.remove(tarball)

    def _apply_patches(self, im_root):
        # Case-insensitive filesystems (APFS) make these `VERSION` files
        # shadow the C++ <version> header during stdlib includes.
        for sub in ("lz4", "libtiff", "libpng"):
            stale = os.path.join(im_root, "src", sub, "VERSION")
            if os.path.exists(stale):
                os.remove(stale)

        # Patch config.mak: route libpng include path through env-var so
        # the Conan-provided libpng wins on every platform.
        replace_in_file(
            self,
            os.path.join(im_root, "src", "config.mak"),
            (
                "ifneq ($(findstring Win, $(TEC_SYSNAME)), )\n"
                "  SRC += $(SRCPNG) \n"
                "  INCLUDES += libpng\n"
                "else\n"
                "  # In Linux, use the installed files in the system (package libpng-dev)\n"
                "  # If using GTK, then must use the same libpng they use\n"
                "  INCLUDES += /usr/include/libpng\n"
                "endif"
            ),
            (
                "ifneq ($(findstring Win, $(TEC_SYSNAME)), )\n"
                "  SRC += $(SRCPNG)\n"
                "  INCLUDES += libpng\n"
                "else\n"
                "  ifdef IM_PNG_INCLUDE\n"
                "    INCLUDES += $(IM_PNG_INCLUDE)\n"
                "  else\n"
                "    INCLUDES += /usr/include/libpng\n"
                "  endif\n"
                "endif"
            ),
        )

        # Patch config.mak: stop compiling the bundled libtiff sources
        # into libim.a. Upstream IM links its own vendored libtiff
        # objects, which then collide at consumer link time with the
        # libtiff.a a tool like wxWidgets pulls in. Keep the bundled
        # libtiff headers on the include path though — IM's
        # im_format_tiff.cpp and tiff_binfile.c reach into private
        # libtiff internals (tiffiop.h) that Conan's libtiff package
        # does not expose. The actual libtiff symbols come from the
        # Conan-provided libtiff.a at link time.
        replace_in_file(
            self,
            os.path.join(im_root, "src", "config.mak"),
            "SRC += $(SRCTIFF) tiff_binfile.c",
            "SRC += tiff_binfile.c",
        )

        # Patch im_fftw3.mak: same idea for FFTW include + lib paths.
        replace_in_file(
            self,
            os.path.join(im_root, "src", "im_fftw3.mak"),
            (
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
            ),
            (
                "ifneq ($(findstring Win, $(TEC_SYSNAME)), )\n"
                "  FFTW = $(TECTOOLS_HOME)/fftw3\n"
                "  ifneq ($(findstring _64, $(TEC_UNAME)), )\n"
                "    LDIR = $(FFTW)/lib/Win64\n"
                "  else\n"
                "    LDIR = $(FFTW)/lib/Win32\n"
                "  endif\n"
                "  INCLUDES += $(FFTW)/include\n"
                "  LIBS += libfftw3f-3 libfftw3-3\n"
                "else\n"
                "  ifdef IM_FFTW_INCLUDE\n"
                "    INCLUDES += $(IM_FFTW_INCLUDE)\n"
                "  endif\n"
                "  ifdef IM_FFTW_LIBDIR\n"
                "    LDIR += $(IM_FFTW_LIBDIR)\n"
                "  endif\n"
                "  LIBS += fftw3f fftw3\n"
                "endif"
            ),
        )

    @property
    def _im_root(self):
        return os.path.join(self.source_folder, "im")

    def build(self):
        self._apply_patches(self._im_root)

        png_dep = self.dependencies["libpng"]
        fftw_dep = self.dependencies["fftw"]

        overrides = {
            "IM_PNG_INCLUDE": png_dep.cpp_info.includedirs[0],
            "IM_FFTW_INCLUDE": fftw_dep.cpp_info.includedirs[0],
            "IM_FFTW_LIBDIR": fftw_dep.cpp_info.libdirs[0],
        }
        saved = {k: os.environ.get(k) for k in overrides}
        os.environ.update(overrides)
        try:
            src_dir = os.path.join(self._im_root, "src")
            # We only ship static archives, so suppress Tecmake's dylib
            # link step (which would otherwise need libpng/fftw on the
            # link line — irrelevant for the .a output).
            for target in ("im", "im_process", "im_fftw3"):
                self.run(f"make {target} NO_DYNAMIC=Yes", cwd=src_dir)
        finally:
            for k, v in saved.items():
                if v is None:
                    os.environ.pop(k, None)
                else:
                    os.environ[k] = v

    def package(self):
        copy(
            self,
            "*.h",
            src=os.path.join(self._im_root, "include"),
            dst=os.path.join(self.package_folder, "include"),
        )

        # Tecmake places outputs under lib/<TEC_UNAME>/.
        lib_root = os.path.join(self._im_root, "lib")
        if os.path.isdir(lib_root):
            tec_dir = next(
                (d for d in os.listdir(lib_root)
                 if os.path.isdir(os.path.join(lib_root, d))),
                None,
            )
            if tec_dir:
                copy(
                    self,
                    "lib*.a",
                    src=os.path.join(lib_root, tec_dir),
                    dst=os.path.join(self.package_folder, "lib"),
                    keep_path=False,
                )

        # macOS Tecmake also produces a libim.so that is actually a
        # Mach-O bundle (not a real dylib), which the linker rejects.
        bogus = os.path.join(self.package_folder, "lib", "libim.so")
        if os.path.exists(bogus):
            os.remove(bogus)

    def package_info(self):
        # Link order matters: im_fftw3 depends on im_process which
        # depends on im.
        self.cpp_info.libs = ["im_fftw3", "im_process", "im"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs = ["m", "pthread"]
