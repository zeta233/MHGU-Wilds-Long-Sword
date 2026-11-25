param([ValidateSet('Recomp','Switch')][string]$Platform = 'Recomp')
$ErrorActionPreference = 'Stop'
$env:ML_RECOMP_RUNTIME = if ($Platform -eq 'Switch') { '0' } else { '1' }
foreach ($name in 'ML_ANIMATION_PREVIEW','ML_ATTACHMENT_PREVIEW','ML_DEBUG_FORCE','ML_TEST_ANY_STYLE','ML_WORLD_AUDIO_PROBE','ML_WORLD_FX_PROBE','ML_ART_FIXTURE','ML_TRACE_EVENTS') {
    Remove-Item "Env:$name" -ErrorAction SilentlyContinue
}
foreach ($name in 'ML_WORLD_COMBAT','ML_WORLD_INPUT_VM','ML_WORLD_PRESENTATION','ML_WILDS_MOVES','ML_ASCENDANCE_PROTOTYPE') {
    Set-Item "Env:$name" 1
}
Push-Location (Join-Path $PSScriptRoot '../integration')
try {
    cargo forge build --release
    if ($LASTEXITCODE -ne 0) { throw "cargo forge failed ($LASTEXITCODE)" }
    Get-FileHash mhw_ls_rebuild.nro
} finally { Pop-Location }
