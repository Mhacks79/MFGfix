# Builds deliberate regressions only into the offline harness; restores source
# and rebuilds the clean harness even when a mutant or command fails.
param([string]$MSBuild='MSBuild.exe')
$ErrorActionPreference='Stop'
$enginePath=Join-Path $PSScriptRoot 'src/Engine.cpp'
$corePath=Join-Path $PSScriptRoot 'src/Core.h'
$engineOriginal=[IO.File]::ReadAllText($enginePath)
$coreOriginal=[IO.File]::ReadAllText($corePath)
$results=@()
try {
    foreach($name in @('snap-transition','stuck-expression','stale-load-request','stale-destroyed-face','unchecked-array-count','wrong-expression-source','missing-final-rebuild')) {
        $engine=$engineOriginal
        $core=$coreOriginal
        switch($name) {
            'wrong-expression-source' { $engine=$engine.Replace('return data->keyFrames[1].values + channel-30;', 'return data->keyFrames[0].values + channel-30;') }
            'missing-final-rebuild' { $engine=$engine.Replace('data->unk1A3=1;data->unk1A4=0;', '/* regression: no force edge */') }
            'snap-transition' { $core=$core.Replace('if (duration <= 0.0f) return target;', 'if (duration >= 0.0f) return target;') }
            'stuck-expression' { $engine=$engine.Replace('data->overrideFlag = 0;', 'data->overrideFlag = 1;') }
            'stale-load-request' {
                $engine=$engine.Replace('if(disabled || request.generation!=generation) return;', 'if(disabled) return;')
                $engine=$engine.Replace('if(request.generation!=generation) return;', 'if(false) return;')
            }
            'stale-destroyed-face' { $engine=$engine.Replace('if (state) { *state={}; InterlockedDecrement(&activeStates); }', 'if (false && state) { *state={}; InterlockedDecrement(&activeStates); }') }
            'unchecked-array-count' { $engine=$engine.Replace('key.count != expected[j] || ', '') }
        }
        if($engine -eq $engineOriginal -and $core -eq $coreOriginal) { throw "No mutation applied for $name" }
        [IO.File]::WriteAllText($enginePath,$engine)
        [IO.File]::WriteAllText($corePath,$core)
        & $MSBuild (Join-Path $PSScriptRoot 'Harness.vcxproj') /p:Configuration=Release /p:Platform=Win32 /m /v:quiet /nologo | Out-Null
        if($LASTEXITCODE -ne 0) { throw "Mutant failed to compile: $name" }
        $output=& (Join-Path $PSScriptRoot 'build/tests/harness.exe') 2>&1
        $code=$LASTEXITCODE
        $failed=@($output | Where-Object { $_ -match 'FAIL' })
        $results+=@{mutation=$name;exit_code=$code;failures=$failed}
        if($code -eq 0 -or !$failed.Count) { throw "Regression was not detected: $name" }
        Write-Output "$name : detected ($($failed.Count) failed checks)"
    }
} finally {
    [IO.File]::WriteAllText($enginePath,$engineOriginal)
    [IO.File]::WriteAllText($corePath,$coreOriginal)
    & $MSBuild (Join-Path $PSScriptRoot 'Harness.vcxproj') /p:Configuration=Release /p:Platform=Win32 /m /v:quiet /nologo | Out-Null
    $results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'build/mutation-results.json') -Encoding utf8
}
if($LASTEXITCODE -ne 0) { throw 'Clean harness rebuild failed' }
& (Join-Path $PSScriptRoot 'build/tests/harness.exe')
if($LASTEXITCODE -ne 0) { throw 'Clean harness failed after source restoration' }
