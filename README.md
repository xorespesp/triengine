# Triengine

**Triengine** is a lightweight OpenGL-based 3D data visualization library.  
It uses [Eigen](https://eigen.tuxfamily.org/) for internal math operations, making it convenient for projects that already rely on Eigen.

---

### Features

- Triangle mesh rendering  
- Point cloud rendering  
- Line set rendering  
- Skeletal motion rendering  
- Text rendering support (3D & 2D space)  
- Smoothed Arcball / Fly camera system support  
- Offscreen rendering support  
- Inter-process rendering support (currently, supports Windows OS only; see [`triengine_interop`](#related-repositories))  
- Blinn-Phong lighting model  
- Deferred shading  
- Order-independent transparency  
- HDR tone-mapping  
- Bloom  
- SMAA anti-aliasing  
- OBJ file visualization  
- PLY file visualization  
- BVH file playback / inspection  
- Resource pool optimization  

---

### TODO

- [ ] SMAA T2x (temporal SMAA variant)  
- [ ] Skybox support  
- [ ] Object picking and outlining (planned with `imguigazmo`)  
- [ ] SSAO support  
- [ ] PBR support  
- [ ] Advanced lighting models (Disney Diffuse, Cook-Torrance BRDF, etc.)  
- [ ] Support for various file formats  
- [ ] Frustum culling  
- [ ] Bounding Volume Hierarchy (BVH)  
- [ ] ECS architecture (Entity-Component-System)  

---

### Related Repositories

- [**triengine_interop**](https://github.com/xorespesp/triengine_interop) — the inter-process interop
  layer (IPC transport, rendering wire-protocol, and DX11 shared-surface toolkit) used by inter-process
  rendering (Windows). Shared between the Triengine renderer process (e.g. the `ex04-interprocess`
  example) and clients such as the Flutter interop plugin.

---