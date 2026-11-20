param(
    [string]$MSBuild='MSBuild.exe',
    [string]$SdkRoot=(Join-Path $PSScriptRoot '..\original-skse-sdk'),
    [switch]$Harness
)
$ErrorActionPreference='Stop'
$SdkRoot=[IO.Path]::GetFullPath($SdkRoot)
if(!(Test-Path -LiteralPath (Join-Path $SdkRoot 'common/Builds/Release/common_vc9.lib'))) {
    & $MSBuild (Join-Path $SdkRoot 'common/common_vc9.vcxproj') /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir="$SdkRoot\common\" /m /v:minimal /nologo
    if($LASTEXITCODE -ne 0) { throw 'Common library build failed' }
}
$project=if($Harness){'Harness.vcxproj'}else{'MfgFixNGLE.vcxproj'}
& $MSBuild (Join-Path $PSScriptRoot $project) /p:Configuration=Release /p:Platform=Win32 /p:SdkRoot="$SdkRoot" /m /v:minimal /nologo
if($LASTEXITCODE -ne 0) { throw 'Port build failed' }
