$ErrorActionPreference = 'Stop'

New-Item -ItemType Directory -Force -Path assets/source, assets/models, assets/audio, assets/textures/icons | Out-Null

$downloads = @(
  @{ Uri='https://kenney.nl/media/pages/assets/nature-kit/8334871c74-1677698939/kenney_nature-kit.zip'; Out='assets/source/kenney_nature-kit.zip' },
  @{ Uri='https://kenney.nl/media/pages/assets/car-kit/a9b1e99e92-1775131960/kenney_car-kit.zip'; Out='assets/source/kenney_car-kit.zip' },
  @{ Uri='https://kenney.nl/media/pages/assets/interface-sounds/d23a84242e-1677589452/kenney_interface-sounds.zip'; Out='assets/source/kenney_interface-sounds.zip' },
  @{ Uri='https://opengameart.org/sites/default/files/Kenney_gameIcons.zip'; Out='assets/source/Kenney_gameIcons.zip' },
  @{ Uri='https://opengameart.org/sites/default/files/ScatterNoise1.mp3'; Out='assets/source/ScatterNoise1.mp3' },
  @{ Uri='https://opengameart.org/sites/default/files/monster.obj'; Out='assets/source/opengameart_koe-kto_monster.obj' },
  @{ Uri='https://opengameart.org/sites/default/files/Animated%20Monster%20Pack%20by%20%40Quaternius.zip'; Out='assets/source/quaternius_animated_monster_pack.zip' }
)

foreach ($item in $downloads) {
  Invoke-WebRequest -Uri $item.Uri -OutFile $item.Out
}

Add-Type -AssemblyName System.IO.Compression.FileSystem

function Extract-Entry($zipPath, $entryName, $outPath) {
  $zip = [IO.Compression.ZipFile]::OpenRead((Resolve-Path $zipPath))
  try {
    $entry = $zip.Entries | Where-Object { $_.FullName -eq $entryName } | Select-Object -First 1
    if (-not $entry) { throw "Missing $entryName in $zipPath" }
    $dest = Join-Path (Get-Location) $outPath
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dest, $true)
  } finally {
    $zip.Dispose()
  }
}

$natureModels = @(
  'tree_default_dark','tree_cone_dark','tree_fat_darkh','rock_largeA','log_large','fence_gate',
  'tree_oak_dark','tree_pineTallA','tree_pineRoundB','plant_bushDetailed','plant_bushLarge',
  'grass_large','stump_oldTall','stump_roundDetailed','sign','ground_pathRocks','bridge_side_wood',
  'fence_planksDouble','log_stackLarge'
)
foreach ($name in $natureModels) {
  Extract-Entry 'assets/source/kenney_nature-kit.zip' "Models/OBJ format/$name.obj" "assets/models/$name.obj"
  Extract-Entry 'assets/source/kenney_nature-kit.zip' "Models/OBJ format/$name.mtl" "assets/models/$name.mtl"
}

$models = @(
  @{zip='assets/source/kenney_car-kit.zip'; entry='Models/GLB format/sedan.glb'; out='assets/models/sedan.glb'},
  @{zip='assets/source/kenney_car-kit.zip'; entry='Models/GLB format/debris-door.glb'; out='assets/models/debris-door.glb'},
  @{zip='assets/source/kenney_car-kit.zip'; entry='Models/GLB format/debris-tire.glb'; out='assets/models/debris-tire.glb'}
)
foreach ($m in $models) { Extract-Entry $m.zip $m.entry $m.out }
Extract-Entry 'assets/source/kenney_car-kit.zip' 'Models/GLB format/Textures/colormap.png' 'assets/models/Textures/colormap.png'

$sounds = @(
  @{entry='Audio/click_002.ogg'; out='assets/audio/click_002.ogg'},
  @{entry='Audio/glitch_003.ogg'; out='assets/audio/glitch_003.ogg'},
  @{entry='Audio/error_006.ogg'; out='assets/audio/error_006.ogg'},
  @{entry='Audio/glass_003.ogg'; out='assets/audio/glass_003.ogg'},
  @{entry='Audio/scratch_004.ogg'; out='assets/audio/scratch_004.ogg'},
  @{entry='Audio/drop_003.ogg'; out='assets/audio/drop_003.ogg'}
)
foreach ($s in $sounds) { Extract-Entry 'assets/source/kenney_interface-sounds.zip' $s.entry $s.out }

