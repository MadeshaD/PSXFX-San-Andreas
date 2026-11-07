"""Inspect SA RenderWare chunk trees read-only; complements analyze_dff.py.
Only known container types are recursed. Skin header interpreted only on geometry extension.
"""
from pathlib import Path
import sys,struct,json,hashlib,collections
p=Path(sys.argv[1]);b=p.read_bytes();counts=collections.Counter();skins=[];strings=[];hanim=[];geom=0
containers={0x10,0x1a,0xf,0x8,0x7,0x6,0xe,0x3,0x14}
def walk(a,z,parent=0):
 global geom
 while a+12<=z:
  typ,n,ver=struct.unpack_from('<III',b,a)
  if typ==0 and n==0 and ver==0:
   assert not any(b[a:z]);break # sector padding
  assert a+12+n<=z,(hex(a),hex(typ),n,z)
  counts[hex(typ)]+=1;c=a+12;end=c+n
  if typ==0xf:geom+=1
  if typ==2 and parent==6:strings.append(b[c:end].split(b'\0')[0].decode('ascii','replace'))
  if typ==0x116:
   assert n>=4
   bones,used,maxweights,pad=b[c:c+4]
   skins.append(dict(offset=hex(a),bytes=n,bones=bones,used_bones=used,max_weights=maxweights))
  if typ==0x11e and n>=12:
   version,node_id,nodes=struct.unpack_from('<3I',b,c)
   if nodes:hanim.append(dict(version=hex(version),node_id=node_id,nodes=nodes))
  if typ in containers:walk(c,end,typ)
  a=end
walk(0,len(b))
print(json.dumps(dict(filename=p.name,sha256=hashlib.sha256(b).hexdigest(),geometry_count=geom,skin_plugins=skins,hanim_roots=hanim,texture_names=sorted(set(filter(None,strings))),chunk_counts=counts),indent=2))
assert p.read_bytes()==b
