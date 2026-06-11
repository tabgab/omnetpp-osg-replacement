# 05 — Geospatial Replacement (osgEarth → OpenUSD)

The hardest part of the migration. osgEarth provides a decade of geospatial infrastructure —
SRS/coordinate math, **live tiled terrain & imagery streaming**, elevation queries, and a
rich annotation/feature/symbology API. **There is no drop-in USD replacement.** This document
sets the strategy: a pragmatic minimal layer first, with honest fidelity gaps, and tiled
streaming as a deferred option.

## 1. What uses osgEarth today

- **INET** (`#if defined(WITH_OSGEARTH) && defined(INET_WITH_VISUALIZATIONOSG)`):
  - `src/inet/visualizer/osg/scene/SceneOsgEarthVisualizer` — loads a `.earth` file →
    `MapNode` → `GeoTransform` → `SimulationScene`; `setViewerStyle(STYLE_EARTH)`;
    `setEarthViewpoint`.
  - `src/inet/environment/ground/OsgEarthGround` — `ElevationQuery`/`Map`/`GeoPoint` for
    terrain-following mobility.
  - `src/inet/common/geometry/common/GeographicCoordinateSystem` —
    `OsgGeographicCoordinateSystem`: lat/lon/alt ↔ ECEF ↔ scene-local via `MapSRS` + `Matrixd`.
    Implements the `IGeographicCoordinateSystem` interface.
- **Samples** (`osg-earth`, `osg-satellites`) use much more: `MapNode`, `GeoTransform`,
  `GeoPoint`, `SpatialReference`, `SkyNode`, annotations (`LabelNode`, `CircleNode`,
  `FeatureNode`, `RectangleNode`, `LocalGeometryNode`), features (`Feature`, `LineString`,
  `MultiGeometry`), symbology (`Style`, `TextSymbol`, `LineSymbol`, `PolygonSymbol`,
  `AltitudeSymbol` drape/clamp), `Util::{EarthManipulator, SkyNode, LinearLineOfSightNode,
  LineOfSightTether}`. `.earth` files use `type=geocentric` + OSM/readymap XYZ tile layers +
  a filesystem tile cache + `<sky>`.

## 2. Options considered

