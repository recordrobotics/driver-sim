# driver_sim

FRC Driver simulation with AdvantageScope 3D Mechanism support and FMS scoring UI.

This project uses a modified version of [dashandslash/blackboard_app](https://github.com/dashandslash/blackboard_app/tree/main) as a backend library.

## Setup

### Discord Social SDK
After becoming part of our discord dev team, download it from this link: https://discord.com/developers/applications/select/social-sdk/downloads. Unzip it and then put the discord_social_sdk folder inside of _external/discordsdk-src/_external.

## Prepare

## Package manifest
Manifest files in driver-sim can either be standalone yaml files or as part of a zip file together with packaged assets. To build with a packaged manifest, place a `manifest.yaml` file in the [driver_sim/packaged](driver_sim/packaged) directory. Additionally you can add packaged assets such as code, jni, etc. into the packaged folder as unzipped files.

During the build the whole packaged folder gets zipped and embedded into the executable.

For an example manifest.yaml see [docs/example-manifest.yaml](docs/example-manifest.yaml)

<img width="155" height="154" alt="image" src="https://github.com/user-attachments/assets/b7b29446-7237-422e-a994-bc80915c8424" />

### Modifying packaged manifest/assets

Due to CMake limitations, it doesn't like autodetecting changes to the packaged directory. When you change something the best way to force a repackage is to delete [build/packaged.zip](build/packaged.zip) and build.

## Build

### Requirements

- CMake 3.25+
- A C++20-capable compiler

### Configure and build (Ninja)

```bash
cmake -S . -B build -G Ninja
cmake --build build --target driver_sim -j
```

### Configure and build (Visual Studio 2022)

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --target driver_sim
```

## Run

Run the `driver_sim` target from your IDE, or execute the built binary from the `build` directory.

On Windows with Visual Studio, this is typically:

```text
build/driver_sim/Release/driver_sim.exe
```

With single-config generators (for example Ninja), this is typically:

```text
build/driver_sim/driver_sim.exe
```
