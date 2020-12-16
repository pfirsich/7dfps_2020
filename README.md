# ARBITRARY COMPLEXITY

Made for 7DFPS 2020

see here: https://pfirsich.itch.io/arbitrary-complexity

## Running

### Windows

Windows builds are built by GitHub Actions, so go the [the actions page](https://github.com/pfirsich/7dfps_2020/actions), select the latest action that ran successfully, click on the `build-windows` job and download the artifact (top-right corner). Keep in mind that you need the [MSVC Redistributable for VS 2019](https://aka.ms/vs/16/release/vc_redist.x64.exe) to run it.

### Linux

Currently you need to build it yourself.

## Building

I advise you not to clone with `--recursive`, because simdjson has **a lot** of submodules, so it will take a long time and the resulting directory will be pretty large.

If you don't want to build with vcpkg, make sure the vcpkg submodule is not initialized or deinitialize it with `git submodule deinit vcpkg`.

### Windows

Make sure to initialize the vcpkg submodule and bootstrap/install as also done in [build.yml](.github/workflows/build.yml).

### Linux

Since luajit is not available on vcpkg on any platform other than Windows, you have to make sure that the `find_library`-calls just work. That means that you need to install all the dependencies with your system's package manager. If they are not available, you are out of luck. You might consider cloning them yourself and installing them manually though.
You can find the necessary packages in [build.yml](.github/workflows/build.yml).

### Mac

See above, but thankfully every package, except [docopt](https://github.com/docopt/docopt.cpp) is available on homebrew. To install that manually, just clone it and do the usual CMake song and dance.
[LuaJIT](https://github.com/LuaJIT/LuaJIT) is also on homebrew but.. don't use it. So please also just clone and manually install it (`make MACOSX_DEPLOYMENT_TARGET=10.15 && make install` or something like that).

### Build .dmg

```sh
mkdir build
cd build
cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -G 'Ninja' ..

ninja

# Fix enet permissions
chmod +w /usr/local/opt/enet/lib/libenet.7.dylib

# Copy media
cp -r ../media/ complexity.app/Contents/media

cpack -G DragNDrop
```
