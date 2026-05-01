# JPackCompiler

JPackCompiler is a compiler for the JPack language, a C/C++-like programming language that compiles to Minecraft Java Edition datapacks. Instead of writing raw `.mcfunction` files, you write structured code with variables, functions, control flow, and classes, and the compiler handles generating the datapack file structure, scoreboard management, and NBT storage.

If you have any issues or questions, you can report/ask them on github or in my discord server: https://discord.gg/Vc7j8RuztG

## How it works

JPack compiles to Minecraft datapacks by mapping language constructs to datapack primitives:

- Integer and floating point variables are stored as scoreboard objectives
- String variables are stored in NBT command storage
- Functions compile to `.mcfunction` files
- Control flow (if, while, for) compiles to sub-functions called conditionally via `execute if/unless`
- The `@intrinsic` annotation marks functions that wrap raw Minecraft commands using the macro system
- Floating point arithmetic uses fixed-point scaling (float x1000, double x1000000)
- Compile-time annotations generate JSON files for dimensions, structures, template pools, and advancements

## Requirements

- Minecraft Java Edition 1.21 or later (uses the `function` directory name introduced in 1.21, and the macro system and `/return` command introduced in 1.20)
- A C++ compiler with C++20 support to build JPackCompiler itself

## Building

Clone the repository and build with MSBuild (this project was made in Rider, but Visual Studio works just as well) or your preferred C++ build system (will need some adapting). The project has no external dependencies except nlohmann/json, which is included as a single header file, and the C++ standard library.

## Usage

```
JPackCompiler -i <file> [-i <file> ...] -o <output> [-d]
```

**Options**

    -i, --input     Input .jpack source file. Can be specified multiple times.
    -o, --output    Output directory for the generated datapack. Defaults to "output".
    -d, --debug     Print debug output including tokens, AST, and codegen details.
    -f, --force     Force overwrite of output directory even if it does not appear to be a JPack output.
    -v, --version   Print the compiler version.
    -h, --help      Print usage information.

**Example**

```
JPackCompiler -i mygame.jpack -i stdlib.jpack -o C:/minecraft/saves/MyWorld/datapacks/mygame
```

## Standard library

The JPack project comes with a standard library that is provided as `stdlib.jpack` and can be included with `#include "stdlib.jpack"`.

**World** — setBlock, setBlockMode, fill, setBorderCenter, setBorderDistance, addBorderDistance, setBorderDistanceOverTime, addBorderDistanceOverTime, placeStructure, placeJigsaw, placeJigsawInDimension

**Entity** — summon, kill, tag, removeTag, effect, clearEffect, teleport, teleportToDimension, damage, ride, exists, existsAt

**Player** — gamemode, give, clear, kick, addExperience, setExperience, addLevels, advancement, recipe, spectate, getX, getY, getZ, getXFloat, getYFloat, getZFloat, getAttribute, getAttributeFloat

**Chat** — say, sayInt, tellRaw, msg, teamMsg, title, subtitle, actionbar

**Sound** — play, stop

**Server** — schedule, gamerule, addObjective, removeObjective, setScore, getScore, addScore, removeScore, executeInDimension, executeAsAt, executeAsInDimension

**Math** — abs, min, max, random

**NBT** — getEntityData, getEntityDataFloat, getBlockData, setEntityData, setBlockData

**Team** — add, remove, join, leave, setColor, setFriendlyFire, setDisplayName, setPrefix, setSuffix, setCollision, setDeathMessage, setNameTag

## VS Code extension

A VS Code extension providing syntax highlighting for `.jpack` files is included in the `jpack-extension` directory.

## Known limitations

- Multiplication and division between two variables works, but there is no native floating point arithmetic. Float and double values use fixed-point integer arithmetic with potential precision loss on repeated multiplication.
- Recursive functions work but share parameter scoreboard entries, so a function calling itself will overwrite its own parameters before the recursive call completes.
- Structs are parsed and represented in the AST but are not yet implemented in codegen. Enums are similarly parsed but not yet compiled.
- Target selectors containing conditions such as `@a[tag=something]` may cause issues in some contexts due to how curly braces are handled in the macro system. Use `@store_result_intrinsic` for execute-chain selectors.
- `Server::schedule` only works correctly with zero-argument functions or wrapper functions. Arguments passed to the inner function call are discarded at compile time.
- Array writes via index assignment are supported but the `__array_set` macro helper requires the array name to be passed explicitly, which is not yet fully automated.
- (Issue of the dungeon example, but exposes some current lang limitations) The portal dimension transition uses a fixed 20-tick delay for chunk loading which may be insufficient on very slow machines or servers.
