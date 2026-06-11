# omnetpp-osg-replacement

**Replacing the OpenSceneGraph (OSG) 3D-visualization backend in OMNeT++ and INET with OpenUSD + Hydra.**

This repository holds the **development plan** for retiring the dead/unmaintained
[OpenSceneGraph](http://www.openscenegraph.org/) and
[osgEarth](http://osgearth.org/) dependencies that OMNeT++ 6.x and the INET 4.6
framework use for 3D network visualization, and replacing them — seamlessly,
preserving the existing functionality — with [OpenUSD](https://openusd.org/)
(Pixar's Universal Scene Description) rendered through its **Hydra / Storm** engine.

> This is a **planning + design** repository. It contains the dependency analysis,
> the target architecture, a phased implementation roadmap, an OSG→USD capability
> mapping, the geospatial (osgEarth) replacement strategy, and the risk register.
> Code lands in a later phase, against the OMNeT++ and INET trees.

## Why

- **OSG is effectively dead.** OpenSceneGraph's last stable release predates 2021;
  development has stalled and the project is in maintenance-only/abandonment territory.
  osgEarth still moves but is a niche dependency that is awkward to build and ships an
  enormous transitive footprint. Continuing to depend on OSG means the OMNeT++/INET
  3D-visualization stack will **bit-rot** — broken builds on new compilers, OSes, and
  GPU stacks, with no upstream to fix them.
- **OpenUSD is future-proof.** It is governed by the
  [Alliance for OpenUSD (AOUSD)](https://aousd.org/) (Pixar, Apple, NVIDIA, Adobe,
  Autodesk, …), is Apache-2.0 licensed, has an active release cadence (25.11 made
  Hydra 2 the default; 26.x continues), and ships a production-grade real-time GL
  renderer (**Hydra Storm**) plus an embeddable C++ API (`UsdImagingGLEngine`) that is
  exactly the shape we need to drop into a Qt OpenGL widget.
- **We need the functionality, not OSG/osgEarth specifically.** The 3D viewer, network-node
  models, mobility trails, signal-propagation rings, physical-environment obstacles,
  and the geospatial/satellite scenarios all stay — re-built on USD. **Both OSG and osgEarth
  are removed entirely** (not kept behind a flag); where a sample or osgEarth-dependent feature
  can't be carried over unchanged, it is **re-implemented** on the USD stack. Reduced visual
  fidelity in some re-implemented geospatial scenarios is an accepted tradeoff — simulation
  behavior is unaffected.

## What depends on OSG today (summary)

The full inventory is in [`docs/01-dependency-inventory.md`](docs/01-dependency-inventory.md).
In short:

| Layer | OSG surface | Notes |
|---|---|---|
| **OMNeT++ kernel (public API)** | `cOsgCanvas` (`include/omnetpp/cosgcanvas.h`), `cObjectOsgNode` (`include/omnetpp/osgutil.h`), `cEnvir::refOsgNode/unrefOsgNode` | `osg::Node` only forward-declared in the kernel; the only hard compile dependency is `cObjectOsgNode : osg::Group`, guarded by `WITH_OSG`. |
| **Qtenv viewer** | `IOsgViewer`/`IOsgViewerFactory` (`src/qtenv/iosgviewer.*`) + the runtime-loaded plugin `oppqtenv-osg` (`src/qtenv/osg/`) | **Already an abstraction with a runtime-loaded plugin and a `DummyOsgViewer` fallback** — the cleanest seam for swapping renderers. |
| **INET 4.6** | ~90 files under `src/inet/visualizer/osg/`, plus osgEarth use in `OsgEarthGround` and `OsgGeographicCoordinateSystem` | The dominant consumer. A 3-way `base/` ⁄ `canvas/` ⁄ `osg/` split; `OsgUtils`/`OsgScene` are the chokepoints. |
| **Samples** | `samples/osg-intro`, `osg-earth`, `osg-indoor`, `osg-satellites` | Model-facing idioms: `.osgb` models, `.earth` maps, osgDB pseudo-loader transform URIs in `.ini`. |

## Repository contents

| File | Purpose |
|---|---|
| [`DEVELOPMENT_PLAN.md`](DEVELOPMENT_PLAN.md) | **Start here.** Master plan: goals, strategy decisions, milestones, feature checklist. |
| [`docs/01-dependency-inventory.md`](docs/01-dependency-inventory.md) | Complete, file-level census of OSG/osgEarth usage across OMNeT++, INET, and samples. |
| [`docs/02-architecture.md`](docs/02-architecture.md) | Target architecture: the seam, scene-graph abstraction decision, the `oppqtenv-usd` viewer, picking, build system. |
| [`docs/03-roadmap.md`](docs/03-roadmap.md) | Phased, buildable-at-every-step implementation roadmap (core + INET + geospatial), with effort tiers. |
| [`docs/04-capability-mapping.md`](docs/04-capability-mapping.md) | OSG/osgText/osgDB → OpenUSD/Hydra capability mapping table, flagging the no-clean-mapping cases. |
| [`docs/05-geospatial.md`](docs/05-geospatial.md) | The osgEarth replacement strategy (the hardest part): coordinate systems, terrain, annotations. |
| [`docs/06-risks-and-decisions.md`](docs/06-risks-and-decisions.md) | Risk register with mitigations, honest fidelity gaps, and the open decisions needing sign-off. |
| [`docs/07-plan-review.md`](docs/07-plan-review.md) | Adversarial review of the plan (Fable-model agent), verified against the real source tree; its findings are folded into the docs above. |
| [`docs/INSTALL_DEPENDENCIES.md`](docs/INSTALL_DEPENDENCIES.md) | What to install to build/run the USD viewer (OpenUSD imaging, Qt6, PROJ) — written to fold into OMNeT++'s install process; removes the OSG/osgEarth deps. |

## Relationship to `omnetpp-usdenv`

This is **separate** from the [`omnetpp-usdenv`](https://github.com/tabgab/omnetpp-usdenv)
project. `omnetpp-usdenv` builds a *new* USD-based user environment (a third `Cmdenv`/`Qtenv`
sibling). **This** project replaces the *existing* OSG rendering backend used by Qtenv and
INET. The two share USD-integration know-how (build/packaging, Hydra embedding) but touch
different code and pursue different goals.

## Status

Planning complete; implementation not started. See `DEVELOPMENT_PLAN.md` for the next steps
and the decisions awaiting sign-off in `docs/06-risks-and-decisions.md`.

## License

Plan/design documents in this repo: Apache-2.0 (matching OpenUSD). The implementation will
respect OMNeT++'s Academic Public License and INET's LGPL-3.0 where it touches those trees;
OpenUSD itself is Apache-2.0, compatible with linking from those projects.
