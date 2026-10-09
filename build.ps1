# Build Spider on Windows. See "Building" in README.md.
#
#   .\build.ps1                       spider and cards (the matching decompilation)
#   .\build.ps1 cards                 one target; any ninja target works
#   .\build.ps1 spider_wasm           the browser port, build\wasm\index.html
#   .\build.ps1 clean [target...]     delete build output, keeping downloaded tools
#   .\build.ps1 --orig orig\XPSP3 -y  options before or after targets go to configure.py
#
# Written for Windows PowerShell 5.1: no ternaries, no ??, no && between commands.
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot

function Fail([string]$Message) {
    [Console]::Error.WriteLine("error: $Message")
    exit 1
}

function Show-Usage {
    Get-Content -LiteralPath $PSCommandPath -TotalCount 6 | Select-Object -Skip 2 |
        ForEach-Object { $_ -replace '^# ?', '' }
    @'

Targets:
  spider, cards      link build\XPSP1\<module>\<binary> (default: both)
  spider_wasm        browser app (CMake + Emscripten), build\wasm\index.html
  check, report      byte-compare with gold / objdiff report (also *_spider, *_cards)
  progress           objdiff report, then the progress summary
  configure          re-run configure.py (with the options last used)
  clean              delete build output, keeping build\tools; with targets, only theirs
  spider_mac         macOS only; use build.sh there
'@
}

# Run a native command and stop on a non-zero exit code. dtk and configure.py
# write progress to stderr, which 5.1 would otherwise raise as an error.
function Invoke-Native([string[]]$Command) {
    $ErrorActionPreference = 'Continue'
    $exe = $Command[0]
    $rest = @()
    if ($Command.Count -gt 1) { $rest = $Command[1..($Command.Count - 1)] }
    & $exe @rest
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Prepend $Dir to PATH for this process if it is not already there.
function Add-PathDir([string]$Dir) {
    if (-not $Dir -or -not (Test-Path -LiteralPath $Dir)) { return }
    $parts = $env:PATH -split ';'
    foreach ($p in $parts) {
        if ($p -and [string]::Equals($p, $Dir, [System.StringComparison]::OrdinalIgnoreCase)) { return }
    }
    $env:PATH = $Dir + ';' + $env:PATH
}

# Chocolatey/emsdk --permanent writes the user PATH, but emsdk.ps1 in the same
# session can drop machine PATH entries (CMake lives in Program Files). Look in
# the usual places and put them back on PATH for this process. Source emsdk
# first: emsdk_env.ps1 also rewrites PATH, so cmake is prepended after that.
function Find-WasmTools {
    $emcmake = Get-Command emcmake -ErrorAction SilentlyContinue
    if (-not $emcmake) {
        $emsdk = Join-Path $env:LOCALAPPDATA 'emsdk'
        $envPs1 = Join-Path $emsdk 'emsdk_env.ps1'
        if (Test-Path -LiteralPath $envPs1) {
            $ErrorActionPreference = 'Continue'
            . $envPs1
            $ErrorActionPreference = 'Stop'
            $emcmake = Get-Command emcmake -ErrorAction SilentlyContinue
        }
        if (-not $emcmake) {
            $emDir = Join-Path $emsdk 'upstream\emscripten'
            if (Test-Path -LiteralPath (Join-Path $emDir 'emcmake.bat')) {
                Add-PathDir $emsdk
                Add-PathDir $emDir
                $emcmake = Get-Command emcmake -ErrorAction SilentlyContinue
            }
        }
    }
    if (-not $emcmake) { Fail 'emcmake not found; install and activate emsdk (https://emscripten.org/docs/getting_started)' }

    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) {
        foreach ($p in @(
            "${env:ProgramFiles}\CMake\bin\cmake.exe",
            "${env:ProgramFiles(x86)}\CMake\bin\cmake.exe"
        )) {
            if (Test-Path -LiteralPath $p) {
                Add-PathDir (Split-Path -Parent $p)
                $cmake = Get-Command cmake -ErrorAction SilentlyContinue
                break
            }
        }
    }
    if (-not $cmake) { Fail 'cmake not found (winget install Kitware.CMake)' }
}

$BuildDir = 'build'
$Clean = $false
$Reconfigure = $false
$Progress = $false
$ConfOpts = New-Object System.Collections.Generic.List[string]
$NinjaTargets = New-Object System.Collections.Generic.List[string]
$WasmTargets = New-Object System.Collections.Generic.List[string]
$ValueOptions = @('--orig', '--build-dir', '--dtk', '--objdiff', '--ninja', '-v', '--version')
$OrigDir = ''

for ($i = 0; $i -lt $args.Count; $i++) {
    $a = [string]$args[$i]
    if ($a -in @('-h', '--help', '-?', '/?')) { Show-Usage; exit 0 }
    elseif ($a -eq 'clean') { $Clean = $true }
    elseif ($a -eq 'configure') { $Reconfigure = $true }
    elseif ($a -eq 'progress') { $Progress = $true }
    elseif ($a -eq 'spider_wasm') { $WasmTargets.Add($a) }
    elseif ($a -in $ValueOptions) {
        if ($i + 1 -ge $args.Count) { Fail "$a needs a value" }
        $i++
        $ConfOpts.Add($a)
        $ConfOpts.Add([string]$args[$i])
        if ($a -eq '--build-dir') { $BuildDir = [string]$args[$i] }
        if ($a -eq '--orig') { $OrigDir = [string]$args[$i] }
    }
    elseif ($a.StartsWith('-')) {
        if ($a.StartsWith('--build-dir=')) { $BuildDir = $a.Substring(12) }
        if ($a.StartsWith('--orig=')) { $OrigDir = $a.Substring(7) }
        $ConfOpts.Add($a)
    }
    else { $NinjaTargets.Add($a) }
}

