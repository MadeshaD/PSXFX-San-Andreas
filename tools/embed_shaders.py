from pathlib import Path
p=Path(__file__).resolve().parents[1]/'src'
grid=(p/'grid.hlsl').read_text()
for source,target,name,tag in [('psx.hlsl','shader_source.hpp','kShader','PSXHLSL'),('effects.hlsl','effects_source.hpp','kEffects','PSXFX'),('sprite.hlsl','sprite_source.hpp','kSprite','PSXSPR')]:
 text=(grid if source in ('psx.hlsl','sprite.hlsl') else '')+(p/source).read_text()
 (p/target).write_text(f'static const char {name}[] = R"{tag}(\n'+text+f'){tag}";\n')