$ffmpeg = (Get-Command ffmpeg -ErrorAction SilentlyContinue).Source
if ($ffmpeg) {
  Get-ChildItem assets/audio/*.ogg | ForEach-Object {
    $out = [IO.Path]::ChangeExtension($_.FullName, '.wav')
    & $ffmpeg -y -loglevel error -i $_.FullName -ac 1 -ar 44100 -sample_fmt s16 $out
  }
} else {
  Write-Warning 'ffmpeg not found; runtime expects WAV conversions for interface sounds.'
}

$icons = @(
  @{entry='PNG/White/2x/locked.png'; out='assets/textures/icons/locked.png'},
  @{entry='PNG/White/2x/power.png'; out='assets/textures/icons/power.png'},
  @{entry='PNG/White/2x/warning.png'; out='assets/textures/icons/warning.png'},
  @{entry='PNG/White/2x/audioOn.png'; out='assets/textures/icons/audioOn.png'},
  @{entry='PNG/White/2x/gear.png'; out='assets/textures/icons/gear.png'}
)
foreach ($i in $icons) { Extract-Entry 'assets/source/Kenney_gameIcons.zip' $i.entry $i.out }

Copy-Item -LiteralPath 'assets/source/ScatterNoise1.mp3' -Destination 'assets/audio/radio_static.mp3' -Force

@'
from pathlib import Path
src = Path('assets/source/opengameart_koe-kto_monster.obj')
out = Path('assets/models/oga_monster_normalized.obj')
verts = []
lines = []
for raw in src.read_text(errors='ignore').splitlines():
    if raw.startswith('v '):
        parts = raw.split()
        x, y, z = map(float, parts[1:4])
        verts.append((x, y, z))
        lines.append(('v', len(verts)-1, raw))
    elif raw.startswith('usemtl'):
        continue
    else:
        lines.append(('line', raw))
mins = [min(v[i] for v in verts) for i in range(3)]
maxs = [max(v[i] for v in verts) for i in range(3)]
center = [(mins[i] + maxs[i]) * 0.5 for i in range(3)]
scale = 2.05 / (maxs[1] - mins[1])
with out.open('w', newline='\n') as f:
    f.write('# Normalized CC0 monster from OpenGameArt monster-3 by koe-kto\n')
    f.write('o OGA_KoeKto_Monster_Normalized\n')
    for item in lines:
        if item[0] == 'v':
            _, idx, _raw = item
            x, y, z = verts[idx]
            f.write(f'v {(x-center[0])*scale:.6f} {(y-mins[1])*scale:.6f} {-(z-center[2])*scale:.6f}\n')
        elif item[0] == 'line':
            raw = item[1]
            if raw.startswith('#') or raw.startswith('o '):
                continue
            f.write(raw + '\n')
'@ | python -

python tools/generate_creature_asset.py

foreach ($name in @('Skeleton','Bat')) {
  Extract-Entry 'assets/source/quaternius_animated_monster_pack.zip' "Animated Monster Pack by @Quaternius/OBJ/$name.obj" "assets/models/quaternius_$($name.ToLower()).obj"
  Extract-Entry 'assets/source/quaternius_animated_monster_pack.zip' "Animated Monster Pack by @Quaternius/OBJ/$name.mtl" "assets/models/quaternius_$($name.ToLower()).mtl"
}

@'
from pathlib import Path
for name in ['skeleton','bat']:
    src = Path(f'assets/models/quaternius_{name}.obj')
    dst = Path(f'assets/models/quaternius_{name}_tri.obj')
    out = []
    for line in src.read_text(errors='ignore').splitlines():
        if line.startswith('f '):
            parts = line.split()[1:]
            if len(parts) <= 3:
                out.append(line)
            else:
                for i in range(1, len(parts) - 1):
                    out.append('f ' + ' '.join([parts[0], parts[i], parts[i + 1]]))
        else:
            out.append(line)
    dst.write_text('\n'.join(out) + '\n', newline='\n')
'@ | python -
