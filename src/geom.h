/* ==========================================================
   geom.h : tessellation of the primitives the actor models use.

   These are ports of Three.js r128's geometry generators, matching
   vertex order, UV layout and index order exactly, so a model built
   here has the same triangles as the browser build. Verified against
   Three's own output by tests/geom_dump.c - see `make geomcheck`.
   ========================================================== */
#ifndef EMBERDEEP_GEOM_H
#define EMBERDEEP_GEOM_H

enum {
    GEO_BOX,          /* width, height, depth, wSeg, hSeg, dSeg          */
    GEO_CYLINDER,     /* rTop, rBottom, height, radSeg, hSeg, openEnded,
                         thetaStart, thetaLength                         */
    GEO_CONE,         /* radius, height, radSeg, hSeg, openEnded,
                         thetaStart, thetaLength                         */
    GEO_SPHERE,       /* radius, wSeg, hSeg, phiStart, phiLength,
                         thetaStart, thetaLength                         */
    GEO_TORUS,        /* radius, tube, radSeg, tubSeg, arc               */
    GEO_PLANE,        /* width, height, wSeg, hSeg                       */
    GEO_ICOSA,        /* radius, detail                                  */
    GEO_COUNT
};

#define GEO_MAX_PARAMS 8

typedef struct {
    unsigned char kind;
    float p[GEO_MAX_PARAMS];
} GeoSpec;

/* Fill caller-supplied buffers with the tessellation.
   `pos` holds 3 floats per vertex, `uv` 2 floats per vertex, `idx` one
   unsigned short per index (0 indices means the mesh is non-indexed and
   `pos` is already triangle soup, as Three does for polyhedra).
   Returns 0 if the buffers are too small, leaving counts at 0. */
int geo_build(const GeoSpec *g,
              float *pos, float *uv, unsigned short *idx,
              int maxVerts, int maxIdx,
              int *outVerts, int *outIdx);

/* Upper bounds, so callers can size scratch buffers. */
void geo_bounds(const GeoSpec *g, int *maxVerts, int *maxIdx);

#endif
