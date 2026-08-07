# driver_sim

FRC Driver simulation with AdvantageScope 3D Mechanism support and FMS scoring UI.

This project uses a modified version of [dashandslash/blackboard_app](https://github.com/dashandslash/blackboard_app/tree/main) as a backend library.

## Setup

### Discord Social SDK
After becoming part of our discord dev team, download it from this link: https://discord.com/developers/applications/select/social-sdk/downloads. Unzip it and then put the discord_social_sdk folder inside of _external/discordsdk-src/_external.

## Prepare

## Package robot code
zip up the jni folder from /build/jni and zip up the 2026-robot.jar from /build/libs/2026-robot.jar and place them where they are in the video
https://github.com/user-attachments/assets/bf91111a-d703-4da5-9b15-164906701b84

## Build

### Requirements

- CMake 3.21+
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
