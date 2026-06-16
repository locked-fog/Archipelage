# Preview Surfaces

Preview surfaces are transient UI stacks anchored to an island. They sit between a compact island and a full expanded surface:

- compact: persistent entry point in the island row
- preview: short-lived or focused secondary surface
- expanded: primary module panel and history/detail view

## Manifest

Plugins register one or more preview layouts in `manifest.json`:

```json
{
  "previewTemplates": [
    {
      "id": "message",
      "component": "MessagePreview.qml",
      "defaultWidth": 380,
      "defaultHeight": 104,
      "timeoutMs": 5000,
      "maxVisible": 5,
      "focusPolicy": "passive"
    }
  ]
}
```

`component` is resolved relative to the plugin directory. `focusPolicy` is `passive` by default; use `request` only for previews that need keyboard focus, such as a password prompt or pairing confirmation.

## Runtime API

Compact and expanded plugin components can declare `property var shellWindow: null`
and call the shell-owned preview API:

```qml
shellWindow.showPreview(moduleId, "message", {
  sourceName: "Mail",
  title: "New message",
  body: "Quarterly update is ready"
})
```

The framework owns lifecycle, stacking, origin geometry, timeout, and focus. The template owns rendering and interprets the payload. Standard payload fields are:

- `sourceName`
- `sourceImage`
- `title`
- `body`
- `items`
- `actions`
- `metadata`

Templates can also receive:

- `payload`
- `instanceId`
- `previewController`
- `shellWindow`

`options.originRect` can point at any shell-local rectangle. If omitted, the preview grows from the module capsule. Pass `shellWindow.originRectForItem(control)` from an expanded panel when a preview should grow from a specific button, row, or sub-control.

The returned instance id can be passed to `shellWindow.dismissPreview(instanceId)`.
Use `shellWindow.dismissPreviewLayout(moduleId, templateId)` to clear one preview stack.

## Stacking

Instances of the same `pluginId/templateId` form one stack. New instances appear at the top, older instances move down, and the oldest visible instance is retired when the stack exceeds `maxVisible`.

Different layouts may overlap. If an existing preview holds focus, passive previews are offset so they do not cover it. A new preview with `focusPolicy: "request"` may take focus.

## Core Package

The core package does not ship preview templates. Notification stacks, connectivity prompts, media previews, and similar surfaces should be provided by external plugins.
