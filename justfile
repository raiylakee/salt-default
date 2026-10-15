builddir := "builddir"
exe := builddir + "/salt_default"

default:
    @just --list

setup:
    meson setup {{builddir}}

wipe:
    meson setup {{builddir}} --wipe

build:
    meson compile -C {{builddir}}

run: build
    ./{{exe}}

format:
    find src -name '*.cpp' -o -name '*.h' -o -name '*.hpp' | xargs clang-format -i

clean:
    rm -rf {{builddir}}
