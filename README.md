# PSXFX San Andreas

**PSXFX** is a plugin that recreates the signature look of the original PlayStation’s 3D graphics in GTA San Andreas. It changes how geometry, textures, color, and the interface are rendered while you play. The original models and textures remain untouched.

## Key features
- **Internal resolution.** `RenderHeight` sets the height at which the 3D scene is rendered. You can keep the game’s display resolution and configure interface filtering and movement snapping separately.
- **Geometry and texture coordinates.** `VertexSnap` locks vertices to a virtual grid, creating the characteristic wobble along object edges. `Affine` recreates texture distortion as the viewing angle changes. You can also limit UV coordinate precision and enable affine interpolation of vertex colors.
- **Support for in-game models.** Effects apply to supported world models, objects, vehicles, and characters, including CJ.
- **Texture pixelation.** Choose between reduced texture copies stored in video memory and screen-space grid pixelation. Point filtering keeps pixel edges sharp. World sprites and visual effects have separate settings.
- **Color and dithering.** Set color depth from 2 to 8 bits per channel, choose the PS1 or Bayer dithering matrix, and adjust pattern strength, saturation, and contrast.
- **HUD, radar, and other interface elements.** `HUDPointFilter` disables smoothing when sprites are enlarged. UISnap aligns their coordinates to a grid, giving some interface elements a stepped movement.
- **Fog, lighting, and world effects.** Adjust fog distance, ambient and directional light, pedestrian shadows, and vehicle brightness. You can also enable or disable stencil shadows, vehicle reflections and shine, heat haze, high-speed motion blur, and procedural grass.

## Controls
By default, F7 toggles the effects, and F8 reloads the INI while the game is running.

## Compatibility
The plugin is incompatible with SkyGfx. If you have the latest version of WidescreenFix installed, disable SMAA in its INI by setting `SMAA = 0`.

## Note
This plugin was made entirely with AI.
