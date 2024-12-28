"""
Optimize a dense position-only Wavefront OBJ for rendering/distribution.

Reduces triangle count via pymeshlab Quadric Edge Collapse Decimation and trims
coordinate precision to shrink file size. Normals/UVs are dropped (engine computes
vertex normals).

car_engine.obj was produced from the 119MB Artec raw scan (1,393,353 v / 2,786,826 f)
with: targetfacenum=278682, decimals=6  ->  139,281 v / 278,682 f, 10.15MB

Run (uv, no global install):
    uv run --python 3.11 --with pymeshlab --with numpy \
        python optimize_obj.py <in.obj> <out.obj> [targetfacenum=278682] [decimals=6]
"""
import sys
import pymeshlab as ml


def optimize(in_path: str, out_path: str, target_faces: int, decimals: int) -> None:
    ms = ml.MeshSet()
    ms.load_new_mesh(in_path)
    print(f"[in]  {ms.current_mesh().vertex_number()} v / {ms.current_mesh().face_number()} f")

    # QECD, quality-preserving settings (boundary/normal/planar preservation)
    ms.meshing_decimation_quadric_edge_collapse(
        targetfacenum=target_faces, qualitythr=1.0, optimalplacement=True,
        preservenormal=True, preserveboundary=True, planarquadric=True,
        planarweight=0.001, preservetopology=True, autoclean=True)

    m = ms.current_mesh()
    V, F = m.vertex_matrix(), m.face_matrix()  # (N,3) float, (M,3) int 0-indexed
    print(f"[out] {len(V)} v / {len(F)} f")

    # Write position-only OBJ with trimmed precision (faces are 1-indexed)
    fmt = "{:." + str(decimals) + "f}"
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(f"# optimized by optimize_obj.py\n# {len(V)} vertices\n# {len(F)} faces\n\n")
        f.write("".join("v " + " ".join(fmt.format(c) for c in v) + "\n" for v in V))
        f.write("\n")
        f.write("".join(f"f {a+1} {b+1} {c+1}\n" for a, b, c in F))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    optimize(sys.argv[1], sys.argv[2],
             int(sys.argv[3]) if len(sys.argv) > 3 else 278682,
             int(sys.argv[4]) if len(sys.argv) > 4 else 6)