$ModuleDir = Join-Path $BuildDir 'XPSP1'
$WasmDir = Join-Path $BuildDir 'wasm'
if (-not $Clean) {
    foreach ($t in $NinjaTargets) {
        if ($t -in @('spider_mac', 'spider_mac_test')) { Fail "$t builds only on macOS; run build.sh there" }
    }
}

# ---- clean ------------------------------------------------------------------

if ($Clean) {
    if ($NinjaTargets.Count -eq 0) {
        Write-Host "Removing $BuildDir\ (keeping $BuildDir\tools) and the generated ninja files"
        if (Test-Path -LiteralPath $BuildDir) {
            Get-ChildItem -LiteralPath $BuildDir -Force |
                Where-Object { $_.Name -ne 'tools' } |
                Remove-Item -Recurse -Force
        }
        foreach ($p in @('build.ninja', '.ninja_deps', '.ninja_log', 'objdiff.json')) {
            if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Recurse -Force }
        }
    }
    else {
        foreach ($t in ($NinjaTargets + $WasmTargets)) {
            if ($t -in @('spider', 'cards')) { $p = Join-Path $ModuleDir $t }
            elseif ($t -in @('spider_mac', 'spider_mac_test')) { $p = Join-Path $BuildDir 'mac' }
            elseif ($t -eq 'spider_wasm') { $p = $WasmDir }
            else { Fail "clean takes spider, cards, spider_mac, or spider_wasm (got $t)" }
            Write-Host "Removing $p\"
            if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Recurse -Force }
        }
    }
    exit 0
}

# ---- tools ------------------------------------------------------------------

# The Microsoft Store "python" alias is a stub that exits non-zero.
$Python = $null
foreach ($candidate in @(, [string[]]@('python')) + @(, [string[]]@('py', '-3'))) {
    if (Get-Command $candidate[0] -ErrorAction SilentlyContinue) {
        $probe = @()
        if ($candidate.Count -gt 1) { $probe = $candidate[1..($candidate.Count - 1)] }
        $ErrorActionPreference = 'Continue'
        & $candidate[0] @probe -c "import sys; sys.exit(sys.version_info < (3, 8))" 2>$null
        $ErrorActionPreference = 'Stop'
        if ($LASTEXITCODE -eq 0) { $Python = $candidate; break }
    }
}
if (-not $Python) { Fail 'Python 3.8+ not found (winget install Python.Python.3.12)' }

if ($NinjaTargets.Count -eq 0 -and $WasmTargets.Count -eq 0 -and -not $Progress -and -not $Reconfigure) {
    $NinjaTargets.Add('spider')
    $NinjaTargets.Add('cards')
}
if ($Progress) { $NinjaTargets.Add('report') }

# ---- matching decompilation (configure.py + ninja) -------------------------

if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    Fail 'ninja not found on PATH (winget install Ninja-build.Ninja)'
}

# configure.py extracts assets and writes build.ninja. Run it when that has
# not happened yet, when options were given, or after a per-module clean.
$needConfigure = $Reconfigure -or $ConfOpts.Count -gt 0 -or -not (Test-Path -LiteralPath 'build.ninja')
foreach ($m in @('spider', 'cards')) {
    if (-not (Test-Path -LiteralPath (Join-Path (Join-Path $ModuleDir $m) 'assets'))) { $needConfigure = $true }
}

if ($needConfigure) {
    if ($ConfOpts.Count -eq 0 -and (Test-Path -LiteralPath 'build.ninja')) {
        # Keep the options of the last configure (--orig, --allow-nonmatching).
        $line = Select-String -LiteralPath 'build.ninja' -Pattern '^configure_args = (.*)$' | Select-Object -First 1
        if ($line) {
            foreach ($word in ($line.Matches[0].Groups[1].Value -split '\s+')) {
                if ($word) { $ConfOpts.Add($word) }
            }
        }
    }
    if ($NinjaTargets.Count -gt 0) {
        Invoke-Native ($Python + @('tools/check_compiler.py'))
    }
    Invoke-Native ($Python + @('configure.py') + $ConfOpts.ToArray())
}

if ($NinjaTargets.Count -gt 0) {
    Invoke-Native (@('ninja') + $NinjaTargets.ToArray())
}
if ($Progress) {
    Invoke-Native ($Python + @('configure.py', 'progress'))
}

# ---- WASM port (Emscripten + CMake) -----------------------------------------

if ($WasmTargets.Count -gt 0) {
    Find-WasmTools
    # Same spider.exe lookup as build.sh: --orig first, then orig\.
    $SpiderExe = Join-Path $PSScriptRoot 'orig\spider.exe'
    if ($OrigDir -ne '') {
        $candidate = if ([System.IO.Path]::IsPathRooted($OrigDir)) { Join-Path $OrigDir 'spider.exe' } else { Join-Path (Join-Path $PSScriptRoot $OrigDir) 'spider.exe' }
        if (Test-Path -LiteralPath $candidate) { $SpiderExe = $candidate }
    }
    if (-not (Test-Path -LiteralPath $SpiderExe)) { Fail "no spider.exe at $SpiderExe (see orig\README.md)" }
    Invoke-Native (@('emcmake', 'cmake', '-S', 'src/wasm', '-B', $WasmDir, '-DCMAKE_BUILD_TYPE=Release', "-DSPIDER_EXE=$SpiderExe"))
    Invoke-Native (@('cmake', '--build', $WasmDir))
    Write-Host "Built $WasmDir\index.html -- serve it with:"
    Write-Host "  python -m http.server -d $WasmDir"
}
