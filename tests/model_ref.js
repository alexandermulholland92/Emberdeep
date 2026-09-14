/* Reference dump of the browser build's actor models.
 *
 * Runs the real model builders from emberdeep.html against the same
 * Three.js revision the page loads (r128), then walks each resulting
 * scene graph and prints every part: geometry type and parameters, the
 * world matrix, the surface it is skinned with, and the rig's named
 * joints. The Vita port must reproduce this part list.
 *
 *   node tests/model_ref.js <emberdeep.html> [modelName]
 *
 * Requires three@0.128.0 resolvable from NODE_PATH or node_modules.
 */
'use strict';
const fs = require('fs');

const htmlPath = process.argv[2];
if (!htmlPath) {
  console.error('usage: node tests/model_ref.js <emberdeep.html> [modelName]');
  process.exit(2);
}
const only = process.argv[3] || null;

global.self = global;
const THREE = require('three');
if (THREE.REVISION !== '128') {
  console.error(`warning: expected three r128, got r${THREE.REVISION}`);
}
global.THREE = THREE;

const html = fs.readFileSync(htmlPath, 'utf8');
function slice(a, b) {
  const i = html.indexOf(a), j = html.indexOf(b, i);
  if (i < 0 || j < 0) throw new Error('cannot locate ' + a);
  return html.slice(i, j);
}
/* weapons, buildBiped, the beast builders, the prototype cache, the class
   models and the enemy roster - everything that defines actor geometry */
const modelBlock = slice('function S(m)', '//  PART 4 - GAME');

/* Materials only carry a surface key here; geometry and transforms are what
   we are checking. rand() is seeded so the one cosmetic jitter in
   buildArachnid is reproducible. */
let seed = 1;
const prelude = `
/* instantiate() clones per-instance materials, so the stub needs clone() */
function surfMat(surfKey, colorHex) {
  var m = { __surf: surfKey, __col: colorHex };
  m.clone = function () { return surfMat(surfKey, colorHex); };
  return m;
}
/* a couple of places build a MeshStandardMaterial directly off the surface
   set, so hand back map objects that still carry the surface key */
var __surfSet = new Proxy({}, { get: function (_, key) {
  return { map: { __surf: key }, normalMap: { __surf: key },
           roughnessMap: { __surf: key } };
} });
function buildSurfaces() { return __surfSet; }
function rand(a, b) { seed = (seed * 1103515245 + 12345) & 0x7fffffff;
                      return a + (seed / 0x7fffffff) * (b - a); }
function randInt(a, b) { return Math.floor(rand(a, b + 1)); }
function choose(arr) { return arr[Math.floor(rand(0, arr.length))]; }
`;
const epilogue = `
return { CLASSES: typeof CLASSES !== 'undefined' ? CLASSES : null,
         ENEMIES: typeof ENEMIES !== 'undefined' ? ENEMIES : null,
         BOSSES:  typeof BOSSES  !== 'undefined' ? BOSSES  : null };
`;
const env = new Function('seed', prelude + modelBlock + epilogue)(seed);

/* ---- geometry description, matching Three's constructor parameters ---- */
function geomOf(g) {
  const p = g.parameters || {};
  const t = g.type.replace('Geometry', '');
  const keys = {
    Box: ['width', 'height', 'depth', 'widthSegments', 'heightSegments',
          'depthSegments'],
    Cylinder: ['radiusTop', 'radiusBottom', 'height', 'radialSegments',
               'heightSegments', 'openEnded', 'thetaStart', 'thetaLength'],
    Cone: ['radius', 'height', 'radialSegments', 'heightSegments', 'openEnded',
           'thetaStart', 'thetaLength'],
    Sphere: ['radius', 'widthSegments', 'heightSegments', 'phiStart',
             'phiLength', 'thetaStart', 'thetaLength'],
    Torus: ['radius', 'tube', 'radialSegments', 'tubularSegments', 'arc'],
    Plane: ['width', 'height', 'widthSegments', 'heightSegments'],
    Icosahedron: ['radius', 'detail']
  }[t] || Object.keys(p);
  return { type: t, params: keys.map(k => p[k]) };
}
const r6 = v => Math.abs(v) < 1e-9 ? 0 : +v.toFixed(6);

