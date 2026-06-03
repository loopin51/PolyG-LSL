<#
.SYNOPSIS
    Build (optional), package, and optionally publish the PolyG_DLL_API Windows app
    as a GitHub Release zip.

.DESCRIPTION
    Run this on the Windows build PC. It collects the Release x64 build output
    (statically linked exe + the two device DLLs + an end-user README) into a single
    zip that users can download and run without Visual Studio or a VC++ redistributable.

.PARAMETER Version
    Release tag, e.g. v0.1.0. Used for the zip name and the GitHub release tag.

.PARAMETER Build
    Also run msbuild (Release|x64) before packaging. Requires Visual Studio 2022.

.PARAMETER Publish
    Also create the GitHub release and upload the zip via the gh CLI.
    Commit & push first so the tag points at the right commit.

.EXAMPLE
    # Already built in Visual Studio - just package:
    pwsh release\package.ps1 -Version v0.1.0

.EXAMPLE
    # Build, package, and publish in one go:
    pwsh release\package.ps1 -Version v0.1.0 -Build -Publish
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Version,
    [switch]$Build,
    [switch]$Publish
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$projDir  = Join-Path $repoRoot "PolyG_DLL_API"
$proj     = Join-Path $projDir  "Test_LXSM_D1WD10.vcxproj"
$outDir   = Join-Path $projDir  "Release64bit"
$exe      = Join-Path $outDir   "Test_LXSM_D1WD10.exe"
$userDoc  = Join-Path $PSScriptRoot "README_USER.md"

# Files that must ship next to the exe.
$payload = @(
    $exe,
    (Join-Path $outDir "ACQPLOT.dll"),
    (Join-Path $outDir "LXSM-D1WD10.dll")
)

function Find-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found. Install Visual Studio 2022 with the C++/MFC workload." }
    $path = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
    if (-not $path) { throw "MSBuild.exe not found via vswhere." }
    return $path
}

if ($Build) {
    Write-Host "==> Building Release|x64 ..." -ForegroundColor Cyan
    $msbuild = Find-MSBuild
    & $msbuild $proj /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
    if ($LASTEXITCODE -ne 0) { throw "msbuild failed with exit code $LASTEXITCODE" }
}

# Verify build output exists.
foreach ($f in $payload) {
    if (-not (Test-Path $f)) {
        throw "Missing build artifact: $f`nBuild Release|x64 in Visual Studio (or re-run with -Build) first."
    }
}

# Sanity check: warn if the exe still depends on the dynamic MFC/CRT DLLs
# (i.e. the static-link vcxproj change was not applied / not rebuilt).
$dumpbin = $null
try { $dumpbin = (Get-Command dumpbin.exe -ErrorAction SilentlyContinue).Source } catch {}
if ($dumpbin) {
    $deps = & $dumpbin /dependents $exe 2>$null
    if ($deps -match "MFC\d+\.dll" -or $deps -match "VCRUNTIME\d+\.dll" -or $deps -match "MSVCP\d+\.dll") {
        Write-Warning "exe still imports MFC/VCRUNTIME/MSVCP DLLs - it is NOT statically linked."
        Write-Warning "Confirm UseOfMfc=Static and RuntimeLibrary=MultiThreaded (/MT) for Release|x64, then rebuild."
    } else {
        Write-Host "==> Static-link check passed (no dynamic MFC/CRT imports)." -ForegroundColor Green
    }
}

# Stage and zip.
$stageName = "PolyG_DLL_API-$Version-win-x64"
$stage = Join-Path ([System.IO.Path]::GetTempPath()) $stageName
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

Copy-Item $payload -Destination $stage
Copy-Item $userDoc -Destination (Join-Path $stage "README.md")

$zip = Join-Path $repoRoot "$stageName.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
Write-Host "==> Packaged: $zip" -ForegroundColor Green
Get-ChildItem $stage | Select-Object Name, Length | Format-Table

if ($Publish) {
    Write-Host "==> Publishing GitHub release $Version ..." -ForegroundColor Cyan
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
        throw "gh CLI not found. Install from https://cli.github.com/ and run 'gh auth login'."
    }
    # Release notes are kept in a separate UTF-8 file and passed via --notes-file.
    # This avoids Korean mojibake: Windows PowerShell 5.x reads a BOM-less .ps1 as the
    # ANSI code page (CP949) and also pipes native-command args in that code page, so a
    # Korean here-string inside this script would corrupt. Keeping this .ps1 ASCII-only
    # and letting gh read the UTF-8 file directly sidesteps both problems.
    $notesFile = Join-Path $PSScriptRoot "RELEASE_NOTES.md"
    if (-not (Test-Path $notesFile)) { throw "Release notes file not found: $notesFile" }
    gh release create $Version $zip --title "PolyG_DLL_API $Version" --notes-file $notesFile
    if ($LASTEXITCODE -ne 0) { throw "gh release create failed with exit code $LASTEXITCODE" }
    Write-Host "==> Released." -ForegroundColor Green
}
