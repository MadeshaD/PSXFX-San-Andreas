// Layout of the x86 RenderWare resource entry and instanced mesh records.
// Only inspect them during a verified plain/day-night building callback.
struct BuildingScope { const BYTE* entry; unsigned cursor; };
static BuildingScope buildingScope{};
struct MeshDraw { bool known,indexed; UINT base,start,primitives; };

static const BYTE* ScopedMaterial(const MeshDraw& draw){
 if(!draw.known||!buildingScope.entry||carDepth||internalDraw)return nullptr;
 const BYTE* header=buildingScope.entry+0x18;
 DWORD count;
 memcpy(&count,header+4,sizeof(count));
 if(count==0||count>4096)return nullptr;
 for(DWORD k=0;k<count;++k){
  const DWORD i=(buildingScope.cursor+k)%count;
  const BYTE* mesh=header+0x40+i*0x24;
  DWORD fields[4];memcpy(fields,mesh+0x14,sizeof(fields)); // base, vertices, start, primitives
  if(fields[0]!=draw.base||fields[3]!=draw.primitives||(draw.indexed&&fields[2]!=draw.start))continue;
  const BYTE* material;memcpy(&material,mesh+8,sizeof(material));
  buildingScope.cursor=(i+1)%count;
  if(material){DWORD texture;memcpy(&texture,material,sizeof(texture));if(texture)return material;}
  return nullptr;
 }
 return nullptr;
}