### (a) Cesium-native / 3D Tiles → USD
[cesium-native](https://github.com/CesiumGS/cesium-native) (Apache-2.0, C++) streams 3D Tiles
& glTF and provides WGS84 ellipsoid math, ECEF↔geographic↔ENU transforms, and tile LOD
selection. It powers the Unreal/Unity/Omniverse integrations; [`vsgCs`](https://github.com/timoore/vsgCs)
shows integration with a non-Omniverse C++ scene graph.
**Verdict:** the right long-term answer for *live* terrain, but heavy — tiles arrive as glTF
that must be converted to USD prims at runtime and inserted/removed from the scene index as
LOD changes, coordinated with sim time. A 3–6 month subsystem. **Deferred to Phase 3 (M13).**

### (b) Static offline USD globe/terrain
Pre-generate a static `.usd` globe or terrain tile from public DEM (SRTM/Copernicus 30 m) +
imagery (OSM, Natural Earth) via GDAL/PDAL/Cesium terrain-builder, referenced at startup.
**Verdict:** great for satellite-orbit views (a textured sphere suffices) and adequate for
ground scenarios where exact elevation is not critical. No streaming, fixed resolution.
**This is the backbone of Phase 1.**

### (c) Minimal custom geospatial layer  — **RECOMMENDED for Phase 1**
Implement exactly what INET needs, no general tile engine:

1. **Coordinate conversion — PROJ 9.x.** `OsgGeographicCoordinateSystem` only needs
   EPSG:4326 (geodetic WGS84) ↔ EPSG:4978 (ECEF). `proj_create_crs_to_crs("EPSG:4326",
   "EPSG:4978", …)` gives identical semantics to osgEarth's `MapSRS`. PROJ is the de-facto
   open geodesy library, actively maintained, packaged everywhere.
   → **`UsdGeographicCoordinateSystem`** implements the existing `IGeographicCoordinateSystem`
   interface (no interface change), computing the scene-origin ENU matrix in plain C++.

2. **GeoTransform → `UsdGeoAnchor`.** A C++ class holding a `UsdGeomXform` prim path;
   `setPosition(lat, lon, alt)` computes ECEF via PROJ, applies the precomputed inverse
   scene-origin ENU matrix, and authors `xformOp:translate`. Mirrors `osgEarth::GeoTransform`
   semantics.

3. **Earth sphere (satellite scenarios).** `UsdGeomSphere` r≈6 371 000 m, textured with an
   8K Natural-Earth `UsdUVTexture`; daily rotation = a time-sampled `xformOp:rotateZ`.

4. **Static terrain (ground scenarios).** A `UsdGeomMesh` baked offline from a single SRTM
   tile over the simulation area, OSM imagery draped at bake time. No live tiles.

5. **Elevation → `UsdEarthGround`.** Replaces `OsgEarthGround`; query height by ray-casting a
   vertical ray against the terrain mesh (CPU BVH/KD-tree over terrain verts). Preserves
   `computeGroundProjection()`/`computeGroundNormal()` semantics.

6. **`SceneOsgEarthVisualizer` → `SceneUsdGeoVisualizer`.** Loads the earth/terrain `.usd`,
   sets `STYLE_EARTH` (→ viewer earth-orbit controller) and the earth viewpoint, sets the
   scene-origin locator via PROJ.

## 3. Annotation / feature equivalents

| osgEarth | Phase-1 replacement | Fidelity |
|---|---|---|
| `Annotation::LabelNode` | viewer HUD text at projected geo position | no auto-declutter |
| `Annotation::CircleNode` (range, terrain-draped) | geo-anchored `UsdGeomBasisCurves` circle | flat — **no terrain drape** |
| `Annotation::FeatureNode`/`LineString` (trails) | `UsdGeomBasisCurves`, points in scene-local via `UsdGeoAnchor` | full for non-draped |
| `Annotation::RectangleNode` (boundary) | `UsdGeomMesh` quad at fixed height | **no clamp** |
| `Util::SkyNode` (atmosphere/lighting) | static skybox sphere with sky/space texture | **no time-of-day** |
| `Util::LinearLineOfSightNode`/`Tether` (satellite LOS) | C++ LOS test (segment vs ellipsoid/terrain) + `UsdGeomBasisCurves` recolored by result | full for ellipsoid; terrain LOS approx |
| feature symbology / **building extrusion** (`boston.earth`) | — | **not provided** — osgEarth-specific |

## 4. Honest fidelity gaps (Phase 1)

Phase 1 explicitly does **not** provide:
- live tiled terrain/imagery streaming (high-res DEM, road networks, 3D buildings);
- terrain altitude clamping/draping for annotations and range circles;
- osgEarth's label decluttering;
- osgEarth's CSS-like feature symbology;
- the `boston.earth` 3D-building extrusion (`feature_geom` + shapefiles) — no plausible USD
  equivalent without a full geospatial feature pipeline.

**Acceptable degraded behavior:**
- *osg-earth scenario:* nodes on a static map/terrain; range circles as flat geometry; trails
  as lines without terrain adherence; no buildings. **Protocol/routing behavior is
  unaffected** — only visuals degrade.
- *osg-satellites scenario:* textured earth globe with rotation; static skybox; LOS lines.
  This is the **highest-fidelity** Phase-1 case (a sphere is most of what's needed).
- *INET `SceneOsgEarthVisualizer` users:* migrate to `SceneUsdGeoVisualizer` understanding
  that live terrain awaits Phase 3.

## 5. Build wiring
- New macro `WITH_USD_GEO` (parallels `WITH_OSGEARTH`); guards rename in place when porting
  `GeographicCoordinateSystem`/`OsgEarthGround`.
- PROJ added as an INET/OMNeT++ optional dependency (detected in `configure.in`).
- `.earth` files retired for Phase 1 in favor of a simple NED/`.ini` parameter set (earth
  texture path, terrain `.usd` path, origin lat/lon/alt); for Phase 3 a `.earth`-equivalent
  becomes a 3D-Tiles URL.

## 6. Phase 3 (M13) — live tiled terrain (deferred)
Integrate cesium-native (CMake → static lib or pkg-config) into the `opp_makemake` build;
convert per-tile glTF → `UsdGeomMesh` prims, insert/remove from the scene index on LOD change;
elevation via cesium-native's results; config via a 3D-Tiles URL (Cesium Ion or a local tile
server). This restores osgEarth-class terrain fidelity on the future-proof substrate.
