"""Read-only analysis of non-native SA DFF geometry; no file is modified.
Usage: python3 tests/analyze_dff.py /path/to/cuntsrod09.dff
"""
import struct,pathlib,sys,json,hashlib
import numpy as np
path=pathlib.Path(sys.argv[1]);data=path.read_bytes()
def chunks(begin,end):
 while begin+12<=end:
  typ,n,ver=struct.unpack_from('<III',data,begin)
  assert begin+12+n<=end
  yield typ,begin+12,begin+12+n,ver
  begin+=12+n
def find_geometry(begin,end):
 for typ,a,b,v in chunks(begin,end):
  if typ==0xf:yield a,b
  elif typ in (0x10,0x1a):yield from find_geometry(a,b)
reports=[]
for ga,gb in find_geometry(0,len(data)):
 _,a,end,version=next(c for c in chunks(ga,gb) if c[0]==1)
 assert version==0x1803ffff,'Parser scoped to attached SA stream version'
 flags,nt,nv,nm=struct.unpack_from('<4I',data,a);a+=16
 assert not flags&0x01000000,'Native geometry unsupported'
 assert nm>=1
 if flags&8:a+=nv*4
 nuv=(flags>>16)&255
 if nuv==0:nuv=2 if flags&0x80 else (1 if flags&4 else 0)
 uvs=[]
 for _ in range(nuv):
  uvs.append(np.frombuffer(data,'<f4',nv*2,a).reshape(nv,2));a+=nv*8
 tri=np.frombuffer(data,'<u2',nt*4,a).reshape(nt,4);a+=nt*8
 idx=tri[:,[1,0,3]].astype(int);assert idx.max()<nv
 a+=16;hasv,hasn=struct.unpack_from('<II',data,a);a+=8;assert hasv
 verts=np.frombuffer(data,'<f4',nv*3,a).reshape(nv,3);a+=nv*12
 if hasn:a+=nv*12
 assert a<=end
 edges=np.stack([np.linalg.norm(verts[idx[:,i]]-verts[idx[:,(i+1)%3]],axis=1) for i in range(3)],1)
 area=np.linalg.norm(np.cross(verts[idx[:,1]]-verts[idx[:,0]],verts[idx[:,2]]-verts[idx[:,0]]),axis=1)*.5
 reports.append(dict(vertices=nv,triangles=nt,material_indices=sorted(set(map(int,tri[:,2]))),
  morph_targets=nm,uv_sets=nuv,has_normals=bool(hasn),
  bounds_min=verts.min(0).tolist(),bounds_max=verts.max(0).tolist(),
  dimensions=(verts.max(0)-verts.min(0)).tolist(),longest_edge=float(edges.max()),
  largest_triangle_area=float(area.max()),uv_min=uvs[0].min(0).tolist(),uv_max=uvs[0].max(0).tolist()))
assert reports
print(json.dumps(dict(filename=path.name,sha256=hashlib.sha256(data).hexdigest(),size_bytes=len(data),geometry=reports),indent=2))
assert path.read_bytes()==data,'Input changed during analysis'
