# JPackCompiler

JPackCompiler is a compiler for the JPack language, a C/C++-like programming language that compiles to Minecraft Java Edition datapacks. Instead of writing raw `.mcfunction` files, you write structured code with variables, functions, control flow, and classes, and the compiler handles generating the datapack file structure, scoreboard management, and NBT storage.

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

The datapack namespace is derived from the first input file's name. The standard library should therefore be passed after your own source file, even though all files are concatenated before compilation.

The output directory is wiped and regenerated on every compilation. The compiler checks that the output directory looks like a JPack output before wiping it, to avoid accidentally deleting unrelated files. Use `-f` to bypass this check.

The output directory will contain a complete datapack:

```
output/
  pack.mcmeta
  data/
    minecraft/
      tags/
        function/
          tick.json
          load.json
    <namespace>/
      function/
        <function name>.mcfunction
```

## The JPack Language

### Primitive types

    int       32-bit signed integer, stored as a scoreboard objective
    float     Fixed-point number scaled by 1000 (1.5 is stored as 1500)
    double    Fixed-point number scaled by 1000000
    bool      Integer scoreboard value, 0 or 1
    string    NBT storage string

### Variable declarations

Variables follow C-style syntax with a semicolon terminator.

```
int x = 10;
float speed = 1.5f;
double precision = 1.5;
bool active = true;
string name = "hello";
```

### Arrays

Integer arrays are backed by NBT list storage. The size must be a compile-time constant.

```
int scores[4] = {0, 0, 0, 0};
int first = scores[0];
scores[1] = 5;
```

Array reads generate a per-array helper function with a chain of scoreboard comparisons since Minecraft does not support dynamic NBT list indexing via the macro system.

### Functions

```
int add(int a, int b) {
    return a + b;
}

void greet(string player) {
    Chat::say("Hello!");
}
```

Functions compile to individual `.mcfunction` files. Parameters are passed via scoreboard operations for numeric types and NBT storage for strings. Return values use the `/return` command introduced in Minecraft 1.20.

### Entry points

The `@tick` and `@load` annotations register a function with the Minecraft tick and load function tags.

```
@load
void onLoad() {
    Chat::say("Datapack loaded!");
}

@tick
void onTick() {
    // runs every game tick
}
```

### Control flow

```
if (x == 10) {
    x = x + 1;
} else {
    x = 0;
}

while (x < 100) {
    x = x + 1;
}

for (int i = 0; i < 10; i++) {
    total = total + i;
}
```

Each if/else branch and loop body compiles to a separate `.mcfunction` file called conditionally. While loops recurse by calling themselves at the end of each iteration.

### Operators

    Arithmetic    +  -  *  /  %
    Comparison    ==  !=  <  >  <=  >=
    Logical       &&  ||  !
    Assignment    =
    Increment     ++  --
    Scope         ::

### Classes, structs, and enums

```
class Player {
public:
    int health = 100;
private:
    float speed = 1.5f;
};

struct Point {
    int x = 0;
    int y = 0;
};

enum Direction {
    NORTH,
    SOUTH,
    EAST,
    WEST
};
```

### Namespaces

Functions can be grouped into namespaces. Namespace names are used as subdirectory names in the generated datapack.

```
namespace World {
    void doSomething() {
        // ...
    }
}

// called as:
World::doSomething();
```

### Include directive

Source files can include other files using the `#include` directive. The included file's contents are inserted at the point of the directive before compilation.

```
#include "stdlib.jpack"
```

### Intrinsic functions

The `@intrinsic` annotation marks a function as a wrapper around a raw Minecraft command. The function body must contain one or more `@cmd` directives. Parameters are interpolated into the command string using `{paramName}` syntax, which the compiler converts to the Minecraft macro system.

```
@intrinsic
void setBlock(int x, int y, int z, string block) {
    @cmd "setblock {x} {y} {z} {block}";
}
```

The compiler emits a macro `.mcfunction` file with `$(paramName)` substitutions, and passes arguments via NBT storage using `function ... with storage`.

For intrinsics that return a value from a command, use `@returns_command`:

```
@intrinsic
@returns_command
int random(int min, int max) {
    @cmd "random value {min}..{max}";
}
```

For intrinsics whose command is an `execute` chain that needs to store a result, use `@store_result_intrinsic`. This is necessary when the result needs to be inserted into the execute chain rather than wrapped with an outer `execute store result`. Use `{__ns}` as a placeholder for the namespace in the storage path.

```
@intrinsic
@store_result_intrinsic
int existsAt(string executor, string selector) {
    @cmd "execute at {executor} store result storage {__ns}:return value int 1 if entity {selector}";
}
```

If you need to capture the result of an `execute` chain, you must use `@store_result_intrinsic`. Using `@returns_command` for execute-based commands will produce invalid nested `execute` syntax.

For intrinsics that write back to a caller variable via a reference parameter, use `@ref_intrinsic`. Reference parameters are passed as two macro variables: `{paramName_player}` for the scoreboard player name and `{paramName_obj}` for the scoreboard objective name.

