// TSceneUBO fields — include inside a uniform SceneUBO { ... } block.
// Must stay in sync with Tomos::TSceneUBO (TVkPass.hh).

mat4  viewProj;
mat4  view;
mat4  viewInv;
mat4  projInv;
vec4  cameraPosNear;  // xyz camera pos, w near
vec4  screenFar;      // xy screen px, z far, w lightCount
uvec4 clusterGrid;    // xyz grid dims, w maxLightsPerCluster
uvec4 debug;          // x = TDebugView (0 off, 1 cluster, 2 heatmap, 3 depth)
float time;           // sim elapsed seconds
float _padTime0;
float _padTime1;
float _padTime2;
