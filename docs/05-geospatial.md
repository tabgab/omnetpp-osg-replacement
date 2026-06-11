# 05 — Geospatial Replacement (osgEarth → OpenUSD)

**Goal: remove osgEarth entirely** and replace its functionality with a native USD + PROJ
stack. osgEarth provides a decade of geospatial infrastructure — SRS/coordinate math, **live
tiled terrain & imagery streaming**, elevation queries, and a rich annotation/feature/
symbology API — and **there is no drop-in USD replacement**. So we build one. Where a
geospatial sample or INET feature cannot be carried over at full fidelity, **it is
re-implemented on the USD stack with documented, accepted fidelity changes** — keeping
osgEarth is not an option.

## 1. What uses osgEarth today (all to be removed)

- **INET** (`#if defined(WITH_OSGEARTH) && defined(INET_WITH_VISUALIZATIONOSG)`):
  - `src/inet/visualizer/osg/scene/SceneOsgEarthVisualizer` — `.earth` → `MapNode` →
    `GeoTransform` → `SimulationScene`; `setViewerStyle(STYLE_EARTH)`; `setEarthViewpoint`.
  - `src/inet/environment/ground/OsgEarthGround` — `ElevationQuery`/`Map`/`GeoPoint` for
    terrain-following mobility.
  - `src/inet/common/geometry/common/GeographicCoordinateSystem` —
    `OsgGeographicCoordinateSystem`: lat/lon/alt ↔ ECEF ↔ scene-local via `MapSRS` + `Matrixd`;
    implements the `IGeographicCoordinateSystem` interface.
- **Samples** (`osg-earth`, `osg-satellites`) use much more: `MapNode`, `GeoTransform`,
  `GeoPoint`, `SpatialReference`, `SkyNode`, annotations (`LabelNode`, `CircleNode`,
  `FeatureNode`, `RectangleNode`, `LocalGeometryNode`), features (`Feature`, `LineString`,
  `MultiGeometry`), symbology (`Style`, `TextSymbol`, `LineSymbol`, `PolygonSymbol`,
  `AltitudeSymbol`), `Util::{EarthManipulator, SkyNode, LinearLineOfSightNode,
  LineOfSightTether}`. `.earth` files use `type=geocentric` + OSM/readymap XYZ tile layers +
  filesystem cache + `<sky>`. **These samples are re-implemented (M11).**

## 2. The committed replacement (M10) — native USD + PROJ

This ships as the product (not optional, not behind an osgEarth fallback):

1. **Coordinate conversion — PROJ 9.x.** `OsgGeographicCoordinateSystem` only needs EPSG:4326
   (geodetic WGS84) ↔ EPSG:4978 (ECEF): `proj_create_crs_to_crs("EPSG:4326","EPSG:4978",…)`,
   identical semantics to osgEarth's `MapSRS`. PROJ is the de-facto open geodesy library,
   actively maintained, packaged everywhere. → **`UsdGeographicCoordinateSystem`** implements
   the existing `IGeographicCoordinateSystem` interface (no interface change), computing the
   scene-origin ENU matrix in plain C++.
2. **GeoTransform → `UsdGeoAnchor`.** Holds a `UsdGeomXform` prim path; `setPosition(lat,lon,
   alt)` → ECEF via PROJ → inverse scene-origin ENU → `xformOp:translate`. Z-up, meters
   (Decision Q8).
3. **Earth sphere (satellite scenarios).** `UsdGeomSphere` r≈6 371 000 m, textured with an
   8K Natural-Earth `UsdUVTexture`; daily rotation = time-sampled `xformOp:rotateZ`. This is
   the **highest-fidelity** case — a textured sphere is most of what osg-satellites needs.
4. **Terrain (ground scenarios).** A `UsdGeomMesh` baked offline from an SRTM/Copernicus tile
   over the simulation area, OSM imagery draped at bake time. Static, fixed resolution.
5. **Elevation → `UsdEarthGround`.** Replaces `OsgEarthGround`; height via vertical ray cast
   against the terrain mesh (CPU BVH/KD-tree). Preserves `computeGroundProjection()` /
   `computeGroundNormal()`.
6. **`SceneOsgEarthVisualizer` → `SceneUsdGeoVisualizer`.** Loads the earth/terrain `.usd`,
   sets `STYLE_EARTH` (→ viewer earth-orbit controller) + earth viewpoint, sets the
   scene-origin locator via PROJ.

## 3. Annotation / feature equivalents

| osgEarth | Replacement | Fidelity |
|---|---|---|
| `Annotation::LabelNode` | viewer HUD text at projected geo position | no auto-declutter |
| `Annotation::CircleNode` (range, terrain-draped) | geo-anchored `UsdGeomBasisCurves` circle | flat — **no terrain drape** |
| `Annotation::FeatureNode`/`LineString` (trails) | `UsdGeomBasisCurves`, points in scene-local via `UsdGeoAnchor` | full for non-draped |
| `Annotation::RectangleNode` (boundary) | `UsdGeomMesh` quad at fixed height | **no clamp** |
| `Util::SkyNode` (atmosphere/lighting) | static skybox sphere + the viewer's explicit lighting | **no time-of-day** |
| `Util::LinearLineOfSightNode`/`Tether` (satellite LOS) | C++ LOS test (segment vs ellipsoid/terrain) + recolored `UsdGeomBasisCurves` | full for ellipsoid; terrain LOS approx |
| feature symbology / **building extrusion** (`boston.earth`) | — | **not provided in v1** — osgEarth-specific |

## 4. Honest fidelity gaps (v1) — accepted tradeoffs

v1 does **not** provide live tiled terrain/imagery streaming, terrain altitude clamping/
draping, label decluttering, CSS-like feature symbology, or `boston.earth` 3D-building
extrusion. Per the project goal, **these are accepted tradeoffs of removing osgEarth**, not
reasons to keep it. **Simulation behavior is never affected — only visuals.**

- *osg-earth (re-implemented):* nodes on a static map/terrain; flat range circles; trails as
  lines without terrain adherence; no buildings.
- *osg-satellites (re-implemented):* textured earth globe with rotation; static skybox; LOS
  lines. High fidelity.
- *INET `SceneOsgEarthVisualizer` users:* migrate to `SceneUsdGeoVisualizer`; live terrain is
  the optional M13 enhancement.

## 5. Build wiring
- `WITH_USD_GEO` (replaces `WITH_OSGEARTH`); guards reimplemented in place when porting
  `GeographicCoordinateSystem`/`OsgEarthGround`. At M12 the `WITH_OSGEARTH` paths are deleted.
- PROJ added as an optional dependency (detected in `configure.in`).
- `.earth` files retired in favor of NED/`.ini` parameters (earth texture, terrain `.usd`,
  origin lat/lon/alt). For M13 a `.earth`-equivalent becomes a 3D-Tiles URL.

## 6. M13 — live tiled terrain (optional later enhancement, **not** a removal prerequisite)
[cesium-native](https://github.com/CesiumGS/cesium-native) (Apache-2.0, C++) streams 3D Tiles
& glTF and provides WGS84/ECEF/ENU math and tile LOD; `vsgCs` shows non-Omniverse C++
integration. Integrate via CMake (static lib / pkg-config) into the `opp_makemake` build;
convert per-tile glTF → `UsdGeomMesh` prims, insert/remove from the scene index on LOD change;
elevation via cesium-native results; config via a 3D-Tiles URL (Cesium Ion or a local tile
server). This restores osgEarth-class terrain fidelity — but the product already ships
**without osgEarth** after M10–M12 regardless of whether M13 is done.