```
@intrinsic
@ref_intrinsic
void getX(string target, int& x) {
    @cmd "execute store result score {x_player} {x_obj} run data get entity {target} Pos[0] 1";
}
```

Called as:

```
int px = 0;
Player::getX("@p", px);
```

### Passing a function reference to an intrinsic

When a string parameter of an intrinsic receives a function call expression, the compiler extracts the function path at compile time instead of evaluating the call. This is used with `Server::schedule` and `Server::executeAsAt`:

```
void announceWinner() {
    Chat::say("The winner has been decided!");
}

Server::schedule(announceWinner(), 200);
Server::executeAsAt("@a", "@e[tag=portal]", announceWinner());
```

The arguments to the inner call are discarded — only the function path is used. The called function must be a zero-argument function or a wrapper that has the desired behaviour baked in.

### Event functions

The `@event` annotation registers a function as an advancement reward, firing when a Minecraft game event occurs.

```
@event("minecraft:player_killed_entity")
@revoke
void onKill() {
    Chat::say("You got a kill!");
}
```

`@revoke` automatically appends an advancement revoke command to the function, allowing it to fire repeatedly. Without `@revoke` the advancement fires only once per player.

A condition can be supplied as a second argument to filter which entities trigger the event:

```
@event("minecraft:player_killed_entity", "{\"entity\":{\"type\":\"minecraft:zombie\"}}")
@revoke
void onZombieKill() {
    Player::give("@s", "minecraft:rotten_flesh", 1);
}
```

Event functions execute as the player who triggered the advancement, so `@s` refers to that player inside the function body.

### Dimension generation

The `@dimension` annotation generates the JSON files required for a custom dimension. The annotated function is automatically registered as a load function.

```
@dimension("void_world")
@dimensionType("{\"ultrawarm\":false,\"natural\":true,\"has_skylight\":true,\"has_ceiling\":false,\"ambient_light\":0.0,\"bed_works\":false,\"respawn_anchor_works\":false,\"has_raids\":true,\"piglin_safe\":false,\"monster_spawn_block_light_limit\":0,\"monster_spawn_light_level\":7,\"height\":256,\"min_y\":0,\"logical_height\":256,\"coordinate_scale\":1.0,\"infiniburn\":\"#minecraft:infiniburn_overworld\",\"effects\":\"minecraft:overworld\"}")
void onVoidWorldLoad() {
    Chat::say("Void world loaded!");
}
```

`@dimensionType` accepts a raw JSON string defining the dimension type properties. If omitted, a default overworld-like dimension type is used. A `@dimensionGenerator` annotation can also be provided with a raw JSON string to override the default flat void generator.

This generates two files: `data/<namespace>/dimension_type/<name>.json` and `data/<namespace>/dimension/<name>.json`.

### Structure and jigsaw generation

The `@structure` annotation generates the worldgen structure JSON required for jigsaw placement.

```
@structure("dungeontest:dungeon_start", "dungeontest:dungeon", "dungeontest:room", "7")
void onStructureLoad() { }
```

Arguments are: structure name, start pool, start jigsaw name, and max depth.

The `@templatePool` annotation generates a worldgen template pool JSON, defining the weighted set of structure pieces used by jigsaw generation.

```
@templatePool("dungeontest:dungeon", [
    ("dungeontest:anchor", 1),
    ("dungeontest:4way", 3),
    ("dungeontest:3way", 2)
])
void onPoolLoad() { }
```

The `.nbt` structure files themselves must be built in-game using structure blocks and placed manually in `data/<namespace>/structure/`. The compiler only generates the JSON wrappers that reference them.

### Comments

```
// single line comment

/* multi
   line comment */
```

### References

Parameters can be passed by reference using `&`, similar to C++. A reference parameter uses the caller's scoreboard entry directly instead of copying.

```
void increment(int& value) {
    value = value + 1;
}
```

## Standard library

The standard library is provided as `stdlib.jpack` and can be included with `#include "stdlib.jpack"`.

**World** — setBlock, setBlockMode, fill, setBorderCenter, setBorderDistance, addBorderDistance, setBorderDistanceOverTime, addBorderDistanceOverTime, placeStructure, placeJigsaw, placeJigsawInDimension

**Entity** — summon, kill, tag, removeTag, effect, clearEffect, teleport, teleportToDimension, damage, ride, exists, existsAt

**Player** — gamemode, give, clear, kick, addExperience, setExperience, addLevels, advancement, recipe, spectate, getX, getY, getZ, getXFloat, getYFloat, getZFloat, getAttribute, getAttributeFloat

**Chat** — say, sayInt, tellRaw, msg, teamMsg, title, subtitle, actionbar

**Sound** — play, stop

**Server** — schedule, gamerule, addObjective, removeObjective, setScore, getScore, addScore, removeScore, executeInDimension, executeAsAt, executeAsInDimension

**Math** — abs, min, max, random

**NBT** — getEntityData, getEntityDataFloat, getBlockData, setEntityData, setBlockData

**Team** — add, remove, join, leave, setColor, setFriendlyFire, setDisplayName, setPrefix, setSuffix, setCollision, setDeathMessage, setNameTag

