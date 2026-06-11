# Change Note: `.oppfeatures` — add the `VisualizationUsd` feature

**Target file:** `samples/inet-4.6.0/.oppfeatures`
**Milestone:** M7 (INET helper layer + node visualization)
**Kind:** single insertion (no existing feature is modified)

---

## Insertion point

Insert the new `<feature>` element immediately **after** the closing `/>` of the
`VisualizationOsg` feature element, which ends at **line 1756**:

```
1754        compileFlags = "-DINET_WITH_VISUALIZATIONOSG"
1755        linkerFlags = ""
1756        />
          ← INSERT HERE
1757    <feature
1758        id = "VisualizationCanvasShowcases"
```

---

## Exact text to insert

```xml
    <feature
        id = "VisualizationUsd"
        name = "Visualization USD (3D)"
        description = "Provides network-level 3D visualization features rendered with
                       OpenUSD/Hydra for physical layer, data link layer and network layer
                       communications, and more. Successor of the OSG (3D) visualization."
        initiallyEnabled = "false"
        requires = "PhysicalEnvironment VisualizationCommon"
        recommended = ""
        labels = "information"
        nedPackages = "
                       inet.visualizer.usd
                      "
        extraSourceFolders = ""
        compileFlags = "-DINET_WITH_VISUALIZATIONUSD"
        linkerFlags = ""
        />
```

The element is formatted identically to `VisualizationOsg` (lines 1741–1756): same
attribute order, same 8-space indentation, same multi-line `nedPackages` string style
(opening `"` on the attribute line, package on the next line with 23-space indent,
closing `"` on its own line with 22-space indent).

---

## Rationale for each attribute value

### `requires = "PhysicalEnvironment VisualizationCommon"`

Mirrors `VisualizationOsg` (line 1747) exactly. The USD visualizer tree
(`visualizer/usd/`) subclasses the same renderer-agnostic `base/` classes as the OSG
tree (Decision D3 in `docs/03-roadmap.md`). Both trees depend on `PhysicalEnvironment`
for the physical-layer infrastructure and on `VisualizationCommon` for the shared
`VisualizationBase`/`VisualizationManager` machinery. No additional requires are needed
at this stage.

### `linkerFlags = ""`

Left empty, identical to `VisualizationOsg`. The OpenUSD libraries (`usd_ms`,
`HgiGL`/`HgiMetal`, etc.) are linked through OMNeT++'s `Makefile.inc` `QTENV_LIBS`
variable (item `m1-makefileinc-qtenv`), not through INET-level linker flags. INET only
needs the compile-time guard macro; the actual symbol resolution happens in the qtenv
plugin layer.

### `compileFlags = "-DINET_WITH_VISUALIZATIONUSD"`

This compile guard is the conditional used by all M7/M8 visualizer sources in
`visualizer/usd/`. It follows the same naming pattern as `-DINET_WITH_VISUALIZATIONOSG`
(line 1754). The Increment-1 `util/` skeleton compiles only when the feature (and thus
the source folder's NED package) is enabled.

### `initiallyEnabled = "false"`

Consistent with `VisualizationOsg`. The feature is opt-in; default INET builds are
completely unaffected.

### `nedPackages = "inet.visualizer.usd"`

References the top-level NED package that will contain all USD visualizer modules.
The actual `.ned` files (`package.ned`, module descriptors, etc.) for `inet.visualizer.usd`
arrive with the M7 visualizer module implementation. An enabled feature pointing at a
NED-package with no `.ned` files yet is harmless while `initiallyEnabled = "false"`;
the OMNeT++ NED compiler only resolves package content at runtime when the feature is
actually turned on. This is a **known transitional state** for Increment 1.

---

## What is deliberately NOT added in this change

### `VisualizationUsdShowcases` feature

The analogue of `VisualizationOsgShowcases` (lines 1773–1788) is **not** added here.
The `inet.showcases.visualizer.usd` NED package does not exist yet; adding an
`.oppfeatures` entry for a non-existent showcase package would mislead users who enable
it. The `VisualizationUsdShowcases` feature will be added with M8, when the USD showcase
scenarios land. Reviewers: the asymmetry between `VisualizationUsd` (present) and
`VisualizationUsdShowcases` (absent) is intentional and expected for Increment 1.

---

## Validation checklist

- [ ] **Default build unchanged:** with `VisualizationUsd` left disabled (default),
  a clean `make` of INET produces bit-identical output to the pre-patch build.
- [ ] **File remains well-formed:** the `.oppfeatures` XML-ish syntax is valid; run
  `opp_featuretool list` in the INET source directory and verify the new feature appears
  in the listing without parse errors.
- [ ] **Feature enablement compiles the stub:** after `opp_featuretool enable VisualizationUsd`,
  the `-DINET_WITH_VISUALIZATIONUSD` flag appears in the compiler invocation for sources
  under `visualizer/usd/`.
- [ ] **No existing feature modified:** `git diff` shows only an insertion between lines
  1756 and 1757; no other lines changed.
