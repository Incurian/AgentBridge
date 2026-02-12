# PCG Biome Demo

Set up 4 biomes using the PCG Biome Workflow in AGENTS.md. One BiomeCore, four BiomeTextures.

## Biomes

| Biome | Color | Meshes | Priority |
|-------|-------|--------|----------|
| Red | `(R=1,G=0,B=0,A=1)` | SM_PalmPlant, SM_LittleFrondPlant | 1 |
| Green | `(R=0,G=1,B=0,A=1)` | SM_TallGrassPlant, SM_PoppyPlant | 2 |
| Blue | `(R=0,G=0,B=1,A=1)` | SM_CactusPlant, SM_SpikeyGrassPlant | 3 |
| Yellow | `(R=1,G=1,B=0,A=1)` | SM_BananaPlant, SM_LeafStarPlant | 4 |

## Paths

| What | Path |
|------|------|
| Definition template | `/CoreAIHawaiiTest/MM_Testing/PCG_Templates/Definition/DefaultBiome` |
| Asset template | `/CoreAIHawaiiTest/MM_Testing/PCG_Templates/Assets/DefaultAsset` |
| BiomeCore class | `BP_PCGBiomeCore` |
| BiomeTexture class | `/CoreAIHawaiiTest/MM_Testing/PCG_Templates/Blueprints/BP_PCGBiomeTexture.BP_PCGBiomeTexture_C` |
| Texture | `/CoreAIHawaiiTest/MM_Testing/Textures/Vegetation/HI_Vegetation.HI_Vegetation` |
| Generator | `/PCGBiomeCore/BiomeGenerators/DefaultGenerator.DefaultGenerator` |
| Meshes | `/CoreAIHawaiiTest/MM_Testing/Meshes/<name>.<name>` |