function dump(name, root) {
  root.updateMatrixWorld(true);

  /* the full node tree with local transforms: the rig animates joints by
     rotating them, so the Vita port needs the hierarchy, not baked world
     matrices. Euler order is recorded because Three composes TRS as
     T * R(order) * S. */
  const nodes = [];
  (function walk(o, parent) {
    const idx = nodes.length;
    const n = {
      name: o.name || null,
      parent: parent,
      pos: [r6(o.position.x), r6(o.position.y), r6(o.position.z)],
      rot: [r6(o.rotation.x), r6(o.rotation.y), r6(o.rotation.z)],
      order: o.rotation.order,
      scale: [r6(o.scale.x), r6(o.scale.y), r6(o.scale.z)],
      world: o.matrixWorld.elements.map(r6)
    };
    if (o.isMesh) {
      const g = geomOf(o.geometry);
      n.geom = g.type;
      n.params = g.params;
      n.surf = (o.material && (o.material.__surf
                || (o.material.map && o.material.map.__surf))) || null;
      /* part colour: surfMat's colorHex, or a directly built material's
         .color, which Three has already converted to linear floats */
      let col = o.material && o.material.__col;
      if (col === undefined && o.material && o.material.color)
        col = o.material.color.getHex();
      n.col = (typeof col === 'number') ? col : null;
    }
    nodes.push(n);
    for (const c of o.children) walk(c, idx);
  })(root, -1);

  const parts = nodes.filter(n => n.geom);
  const rig = root.userData || {};
  const joints = {};
  for (const k of ['body', 'chest', 'headPivot', 'tail']) if (rig[k]) joints[k] = rig[k].name;
  for (const side of ['armR', 'armL'])
    if (rig[side]) joints[side] = ['shoulder', 'elbow', 'hand']
      .map(j => rig[side][j] && rig[side][j].name);
  if (rig.legs) joints.legs = rig.legs.map(L => [L.hip && L.hip.name, L.knee && L.knee.name]);
  if (rig.capes && rig.capes.length) joints.capes = rig.capes.map(c => c && c.name);
  if (rig.wings) joints.wings = rig.wings.map(w => w && w.name);
  return { name, kind: rig.kind || null, partCount: parts.length, nodeCount: nodes.length, joints, nodes };
}

const models = [];
if (env.CLASSES) for (const c of env.CLASSES) models.push([c.key, c.model]);
for (const roster of [env.ENEMIES, env.BOSSES])
  if (roster) for (const k of Object.keys(roster))
    if (roster[k].model) models.push([k, roster[k].model]);

/* --geom: dump the tessellated vertices Three produces for every distinct
   geometry used, so the C tessellator can be diffed against them */
if (process.argv.includes('--geom')) {
  const seen = new Map();
  for (const [, make] of models) {
    const root = make();
    root.traverse(o => {
      if (!o.isMesh) return;
      const g = geomOf(o.geometry);
      const key = g.type + '(' + g.params.join(',') + ')';
      if (seen.has(key)) return;
      const pos = o.geometry.getAttribute('position');
      const uv = o.geometry.getAttribute('uv');
      const idx = o.geometry.getIndex();
      seen.set(key, {
        key, type: g.type, params: g.params,
        vertexCount: pos.count,
        indexCount: idx ? idx.count : 0,
        pos: Array.from(pos.array).map(r6),
        uv: uv ? Array.from(uv.array).map(r6) : [],
        index: idx ? Array.from(idx.array) : []
      });
    });
  }
  process.stdout.write(JSON.stringify([...seen.values()], null, 1) + '\n');
  process.exit(0);
}

const out = [];
for (const [name, make] of models) {
  if (only && name !== only) continue;
  out.push(dump(name, make()));
}
process.stdout.write(JSON.stringify(out, null, 1) + '\n');
