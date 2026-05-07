from __future__ import annotations

import math
import struct
from pathlib import Path

from pygltflib import (
    ARRAY_BUFFER,
    ELEMENT_ARRAY_BUFFER,
    FLOAT,
    SCALAR,
    UNSIGNED_SHORT,
    VEC3,
    Accessor,
    Asset,
    Buffer,
    BufferView,
    GLTF2,
    Material,
    Mesh,
    Node,
    PbrMetallicRoughness,
    Primitive,
    Scene,
)


OUT = Path("assets/models")


class ObjWriter:
    def __init__(self) -> None:
        self.vertices: list[tuple[float, float, float]] = []
        self.faces: list[tuple[str, tuple[int, ...]]] = []

    def vertex(self, x: float, y: float, z: float) -> int:
        self.vertices.append((x, y, z))
        return len(self.vertices)

    def face(self, material: str, *indices: int) -> None:
        if len(indices) <= 3:
            self.faces.append((material, tuple(indices)))
            return
        anchor = indices[0]
        for i in range(1, len(indices) - 1):
            self.faces.append((material, (anchor, indices[i], indices[i + 1])))

    def write(self, path: Path) -> None:
        with path.open("w", newline="\n") as f:
            f.write("# Linear Drive custom low-poly creature\n")
            f.write("o Creature_Stalker\n")
            for x, y, z in self.vertices:
                f.write(f"v {x:.6f} {y:.6f} {z:.6f}\n")
            for _material, indices in self.faces:
                f.write("f " + " ".join(str(i) for i in indices) + "\n")

    def write_glb(self, path: Path) -> None:
        positions = b"".join(struct.pack("<3f", *v) for v in self.vertices)
        indices_list = [index - 1 for _material, face in self.faces for index in face]
        indices = b"".join(struct.pack("<H", i) for i in indices_list)

        def pad(blob: bytes) -> bytes:
            return blob + (b"\x00" * ((4 - len(blob) % 4) % 4))

        positions_padded = pad(positions)
        indices_offset = len(positions_padded)
        blob = positions_padded + pad(indices)
        mins = [min(v[i] for v in self.vertices) for i in range(3)]
        maxs = [max(v[i] for v in self.vertices) for i in range(3)]

        gltf = GLTF2(
            asset=Asset(version="2.0", generator="Linear Drive creature generator"),
            buffers=[Buffer(byteLength=len(blob))],
            bufferViews=[
                BufferView(buffer=0, byteOffset=0, byteLength=len(positions), target=ARRAY_BUFFER),
                BufferView(buffer=0, byteOffset=indices_offset, byteLength=len(indices), target=ELEMENT_ARRAY_BUFFER),
            ],
            accessors=[
                Accessor(
                    bufferView=0,
                    byteOffset=0,
                    componentType=FLOAT,
                    count=len(self.vertices),
                    type=VEC3,
                    min=mins,
                    max=maxs,
                ),
                Accessor(
                    bufferView=1,
                    byteOffset=0,
                    componentType=UNSIGNED_SHORT,
                    count=len(indices_list),
                    type=SCALAR,
                    min=[0],
                    max=[max(indices_list)],
                ),
            ],
            materials=[
                Material(
                    name="pale sinew",
                    pbrMetallicRoughness=PbrMetallicRoughness(
                        baseColorFactor=[0.62, 0.68, 0.60, 1.0],
                        roughnessFactor=0.95,
                        metallicFactor=0.0,
                    ),
                )
            ],
            meshes=[Mesh(primitives=[Primitive(attributes={"POSITION": 0}, indices=1, material=0)])],
            nodes=[Node(mesh=0, name="creature_stalker")],
            scenes=[Scene(nodes=[0])],
            scene=0,
        )
        gltf.set_binary_blob(blob)
        gltf.save_binary(path)


def normalize(v: tuple[float, float, float]) -> tuple[float, float, float]:
    length = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) or 1.0
    return (v[0] / length, v[1] / length, v[2] / length)


