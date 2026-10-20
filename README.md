Check out the demo here! https://rendererdemo.vercel.app/

This renderer was written as a way to leverage my RHI (https://github.com/omar-yasser-ayoub/RHI) and see how far I can push it, and also see how nice the API is to use.

Its pretty basic its features are: 
- a GLTF importer, which creates all of the resources on the GPU up front (one big texture and one big index buffer for indirect draws, and all of the textures in their appropriate formats). This was made using fastgltf and just parsing the output.
- a basic forward PBR renderer using a Cook–Torrance microfacet BRDF (GGX distribution, Smith geometry, Schlick Fresnel), normal mapping, metallic-roughness materials.
- Shadow mapping using a depth pre-pass on the lights.
- Ambient occlusion.
- A bloom pass at the end to make things a little more pretty!
- A Dear Imgui debug menu to toggle on and off both effects and look at the AO result texture for debugging purposes.

I'm mostly happy about this project because it took me ~5-7 days to cook it up, most of the lore was understanding the rendering techniques rather than writing code. I'm happy I leveraged my RHI for the first time!
