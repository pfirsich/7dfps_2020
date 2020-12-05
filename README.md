# 7dfps
## Running
### Windows
Windows builds are built by GitHub Actions, so go the [the actions page](https://github.com/pfirsich/7dfps_2020/actions), select the latest action that ran successfully, click on the `build-windows` job and download the artifact (top-right corner). Keep in mind that you need the [MSVC Redistributable for VS 2019](https://aka.ms/vs/16/release/vc_redist.x64.exe) to run it.

### Linux
Currently you need to build it yourself.

## Building
See [.github/workflows/build.yml] to see an example of how to build without vcpkg (build-linux) and an example of how to build with vcpkg (build-windows).

I advise you to not clone with `--recursive`, because simdjson has **a lot** of submodules, so it will take a long time and the resulting directory will be large.

If you don't want to build with vcpkg, make sure the vcpkg submodule is not initialized or deinitialize it with `git submodule deinit vcpkg`.
