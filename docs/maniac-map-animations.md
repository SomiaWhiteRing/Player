# Maniac map animations (command 11210)

The Maniac path handles the nine-parameter layout emitted by the 241028
`cmdcs.ShowAnimControl` editor. The ordinary RPG_RT path is unchanged.

| Parameter | Meaning |
| --- | --- |
| 0 | Animation operand |
| 1 | Event or picture operand (unused for coordinates) |
| 2 | Bit 0: wait; bit 1: retain source |
| 3 | 0: event; 1: entire map; 2: picture; 3: fixed position; 4: variable binding |
| 4 | Six four-bit value-mode selectors |
| 5 | Buffer operand |
| 6, 7 | X and Y operands |
| 8 | Horizontal flip operand |

Selectors 0–5 apply to animation, target, buffer, X, Y and flip respectively.
Ordinary selectors use constant, variable and indirect-variable evaluation.
Binding coordinates instead use selector 0 for a variable and 1 for an indirect
variable, matching the editor's two-entry selector. Missing tail parameters are
zero. Coordinates and picture positions are screen pixels, not map tiles.

## Playback and lifetime

Each nonnegative buffer has an independent animation instance. Starting an
animation replaces only that buffer. Waiting uses the started animation's
remaining duration and does not wait for unrelated buffers. Animation ID zero
clears the selected buffer. Buffer IDs are sparse, so a large ID does not require
allocating all preceding slots.

Picture targets are resolved by ID on update and draw, using the picture's
current position. Erased pictures retain their last coordinates; the animation
can finish there. Variable bindings are evaluated on update and draw, including
the indirect lookup. Event references are refreshed when map events change; an
effect targeting a deleted event is removed. Map changes clear all buffers.

Completed effects release their resources unless retain-source is enabled.
Retained effects do not update, draw or participate in waiting. Replacing a
retained effect with the same animation keeps its bitmap alive while the new
instance's asynchronous request completes. Clearing a buffer or changing maps
releases retained resources.

An idle buffer does not clear another buffer's flash. Concurrent screen flashes
are applied in buffer order; their exact precedence against the original Maniac
runtime has not been measured.

## Saves and dependency change

Requires liblcf commit `4056dbf533182dd46c0a9b1831da23bc16cc0ae8`
(`Persist Maniac animation buffer state in SaveScreen`) in the independent
`lib/liblcf` repository. It is not tracked as a Player submodule. `SaveScreen` adds an
EasyRPG-only vector field at chunk `0xC8`, declared in
`generator/csv/fields_easyrpg.csv`. Regenerate with `generator/generate.py` and
rebuild liblcf before Player. This extension does not change any original
RPG_RT field and is not a claim of native Maniac save compatibility.

The vector starts with format version 1. Each subsequent 14-int32 record stores:

```text
buffer, animation_id, position_mode, target_id,
x_operand, y_operand, x_mode, y_mode, flip, retain_source,
frame, last_screen_x, last_screen_y, reserved_zero
```

Playback, including retained completed instances, is recreated during graphics
initialization. The ordinary event animation fields remain available for old
saves. Older Player builds ignore the extension and cannot resume these effects.

## Evidence and validation limits

The editor IL establishes field layout, selectors and option bits. Its resource
strings establish the five position modes. The examined game's native runtime
SHA-256 matches the English Maniac 241028 runtime:
`49ba424f3ff1fa7d4fc1334eea272fde4e437d07a6901dc6b87efb95383ec44d`.

The examined 201 maps contain 189 commands: 156 event targets, 21 fixed-position
targets and 12 picture targets; buffer IDs reach 14. These are all represented by
this implementation. Binding mode is supported but absent from that corpus.

Buffer replacement, erased-picture behavior, indirect-binding refresh timing,
zero-animation cancellation and flash precedence are the implementation's
explicit behavior; they have not been compared frame by frame with the native
runtime. Runtime save/load and visual parity require game verification. Existing
character tests do not establish coverage of these new animation modes.
