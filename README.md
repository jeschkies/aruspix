# Aruspix #

Aruspix is software application for the optical recognition, the superimposition and the collation of early music prints. More information about the project is available at [http://www.aruspix.net](http://www.aruspix.net). 

The [Wiki](https://github.com/DDMAL/aruspix/wiki) of this repository gives more information on how to build Aruspix.

## Building on Windows

Aruspix is built as 64-bit on Windows. The dependencies Conan resolves are `x86_64`, so the aruspix target must be compiled from an x64 MSVC environment or the final link will fail with `LNK4272` warnings and unresolved-symbol errors.

1. Open the **x64 Native Tools Command Prompt for VS 2022** (or run `vcvarsall.bat x64` in a fresh shell).
2. `conan export recipes/im`
3. `conan install . --build=missing -c tools.cmake.cmaketoolchain:generator=Ninja`
4. Source Conan's generated environment so subsequent cmake invocations use the x64 toolchain Conan picked:
   ```
   build\Release\generators\conanbuild.bat
   ```
5. `cmake --preset conan-release`
6. `cmake --build build/Release`

If `conan install` complains that `Windows builds require arch=x86_64`, you are in an x86 shell — reopen the x64 prompt or pass `-s arch=x86_64 -s:b arch=x86_64` to `conan install`.
