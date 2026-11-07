# Shader references and adaptation boundaries

Reviewed 2026-10-30. These are independent renderer integrations, not drop-in
D3D9 replacements. Source references were fetched read-only; no external Unity,
Godot, textures or models are bundled. Shared mathematical techniques do not
imply identical lighting, projection conventions or hardware emulation.

## dsoft20 / psx_retroshader

Repository: https://github.com/dsoft20/psx_retroshader
Reviewed commit: d6754070390529d0b50c3b3bef3d087e731e9e45
Shader: https://github.com/dsoft20/psx_retroshader/blob/d6754070390529d0b50c3b3bef3d087e731e9e45/Assets/Shaders/psx-vertexlit.shader
License: MIT, Copyright (c) 2015 dsoft20; retained in THIRD_PARTY_NOTICES.md.

VertexSnapType=1 follows its NDC floor scheme. Original fixed horizontal/vertical
coefficients 160/120 become half our configured virtual width/height; original
clip Z/W are retained. A common 0..1 strength and near-eye guard are added.
Original Unity lighting/fog and distance-based UV weight were NOT ported.

## keijiro / Retro3D

Repository: https://github.com/keijiro/Retro3D
Reviewed commit: 58a1d1f10e8c2eba6fb91664866c5ec901cb4059
Shader: https://github.com/keijiro/Retro3D/blob/58a1d1f10e8c2eba6fb91664866c5ec901cb4059/Assets/Retro3D/Retro3D%20Unlit.shader
README permits use of the shader for any purpose.

Its UV multiplication by clip W followed by fragment division is the classic
perspective-cancellation technique behind AffineType=1. The same technique was
already present in our bounded path. v0.16 removed the cap; v0.17 adds a hard
radial texel cap after the linear blend.

Retro3D quantizes view-space XYZ before projection. VertexSnapType=2 is NOT that
exact algorithm: we independently quantize final clip XY before division by W.
This preserves native skinning, lighting, clip Z/W and avoids guessing bone/MVP
constant registers. Precision is in clip-space units and apparent displacement
falls with W. It is an adaptation of the low-precision geometry idea, not a
faithful simulation of PS1 GTE or a literal Retro3D port.

## GarrettGunnell / Godot-PSX (Acerola)

Repository: https://github.com/GarrettGunnell/Godot-PSX
Reviewed commit: 68c12457303daa1505f8636f00d593aab48a4f29
Shader: https://github.com/GarrettGunnell/Godot-PSX/blob/68c12457303daa1505f8636f00d593aab48a4f29/Shaders/playstation_one.gdshader

Reviewed its normalized-coordinate rounding and W-weighted UV cancellation.
These overlap the existing screen snap/classic affine techniques; we do not add
another author-labelled mode with the same mathematics. No source code copied
or distributed from this repository; it is a research reference only.

## Our retained and additional modes

VertexSnapType=0: existing nearest screen-pixel rounding with D3D Y convention.
AffineType=0: existing smooth radial UV displacement cap measured in texels.
AffineType=2: our selectable homogeneous-weight interpolation, NOT attributed to
a GitHub author and NOT the dsoft20 radial-distance formula. Let B be the
perspective UV, A the interpolated (UV*W), H the interpolated W, t the strength:

  UV = ((1-t)*B + t*A) / ((1-t) + t*H)

This is equivalent to interpolating with per-vertex weights (1-t)+t*W and
normalizing in the fragment shader. At 0 it is perspective-correct; at 1 it is
classic affine. The middle range depends on depth (and on common scaling of W),
unlike the linear blend in type 1. Denominator guards are retained.

No changes to third-party shaders' unrelated lighting/fog/material semantics
are represented as faithful ports. All six selections are implemented in our
own HLSL/native token adapter and share the same INI parameters across supported
model classes.

## v0.17 UI references

- https://www.ppsspp.org/docs/psp-hardware/gpu/raster-pipeline/
  PSP screen XY uses 12.4 fixed point; PSP does have subpixel precision.
- https://psx-spx.consoledev.net/graphicsprocessingunitgpu/
  PS1 integer screen vertex coordinates and affine texture interpolation.
- https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Radar.cpp
- plugin-sdk/plugin_sa/game_sa/rw/rwplcore.h and rwcore.h:
  RwGlobals/RwDevice, 28-byte D3D9 Im2D vertex ABI; checked against local EXE wrappers.

The observed stepped map/icon motion does not prove its hardware cause.
Our independent implementation rounds final Im2D XY to a square grid. It does
not copy a PSP renderer or lower the refresh rate. No third-party source code
was copied for this new UI implementation.
The AffineType=2 formula above is now followed by a hard radial limit of
strength * AffineMaxTexels in base texture texels. Type 1 uses the same cap;
type 0 retains the smooth cap.

## v0.18 resolution and dithering

https://psx-spx.consoledev.net/dev/ps1/gpu/misc/
PS1 4x4 signed offsets, saturate 8-bit RGB then truncate by 8 at 5 bits/channel.
The plugin applies this to the pre-HUD scene composite, NOT per primitive as
hardware does. Other bit depths scale the offsets; that is an artistic extension.

https://github.com/gta-reversed/gta-reversed/blob/master/source/app/app_game.cpp
RenderScene includes camera end/begin cycles; RenderEffects ends in native
CPostEffects::Render. Verified E9 at 0x53E227 to 0x7046E0 in supported EXE.

https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-stretchrect
Color-target seed copy; no depth/stencil stretching is attempted. MSAA bypassed.

D3D9 vtable methods: SetRenderTarget 37, SetDepthStencilSurface 39,
SetViewport 47, SetScissorRect 75; original methods retained. No new third-party
shader source is copied; the matrix and API behavior inform our implementation.
