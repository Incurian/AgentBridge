# PCG Biome Setup — Hawaii Vegetation Demo

> **Prerequisites:** Unreal Editor open, Tempo server running.
> **Workflow reference:** See [AGENTS.md — PCG Biome Workflow](#) for step-by-step execution.

---

## Objective

Set up a 4-biome PCG system using `BP_PCGBiomeCore` and `BP_PCGBiomeTexture` blueprints.
Each biome maps to one color channel of a shared vegetation texture.
One BiomeCore, four BiomeTextures. All volumes must be landscape-sized.

---

## Biome Definitions

| Biome | Channel | Color (RGBA) | Meshes | Priority |
|-------|---------|--------------|--------|----------|
| **Red** | R | `(R=1,G=0,B=0,A=1)` | SM_PalmPlant, SM_LittleFrondPlant | 1 |
| **Green** | G | `(R=0,G=1,B=0,A=1)` | SM_TallGrassPlant, SM_PoppyPlant | 2 |
| **Blue** | B | `(R=0,G=0,B=1,A=1)` | SM_CactusPlant, SM_SpikeyGrassPlant | 3 |
| **Yellow** | RG | `(R=1,G=1,B=0,A=1)` | SM_BananaPlant, SM_LeafStarPlant | 4 |

---

## Asset Paths

### Templates (source for duplication)

| Asset | Path |
|-------|------|
| Biome Definition | `/CoreAIHawaiiTest/MM_Testing/PCG_Templates/Definition/DefaultBiome` |
| Biome Assets | `/CoreAIHawaiiTest/MM_Testing/PCG_Templates/Assets/DefaultAsset` |
| BiomeCore BP | `BP_PCGBiomeCore` (short name works) |
| BiomeTexture BP | `/CoreAIHawaiiTest/MM_Testing/PCG_Templates/Blueprints/BP_PCGBiomeTexture.BP_PCGBiomeTexture_C` (full path required) |

### Shared Texture

`/CoreAIHawaiiTest/MM_Testing/Textures/Vegetation/HI_Vegetation.HI_Vegetation`

### Default Generator

`/PCGBiomeCore/BiomeGenerators/DefaultGenerator.DefaultGenerator`

### Meshes

All under `/CoreAIHawaiiTest/MM_Testing/Meshes/`:

| Mesh | Full Path |
|------|-----------|
| SM_BananaPlant | `/CoreAIHawaiiTest/MM_Testing/Meshes/SM_BananaPlant.SM_BananaPlant` |
| SM_CactusPlant | `/CoreAIHawaiiTest/MM_Testing/Meshes/SM_CactusPlant.SM_CactusPlant` |
| SM_LeafStarPlant | `/CoreAIHawaiiTest/MM_Testing/Meshes/SM_LeafStarPlant.SM_LeafStarPlant` |
| SM_LittleFrondPlant | `/CoreAIHawaiiTest/MM_Testing/Meshes/SM_LittleFrondPlant.SM_LittleFrondPlant` |
| SM_PalmPlant | `/CoreAIHawaiiTest/MM_Testing/Meshes/SM_PalmPlant.SM_PalmPlant` |
| SM_PoppyPlant | `/CoreAIHawaiiTest/MM_Testing/Meshes/SM_PoppyPlant.SM_PoppyPlant` |
| SM_SpikeyGrassPlant | `/CoreAIHawaiiTest/MM_Testing/Meshes/SM_SpikeyGrassPlant.SM_SpikeyGrassPlant` |
| SM_TallGrassPlant | `/CoreAIHawaiiTest/MM_Testing/Meshes/SM_TallGrassPlant.SM_TallGrassPlant` |

---

## Naming Convention

Duplicated assets and spawned actors should follow this pattern:

| Biome | Definition Asset | Asset Config | Actor Label |
|-------|-----------------|--------------|-------------|
| Red | `RedBiome` | `RedAssets` | `RedBiomeTexture` |
| Green | `GreenBiome` | `GreenAssets` | `GreenBiomeTexture` |
| Blue | `BlueBiome` | `BlueAssets` | `BlueBiomeTexture` |
| Yellow | `YellowBiome` | `YellowAssets` | `YellowBiomeTexture` |

- Definitions go to `/Game/PCGBiomes/Definitions/`
- Assets go to `/Game/PCGBiomes/Assets/`
- All actors placed in outliner folder `PCGBiomes`
- The single BiomeCore actor is labeled `BiomeCore`

---

## Execution

Follow the **PCG Biome Workflow** in `AGENTS.md`, applying the biome table and asset paths above.
The workflow covers: duplicate templates, configure definitions, configure assets, save, get
landscape bounds, spawn actors, fix volumes, assign references, and verify.
