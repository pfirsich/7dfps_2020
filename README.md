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

Like Linux, but thankfully every package, except [docopt](https://github.com/docopt/docopt.cpp) is available on homebrew. To install that manually, just clone it and do the usual CMake song and dance.
[LuaJIT](https://github.com/LuaJIT/LuaJIT) is also on homebrew but.. don't use it. So please also just clone and manually install it.

In detail you have to do this:

```sh
# clone the game
git clone git@github.com:pfirsich/7dfps_2020.git && cd 7dfps_2020

# init needed submodules
git submodule update --init deps/glwrap deps/gltf deps/sol2 deps/imgui deps/soloud/repo
cd deps/glwrap && git submodule update --init deps/stb && cd ../..
cd deps/gltf && git submodule update --init simdjson && cd ../..

# install brew deps
brew install ninja cmake sdl2 fmt glm enet

# create a directory to clone and build the other deps
mkdir build-other-deps
cd build-other-deps

# clone and build docopt
git clone git@github.com:docopt/docopt.cpp.git && cd docopt.cpp
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo -DCMAKE_CXX_COMPILER=clang++ ..
ninja
sudo ninja install
cd ../..

# clone and build lua jit
git clone git@github.com:LuaJIT/LuaJIT.git && cd LuaJIT
# NOTE: MACOSX_DEPLOYMENT_TARGET needs to be a supported SDK.
# SDKs are installed in `/Library/Developer/CommandLineTools/SDKs/`.
# For `10.15` to work you need to have `MacOSX10.15.sdk` in this directory.
# Other versions might work too.
make MACOSX_DEPLOYMENT_TARGET=10.15
sudo make install
cd ../..

# back to the main repo
cd ..

# build the game
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo -DCMAKE_CXX_COMPILER=clang++ ..
ninja

# run the game
cd ..
./build/complexity
```
