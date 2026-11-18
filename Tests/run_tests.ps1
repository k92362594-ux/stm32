param(
    [string]$CompilerRoot = 'D:\Keil_v5\ARM\ARMCC',
    [string]$PythonDependencies = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    & "$CompilerRoot\bin\armcc.exe" --cpu Cortex-M3 --c99 --apcs=interwork -O0 -DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER -I Start -I Library -I System -I Hardware -I User -c Tests\test_modes.c -o Tests\test_modes.o
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
    & "$CompilerRoot\bin\armlink.exe" --cpu Cortex-M3 --entry TestMain --ro_base 0x08000000 --rw_base 0x20000000 --libpath "$CompilerRoot\lib" Tests\test_modes.o -o Tests\test_modes.axf
    if ($LASTEXITCODE -ne 0) { throw 'Test linking failed.' }
    if ($PythonDependencies) { $env:PYTHONPATH = $PythonDependencies }
    python Tests\run_emulated.py Tests\test_modes.axf
    if ($LASTEXITCODE -ne 0) { throw 'Emulated tests failed.' }
}
finally {
    Pop-Location
}