def cross(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def add_ellipsoid(
    obj: ObjWriter,
    material: str,
    center: tuple[float, float, float],
    radii: tuple[float, float, float],
    rings: int,
    sides: int,
) -> None:
    grid: list[list[int]] = []
    for r in range(rings + 1):
        phi = -math.pi / 2.0 + math.pi * r / rings
        ring: list[int] = []
        for s in range(sides):
            theta = math.tau * s / sides
            x = center[0] + math.cos(phi) * math.cos(theta) * radii[0]
            y = center[1] + math.sin(phi) * radii[1]
            z = center[2] + math.cos(phi) * math.sin(theta) * radii[2]
            ring.append(obj.vertex(x, y, z))
        grid.append(ring)

    for r in range(rings):
        for s in range(sides):
            a = grid[r][s]
            b = grid[r][(s + 1) % sides]
            c = grid[r + 1][(s + 1) % sides]
            d = grid[r + 1][s]
            obj.face(material, a, b, c, d)


def add_limb(
    obj: ObjWriter,
    material: str,
    start: tuple[float, float, float],
    end: tuple[float, float, float],
    r0: float,
    r1: float,
    sides: int = 7,
) -> None:
    axis = normalize((end[0] - start[0], end[1] - start[1], end[2] - start[2]))
    ref = (0.0, 1.0, 0.0) if abs(axis[1]) < 0.92 else (1.0, 0.0, 0.0)
    right = normalize(cross(axis, ref))
    up = normalize(cross(right, axis))
    start_ring: list[int] = []
    end_ring: list[int] = []
    for i in range(sides):
        angle = math.tau * i / sides
        cx = math.cos(angle)
        cy = math.sin(angle)
        sx = right[0] * cx + up[0] * cy
        sy = right[1] * cx + up[1] * cy
        sz = right[2] * cx + up[2] * cy
        start_ring.append(obj.vertex(start[0] + sx * r0, start[1] + sy * r0, start[2] + sz * r0))
        end_ring.append(obj.vertex(end[0] + sx * r1, end[1] + sy * r1, end[2] + sz * r1))
    for i in range(sides):
        obj.face(material, start_ring[i], start_ring[(i + 1) % sides], end_ring[(i + 1) % sides], end_ring[i])
    start_center = obj.vertex(*start)
    end_center = obj.vertex(*end)
    for i in range(sides):
        obj.face(material, start_center, start_ring[(i + 1) % sides], start_ring[i])
        obj.face(material, end_center, end_ring[i], end_ring[(i + 1) % sides])


def add_cone(
    obj: ObjWriter,
    material: str,
    base: tuple[float, float, float],
    tip: tuple[float, float, float],
    radius: float,
    sides: int = 6,
) -> None:
    axis = normalize((tip[0] - base[0], tip[1] - base[1], tip[2] - base[2]))
    ref = (0.0, 1.0, 0.0) if abs(axis[1]) < 0.92 else (1.0, 0.0, 0.0)
    right = normalize(cross(axis, ref))
    up = normalize(cross(right, axis))
    ring = []
    tip_index = obj.vertex(*tip)
    for i in range(sides):
        angle = math.tau * i / sides
        x = base[0] + (right[0] * math.cos(angle) + up[0] * math.sin(angle)) * radius
        y = base[1] + (right[1] * math.cos(angle) + up[1] * math.sin(angle)) * radius
        z = base[2] + (right[2] * math.cos(angle) + up[2] * math.sin(angle)) * radius
        ring.append(obj.vertex(x, y, z))
    for i in range(sides):
        obj.face(material, ring[i], ring[(i + 1) % sides], tip_index)
    base_center = obj.vertex(*base)
    for i in range(sides):
        obj.face(material, base_center, ring[(i + 1) % sides], ring[i])


def build() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    obj = ObjWriter()

    add_ellipsoid(obj, "skin", (0.0, 1.22, -0.08), (0.32, 0.62, 0.22), 6, 10)
    add_ellipsoid(obj, "skin", (0.0, 1.94, -0.16), (0.23, 0.28, 0.18), 5, 9)
    add_ellipsoid(obj, "dark", (0.0, 1.48, 0.12), (0.42, 0.34, 0.12), 4, 9)

    for side in (-1, 1):
        s = float(side)
        add_limb(obj, "skin", (0.20 * s, 1.54, -0.1), (0.68 * s, 0.92, -0.36), 0.075, 0.052)
        add_limb(obj, "skin", (0.68 * s, 0.92, -0.36), (1.18 * s, 0.26, -0.82), 0.055, 0.036)
        add_limb(obj, "skin", (0.15 * s, 0.78, -0.02), (0.36 * s, 0.34, -0.26), 0.088, 0.064)
        add_limb(obj, "skin", (0.36 * s, 0.34, -0.26), (0.58 * s, 0.03, -0.7), 0.064, 0.045)
        for claw in range(3):
            y = 0.2 + (claw - 1) * 0.055
            add_cone(obj, "bone", (1.18 * s, y, -0.82), (1.42 * s, y - 0.05, -1.04), 0.018)

    for i, x in enumerate((-0.22, -0.1, 0.0, 0.1, 0.22)):
        add_cone(obj, "bone", (x, 1.94 + abs(i - 2) * 0.03, -0.12), (x * 1.8, 2.48 - abs(i - 2) * 0.07, -0.38), 0.035)

    for z in (-0.02, 0.08, 0.18):
        add_cone(obj, "dark", (0.0, 1.18, z), (0.0, 1.45, z + 0.28), 0.045)

    obj.write(OUT / "creature_stalker.obj")
    obj.write_glb(OUT / "creature_stalker.glb")
    (OUT / "creature_stalker.mtl").write_text(
        "\n".join(
            [
                "newmtl skin",
                "Kd 0.62 0.68 0.60",
                "Ka 0.04 0.05 0.04",
                "newmtl dark",
                "Kd 0.08 0.10 0.08",
                "Ka 0.01 0.01 0.01",
                "newmtl bone",
                "Kd 0.82 0.80 0.70",
                "Ka 0.05 0.05 0.04",
                "",
            ]
        ),
        newline="\n",
    )


if __name__ == "__main__":
    build()