### Player position

`Player::getX`, `Player::getY`, and `Player::getZ` return the integer block coordinate of the target entity. `Player::getXFloat`, `Player::getYFloat`, and `Player::getZFloat` return the coordinate scaled by 1000 to match the float fixed-point system, giving sub-block precision.

```
int px = 0;
int py = 0;
int pz = 0;
Player::getX("@p", px);
Player::getY("@p", py);
Player::getZ("@p", pz);
World::setBlock(px, py + 2, pz, "minecraft:diamond_block");
```

### Score manipulation

`Server::addObjective` and `Server::removeObjective` manage scoreboard objectives. `Server::setScore`, `Server::getScore`, `Server::addScore`, and `Server::removeScore` manipulate player scores on existing objectives. This is the primary mechanism for persistent state that survives across ticks.

```
// in onLoad:
Server::addObjective("kills", "playerKillCount");

// in onTick:
int kills = 0;
Server::getScore("@p", "kills", kills);
```

### Portal and dimension transition

A typical portal system uses a marker entity to define the portal center, detects nearby players each tick, and teleports them to a custom dimension with a short delay to allow chunk loading.

```
@load
void onLoad() {
    Entity::summon("minecraft:marker", 0, 100, 0);
    Entity::tag("@e[type=minecraft:marker]", "portal_center");
}

void placeDungeonForPlayer() {
    World::placeJigsaw("dungeontest:dungeon", "dungeontest:start", 7, 0, 100, 0);
    Entity::removeTag("@s", "entering_void");
    Entity::clearEffect("@s");
}

void schedulePlacement() {
    Server::executeAsInDimension("@a[tag=entering_void]", "mygame:void_world", placeDungeonForPlayer());
}

void teleportNearbyToVoid() {
    int close = Entity::exists("@s[distance=..2]");
    if (close == 1) {
        Entity::tag("@s", "entering_void");
        Entity::effect("@s", "minecraft:darkness", 2, 255);
        Entity::effect("@s", "minecraft:levitation", 2, 0);
        Entity::teleportToDimension("@s", "mygame:void_world", 0, 100, 0);
        Server::schedule(schedulePlacement(), 20);
    }
}

@tick
void onTick() {
    int near = Entity::existsAt("@e[tag=portal_center,limit=1]", "@a[distance=..2]");
    if (near == 1) {
        Server::executeAsAt("@a", "@e[tag=portal_center,limit=1]", teleportNearbyToVoid());
    }
}
```

The 20 tick delay before placing the jigsaw structure gives the destination dimension time to load after the player arrives. The darkness and levitation effects hide the generation from the player.

## Generated output

The compiler adds source location comments to the generated `.mcfunction` files to help with debugging.

```
# Generated from function 'onTick' [line 5]

# [line 7]
scoreboard objectives add test__x_0001 dummy
scoreboard players set test test__x_0001 10
```

Scoreboard objective names follow the format `<namespace>__<varname>_<counter>` where the namespace is truncated to 6 characters and the counter is a 4-digit hex value. Minecraft removed the 16-character objective name limit in 1.18, so this truncation may be relaxed in a future compiler version.

Temporary scoreboard entries created during expression evaluation are automatically removed after each statement. Local variable entries are removed when the function exits.

## VS Code extension

A VS Code extension providing syntax highlighting for `.jpack` files is included in the `jpack-extension` directory.

**Installing from the VSIX file**

1. Open VS Code
2. Go to the Extensions view (Ctrl+Shift+X)
3. Click the three-dot menu at the top right of the Extensions panel
4. Select "Install from VSIX..."
5. Navigate to `jpack-extension/jpack-0.0.1.vsix` and select it

**Installing in JetBrains Rider**

1. Open Settings (Ctrl+Alt+S)
2. Navigate to Editor > TextMate Bundles
3. Click the + button
4. Navigate to the `jpack-extension` directory and select it
5. Restart Rider if the extension is not picked up automatically

The extension provides highlighting for keywords, types, annotations, string literals, numeric literals, comments, function names, and namespace identifiers.

## Known limitations

- Multiplication and division between two variables works, but there is no native floating point arithmetic. Float and double values use fixed-point integer arithmetic with potential precision loss on repeated multiplication.
- Recursive functions work but share parameter scoreboard entries, so a function calling itself will overwrite its own parameters before the recursive call completes.
- Classes and structs are parsed and represented in the AST but are not yet implemented in codegen. Enums are similarly parsed but not yet compiled.
- Target selectors containing conditions such as `@a[tag=something]` may cause issues in some contexts due to how curly braces are handled in the macro system. Use `@store_result_intrinsic` for execute-chain selectors.
- `Server::schedule` only works correctly with zero-argument functions or wrapper functions. Arguments passed to the inner function call are discarded at compile time.
- Array writes via index assignment are supported but the `__array_set` macro helper requires the array name to be passed explicitly, which is not yet fully automated.
- The portal dimension transition uses a fixed 20-tick delay for chunk loading which may be insufficient on very slow machines or servers.