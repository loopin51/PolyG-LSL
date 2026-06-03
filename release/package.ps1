<#
.SYNOPSIS
    Build (optional), package, and optionally publish the PolyG_DLL_API Windows app
    as a GitHub Release zip.

.DESCRIPTION
    Run this on the Windows build PC. It collects the Release x64 build output
    (exe + the two device DLLs + the bundled C/C++ runtime DLLs + an end-user README)
    into a single zip that users can download and run on a clean Windows 10+ PC
    without installing a VC++ redistributable (app-local deployment).

    Note: the app cannot be statically linked because ACQPLOT.dll is a prebuilt MFC
    DLL that requires the shared (dynamic) MFC/CRT. Static MFC in the host crashes it.
    So the host links MFC dynamically and the runtime DLLs are shipped alongside.

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

# Stage the app.
$stageName = "PolyG_DLL_API-$Version-win-x64"
$stage = Join-Path ([System.IO.Path]::GetTempPath()) $stageName
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

Copy-Item $payload -Destination $stage
Copy-Item $userDoc -Destination (Join-Path $stage "README.md")

# Bundle the C/C++ runtime DLLs next to the exe (app-local deployment) so the app runs
# on a clean Windows 10+ PC with no VC++ redistributable install. The host exe needs the
# VS2015-2022 runtime (MBCS MFC); the prebuilt ACQPLOT.dll needs the VS2010 runtime. The
# UCRT (ucrtbase.dll / api-ms-win-crt-*) ships with Windows 10+, so it is not bundled.
# Each DLL is taken from release\runtime\ (committed, if present) first, else from
# System32 on this build machine. CI runners usually lack the VS2010 DLLs (mfc100 /
# msvcr100) in System32 -- vendor those in release\runtime\ for a complete CI zip.
$runtimeDir = Join-Path $PSScriptRoot "runtime"
$sys32 = Join-Path $env:WINDIR "System32"
$runtimeDlls = @(
    "mfc140.dll", "msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll",  # host exe (VS2015-2022, MBCS MFC)
    "mfc100.dll", "msvcr100.dll"                                              # ACQPLOT.dll (VS2010)
)
$missing = @()
foreach ($dll in $runtimeDlls) {
    $src = $null
    $vendored = Join-Path $runtimeDir $dll
    if (Test-Path $vendored) { $src = $vendored }
    elseif (Test-Path (Join-Path $sys32 $dll)) { $src = Join-Path $sys32 $dll }
    if ($src) {
        Copy-Item $src -Destination $stage
        Write-Host "    bundled $dll  ($src)" -ForegroundColor DarkGray
    } else {
        $missing += $dll
    }
}
if ($missing.Count -gt 0) {
    Write-Warning ("Runtime DLL(s) not found, NOT bundled: {0}" -f ($missing -join ", "))
    Write-Warning "The zip may crash on a clean PC. Put the missing DLL(s) in release\runtime\"
    Write-Warning "(commit them), or install the matching VC++ redistributable here, then re-run."
} else {
    Write-Host "==> All runtime DLLs bundled." -ForegroundColor Green
}

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
