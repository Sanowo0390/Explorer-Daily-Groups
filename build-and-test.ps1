param(
    [string]$WindhawkRoot = (Join-Path $env:ProgramFiles 'Windhawk'),
    [string]$EngineVersion = '1.7.3',
    [string]$BuildDirectory = (Join-Path $PSScriptRoot 'build')
)
$ErrorActionPreference = 'Stop'
$compiler = Join-Path $WindhawkRoot 'Compiler\bin\clang++.exe'
$library = Join-Path $WindhawkRoot "Engine\$EngineVersion\64\windhawk.lib"
if (!(Test-Path -LiteralPath $compiler) -or !(Test-Path -LiteralPath $library)) {
    throw 'Windhawk compiler/64-bit import library not found. Set WindhawkRoot and EngineVersion.'
}
New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$oldTemp = $env:TEMP; $oldTmp = $env:TMP
try {
    $env:TEMP = $BuildDirectory; $env:TMP = $BuildDirectory
    $common = @('--target=x86_64-w64-mingw32','-std=c++23','-Wall','-Wextra')
    & $compiler @common '-O2' '-shared' '-DWH_MOD' '-DUNICODE' '-D_UNICODE' `
        '-DWH_MOD_ID=L"explorer-daily-groups"' '-DWH_MOD_VERSION=L"0.1.1"' `
        '-include' 'windhawk_api.h' (Join-Path $PSScriptRoot 'explorer-daily-groups.wh.cpp') `
        $library '-lole32' '-lpropsys' '-lversion' '-luuid' `
        '-o' (Join-Path $BuildDirectory 'compile-check.dll')
    if ($LASTEXITCODE -ne 0) { throw 'Mod compile/link check failed.' }
    Write-Output 'PASS: x64 Mod compile/link check. Not installed or loaded.'
    & $compiler @common '-static' (Join-Path $PSScriptRoot 'tests.cpp') '-lole32' `
        '-o' (Join-Path $BuildDirectory 'tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Core test build failed.' }
    & (Join-Path $BuildDirectory 'tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed.' }
    & $compiler @common '-static' '-DWH_EDITING' '-DWH_MOD' '-DUNICODE' '-D_UNICODE' `
        '-DWH_MOD_ID=L"explorer-daily-groups"' (Join-Path $PSScriptRoot 'hook-tests.cpp') `
        '-lole32' '-lpropsys' '-lversion' '-luuid' '-o' (Join-Path $BuildDirectory 'hook-tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Hook logic test build failed.' }
    & (Join-Path $BuildDirectory 'hook-tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Hook logic tests failed.' }
    Write-Output 'Explorer injection, UI, sorting, collapse and stability remain UNTESTED.'
} finally {
    $env:TEMP = $oldTemp; $env:TMP = $oldTmp
}
