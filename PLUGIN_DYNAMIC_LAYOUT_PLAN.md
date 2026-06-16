# Plugin Dynamic Layout Plan

## Summary

Archipelago currently gives each island module a mostly static width from user config. A richer plugin such as the media control center needs to adapt its compact width to runtime content and controls. The shell should let plugins publish layout intent, then let the host schedule widths, positions, and visibility with smooth transitions.

This document is a proposal only. It does not require changing existing plugin packages immediately.

## Goals

- Let a plugin request compact width at runtime.
- Let the shell decide the final width based on available space, priority, anchor group, and minimum/maximum constraints.
- Animate width changes and sibling movement instead of snapping.
- Support plugin-driven enabled/hidden state with expand/collapse animation.
- Preserve deterministic behavior when multiple plugins request space at the same time.

## Proposed Plugin Contract

Compact QML components may expose optional properties:

```qml
property int preferredCompactWidth: 180
property int minimumCompactWidth: 44
property int maximumCompactWidth: 320
property bool compactVisible: true
property int layoutPriority: 70
```

Meaning:

- `preferredCompactWidth`: ideal current width for the plugin content.
- `minimumCompactWidth`: smallest useful width before the module should be hidden or collapsed.
- `maximumCompactWidth`: hard cap to avoid one plugin consuming an anchor row.
- `compactVisible`: runtime visibility request, for example media hides itself when there is no active MPRIS player.
- `layoutPriority`: optional runtime override for conflict resolution; manifest/config priority remains the default.

The shell should treat these as requests, not commands. User config and host constraints remain authoritative.

## Host Scheduling Model

For each anchor row:

1. Collect configured modules plus manifest-default modules.
2. Read each mounted compact item’s requested width and visibility.
3. Clamp requested width to `[minimumCompactWidth, maximumCompactWidth]`.
4. If total width fits, assign each module its requested width.
5. If total width does not fit, shrink lower-priority modules toward minimum width first.
6. If the row still does not fit, hide lower-priority modules with `compactVisible=false` or below the compact-level threshold.

The scheduler should produce a stable layout list:

```js
{
  moduleId,
  visible,
  targetWidth,
  targetX,
  targetOpacity,
  collapseReason
}
```

`IslandHost` then renders from this assigned layout instead of relying only on `Row` implicit positioning.

## Animation Model

- Width changes: animate `IslandModule.width` to `targetWidth`.
- Sibling movement: animate each module’s `x` to `targetX`.
- Hide: animate opacity to `0`, width to `0`, then set `visible=false`.
- Show: set `visible=true`, width from `0` to target, opacity from `0` to `1`.
- Expanded surfaces should subscribe to the same geometry revision system already used for masks so transitions remain coherent.

Recommended defaults:

- Width/movement duration: use `ArchipelagoConfig.compactFadeDuration` or introduce `layoutMs`.
- Easing: reuse the existing capsule/expanded bezier curves for visual consistency.

## Implementation Outline

1. Extend `IslandModule` to bind optional compact item properties after the loader creates the plugin item.
2. Add a per-module `layoutRequest` object exposed from `IslandModule` to `IslandHost`.
3. Replace `Row` positioning in `IslandHost` with positioned `Item` containers per anchor so assigned `x` and width can animate.
4. Add a scheduler function in `IslandHost` that recomputes layouts when config, registry, compact level, host width, or any module layout request changes.
5. Keep the current manifest/config behavior as fallback when a plugin does not expose dynamic layout properties.
6. Add QML tests for dynamic width changes, hide/show animation states, priority conflict resolution, and fallback behavior.

## Compatibility

- Existing plugins keep working because all new properties are optional.
- User config `modules.<id>.width` should remain a hard user override unless a future config flag opts into dynamic width.
- Manifest `preferredWidth` should continue to apply to expanded surfaces, not compact width, unless renamed in a future schema.

## Open Questions

- Should user config allow `width: "dynamic"` to opt into plugin-driven compact sizing?
- Should hidden plugin modules still be able to show previews or expanded surfaces?
- Should dynamic layout requests be rate-limited to avoid jitter from frequently changing text width?
- Should the scheduler be implemented in QML first or moved into `ArchipelagoCore` C++ once behavior stabilizes?
