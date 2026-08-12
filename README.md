<p align="center">
  <img src="./README/hero.png" alt="Hero" width="100%">
</p>

<p align="center">
  <strong>English</strong> &nbsp;&nbsp; <a href="README/UA.md">Українська</a> &nbsp;&nbsp; <a href="README/RU.md">Русский</a><br>
  <sup>▔▔▔▔&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</sup>
</p>

Pip3D is a fast and lightweight 3D graphics, physics, and spatial audio engine designed specifically for ESP32-S3 microcontrollers and similar platforms, running on top of the PipCore HAL framework. Rendering is fully software-based: frames are generated in internal SRAM using band rendering, while display transfers run in parallel through double-buffered DMA. Physics simulation executes on Core 0 in a separate task alongside rendering without mutual blocking. The engine core includes a custom memory allocator, instantaneous and averaged FPS counters, and a color module with HSV support, dithering, palettes, and Kelvin-based color temperature conversion.

Graphics are powered by a set of specialized software rasterizers (Solid, Smooth, Textured, Billboard, Water) supporting three shading models: Flat, Gouraud, and Blinn-Phong. The engine provides multiple billboard orientations (screen-aligned, Y-axial, fixed-yaw) and perspective-correct texturing with built-in assets (Concrete, Gravel, Tile, Barrier), automatic mipmapping up to 8 levels, and dithering.

The lighting system includes directional and point lights, hemispherical ambient lighting, rim lighting, Blinn-Phong specular highlights, soft planar shadows projected onto the ground via raycasting, and a deferred pass for point lights with culling and Bayer matrix dithering. Environmental features include skyboxes with presets, tiled cloud layers, fog, tone mapping, and a time-of-day controller with realistic sun movement along an arc. Visual effects include a particle engine (fire, smoke, sparks with collisions and bouncing, explosions, trails), sun lens flares with Z-buffer occlusion testing, and an adaptive HUD supporting both Latin and Cyrillic alphabets while automatically adjusting text color to background brightness.

The physics module is based on a rigid-body solver with a fixed simulation step of 1/120 s and up to 16 continuous collision detection (CCD) substeps. Supported collision shapes include spheres, capsules, boxes, cylinders, and convex hulls, with narrow-phase collision detection powered by the GJK and EPA algorithms.

To ensure stable contacts, the solver preserves impulses between frames, performs positional correction, and uses predictive collision tests to prevent objects from tunneling through each other. The engine also supports configurable joints and constraints, as well as Verlet-based rope simulation with friction and collision handling. Built on top of the physics system is a procedural character controller supporting both first-person and third-person views, movement, and procedural walk/run animation.

The engine provides a hierarchical scene graph, mesh instancing, a perspective camera with free-fly and orbital controllers, frustum, backface, and occlusion culling, animation tracks (LINEAR, SMOOTH, EASE), trauma-based procedural camera shake, built-in 3D models (Car, Suzanne, Teapot), and parametric primitives such as icospheres, toruses, capsules, spirals, and knots.

The audio subsystem supports 16-voice 3D spatial sound with attenuation, panning, rear-field dampening, a custom PAC codec, and an FDN reverb processor whose parameters automatically adapt to room geometry using a 10-ray acoustic probe. For debugging, the engine includes a comprehensive 3D gizmo system (lines, rays, arrows, AABBs, OBBs, spheres, capsules, axes, frustums, grids), a modular logger (8 modules, 6 log levels), and frame telemetry tools.

<p align="center">
  <strong>Resources</strong>&emsp;&emsp;&emsp;<strong>Module</strong><br>
  &ensp;&ensp;<a href="https://pisppus.is-a.dev/docs/pip3d">Docs</a>&emsp;&emsp;&emsp;&emsp;&ensp;<a href="https://github.com/pisppus/PipCore">PipCore</a>
</p>

<p align="center">
  Distributed under the MIT License.
</p>