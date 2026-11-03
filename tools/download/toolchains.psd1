@{
    # Official portable distributions, pinned so a new PC gets the same tools.
    Packages = @(
        @{
            Name = 'CMake 3.31.10'
            Directory = 'cmake-3.31.10-windows-x86_64'
            Archive = 'cmake-3.31.10-windows-x86_64.zip'
            Url = 'https://github.com/Kitware/CMake/releases/download/v3.31.10/cmake-3.31.10-windows-x86_64.zip'
            Sha256 = '13d1a463d7130df5339baedd63d8ae990aaf385062b2f42f372796143ae94086'
            RequiredFiles = @('share/cmake-3.31/Modules/CMakeDetermineCCompiler.cmake')
            Checks = @(@{ Exe = 'cmake.exe'; Argument = '--version'; Pattern = '^cmake version 3\.31\.10\b' })
        }
        @{
            Name = 'Arm GNU Toolchain 12.2.Rel1 (GCC + G++)'
            Directory = 'arm-gnu-toolchain-12.2.rel1-mingw-w64-i686-arm-none-eabi'
            Archive = 'arm-gnu-toolchain-12.2.rel1-mingw-w64-i686-arm-none-eabi.zip'
            Url = 'https://developer.arm.com/-/media/Files/downloads/gnu/12.2.rel1/binrel/arm-gnu-toolchain-12.2.rel1-mingw-w64-i686-arm-none-eabi.zip'
            Sha256 = 'ad1427496cde9bbe7604bc448ec6e115c6538e04af1c8275795ebb1c2b7b2830'
            RequiredFiles = @(
                'libexec/gcc/arm-none-eabi/12.2.1/cc1.exe'
                'libexec/gcc/arm-none-eabi/12.2.1/cc1plus.exe'
                'arm-none-eabi/lib/thumb/v7-m/nofp/libc.a'
                'arm-none-eabi/lib/thumb/v7-m/nofp/libm.a'
                'bin/arm-none-eabi-ld.exe'
            )
            Checks = @(
                @{ Exe = 'arm-none-eabi-gcc.exe'; Argument = '-dumpfullversion'; Pattern = '^12\.2\.1$' }
                @{ Exe = 'arm-none-eabi-g++.exe'; Argument = '-dumpfullversion'; Pattern = '^12\.2\.1$' }
                @{ Exe = 'arm-none-eabi-size.exe'; Argument = '--version'; Pattern = '^GNU size' }
                @{ Exe = 'arm-none-eabi-objcopy.exe'; Argument = '--version'; Pattern = '^GNU objcopy' }
                @{ Exe = 'arm-none-eabi-ar.exe'; Argument = '--version'; Pattern = '^GNU ar' }
            )
        }
        @{
            Name = 'xPack Windows GNU Make 4.4.1-3'
            Directory = 'xpack-windows-build-tools-4.4.1-3'
            Archive = 'xpack-windows-build-tools-4.4.1-3-win32-x64.zip'
            Url = 'https://github.com/xpack-dev-tools/windows-build-tools-xpack/releases/download/v4.4.1-3/xpack-windows-build-tools-4.4.1-3-win32-x64.zip'
            Sha256 = '113d4dfdbbc56dc9b865c9f75d38cd0da82f0d7094187e6f7a803fe6eef1d218'
            RequiredFiles = @('bin/make.exe')
            Checks = @(@{ Exe = 'make.exe'; Argument = '--version'; Pattern = '^GNU Make 4\.4\.1\b' })
        }
    )
}
