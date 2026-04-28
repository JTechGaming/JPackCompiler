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
    -v, --version   Print the compiler version.
    -h, --help      Print usage information.

**Example**

```
JPackCompiler -i mygame.jpack -i stdlib.jpack -o C:/minecraft/saves/MyWorld/datapacks/mygame
```

The datapack namespace is derived from the first input file's name. The standard library should therefore be passed after your own source file, even though all files are concatenated before compilation.

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

When a string parameter of an intrinsic receives a function call expression, the compiler extracts the function path at compile time instead of evaluating the call. This is used with `Server::schedule`:

```
Server::schedule(Chat::say("Test"), 40);
```

This compiles to scheduling `namespace:chat/say` to run after 40 ticks. Note that the arguments to the inner call are discarded — only the function path is used. This means the scheduled function must be a zero-argument function or a wrapper function that has the desired behaviour baked in. So the previous example would not work. Instead, you have to do something like this:

```
void announceWinner() {
    Chat::say("The winner has been decided!");
}

Server::schedule(announceWinner(), 200);
```

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

The standard library is provided as `stdlib.jpack` and should be passed as an input file after your own source file.

**World** — setBlock, setBlockMode, fill, setBorderCenter, setBorderDistance, addBorderDistance, setBorderDistanceOverTime, addBorderDistanceOverTime

**Entity** — summon, kill, tag, removeTag, effect, clearEffect, teleport, damage, ride

**Player** — gamemode, give, clear, kick, addExperience, setExperience, addLevels, advancement, recipe, spectate, getX, getY, getZ, getXFloat, getYFloat, getZFloat, getAttribute, getAttributeFloat

**Chat** — say, sayInt, tellRaw, msg, teamMsg, title, subtitle, actionbar

**Sound** — play, stop

**Server** — schedule, gamerule, addObjective, removeObjective, setScore, getScore, addScore, removeScore

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

## Generated output

The compiler adds source location comments to the generated `.mcfunction` files to help with debugging.

```
# Generated from function 'onTick' [line 5]

# [line 7]
scoreboard objectives add test__00000001 dummy
scoreboard players set test test__00000001 10
```

Scoreboard objective names follow the format `<namespace><uuid>` where the namespace is truncated to 6 characters and the UUID is an 8-digit hex counter. Minecraft removed the 16-character objective name limit in 1.18, so this truncation may be relaxed in a future compiler version.

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
- The `@event` annotation for advancement-based event triggers is not yet implemented.
- Recursive functions work but share parameter scoreboard entries, so a function calling itself will overwrite its own parameters before the recursive call completes.
- Classes and structs are parsed and represented in the AST but are not yet implemented in codegen. Enums are similarly parsed but not yet compiled.
- Target selectors containing conditions such as `@a[tag=something]` may not work correctly in all cases due to how curly braces are handled in string interpolation.
- `Server::schedule` only works correctly with zero-argument functions or wrapper functions. Arguments passed to the inner function call are discarded at compile time.