from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class AruspixConan(ConanFile):
    name = "aruspix"
    settings = "os", "compiler", "build_type", "arch"
    generators = "VirtualBuildEnv"

    def requirements(self):
        # The IM toolkit is built from a local recipe (recipes/im).
        # Run `conan export recipes/im` once before `conan install`.
        self.requires("im/3.15")
        self.requires("wxwidgets/[~3.2]")
        self.requires("libxml2/[>=2.10 <3]")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()
