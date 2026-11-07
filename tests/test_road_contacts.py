"""Read a real DFF boundary, synthesize a neighboring model with a split edge."""
import contextlib,io,runpy,subprocess,sys,tempfile
from pathlib import Path
import numpy as np
root=Path(__file__).resolve().parents[1]
with contextlib.redirect_stdout(io.StringIO()):
    geo=runpy.run_path(str(root/'tests/analyze_dff.py'))
v,tri=geo['verts'],geo['idx'];counts={};lines={}
for face in tri:
    for j in range(3):
        a,b,c=v[face[[j,(j+1)%3,(j+2)%3]]]
        key=tuple(sorted([tuple(a),tuple(b)]))
        counts[key]=counts.get(key,0)+1
        lines[key]=np.concatenate([a,b,c])
edges=[line for key,line in lines.items() if counts[key]==1 and
       np.linalg.norm(np.cross(line[3:6]-line[:3],line[6:]-line[:3]))>.1 and
       np.linalg.norm(line[3:6]-line[:3])>1]
assert edges
with tempfile.TemporaryDirectory() as tmp:
    exe=Path(tmp)/'contacts'
    subprocess.run(['g++','-std=c++17','-O2',str(root/'tests/test_world_contact.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],input='\n'.join(' '.join(map(str,x)) for x in edges),text=True,check=True)
print(f'PASS: {len(edges)} boundary edges from {Path(sys.argv[1]).name}, each matched against a synthetic split neighbor. Same-side, depth-separated, crossing and point-only contacts rejected.')
