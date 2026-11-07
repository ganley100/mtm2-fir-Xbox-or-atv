<#
.SYNOPSIS
  Make the Xbox Dev Mode package (.msix) from what build.ps1 built, signed
  with a self-signed certificate made on first use.

.EXAMPLE
  xbox\package.ps1                         # the full game, no game files inside
  xbox\package.ps1 -Game game              # with your game\ folder inside the package
  xbox\package.ps1 -Variant demo -Game game-demo

  Output in xbox\out\: the .msix, the certificate (.cer) to install it with,
  and Microsoft.VCLibs.x64.14.00.appx, the C++ runtime it needs. docs\xbox.md
  has the Device Portal steps.
#>
param(
  [ValidateSet('full', 'demo')] [string] $Variant = 'full',
  [string] $Game,
  [string] $Version = '0.1.0.0'
)
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$built = "$here\build\$Variant\mtm"
$layout = "$here\build\layout-$Variant"
$outDir = "$here\out"
foreach ($f in 'mtm.exe', 'SDL2.dll') {
  if (-not (Test-Path "$built\$f")) { throw "no $built\${f}: run xbox\build.ps1 -Variant $Variant first" }
}

$kits = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
$sdkbin = Get-ChildItem $kits -Directory | Where-Object { Test-Path "$($_.FullName)\x64\makeappx.exe" } |
  Sort-Object Name -Descending | Select-Object -First 1
if (-not $sdkbin) { throw 'makeappx.exe not found: install the Windows 10/11 SDK' }
$makeappx = "$($sdkbin.FullName)\x64\makeappx.exe"
$signtool = "$($sdkbin.FullName)\x64\signtool.exe"

# ---- the layout: the program, SDL2, the manifest, the tiles, the fonts, the game
Remove-Item -Recurse -Force $layout -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $layout, $outDir | Out-Null
Copy-Item "$built\mtm.exe", "$built\SDL2.dll" $layout
# The tiles: yours from tools\make_banner.py (Assets-user, Assets-user-demo;
# gitignored) when they are there, else the plain built-in ones. The demo
# without art of its own uses yours, as its Android banner does.
$choices = if ($Variant -eq 'demo') { 'Assets-user-demo', 'Assets-user', 'Assets-demo', 'Assets' } else { 'Assets-user', 'Assets' }
$assets = $choices | ForEach-Object { "$here\$_" } | Where-Object { Test-Path "$_\Square150x150Logo.png" } | Select-Object -First 1
Write-Host "tiles: $assets"
Copy-Item -Recurse $assets "$layout\Assets"
if (Test-Path "$here\fonts") { Copy-Item -Recurse "$here\fonts" "$layout\fonts" }
if ($Game) {
  if (-not (Test-Path "$Game\Monster.exe")) { throw "no Monster.exe in $Game" }
  Copy-Item -Recurse $Game "$layout\game"
  Write-Host "the game files from $Game are inside the package: do not share it"
}
$name, $title = if ($Variant -eq 'demo') { 'MTM2Recomp.Demo', 'Monster Truck Madness 2 Demo' } else { 'MTM2Recomp', 'Monster Truck Madness 2' }
(Get-Content "$here\AppxManifest.xml.in" -Raw).
  Replace('@NAME@', $name).Replace('@TITLE@', $title).Replace('@VERSION@', $Version) |
  Set-Content -Encoding UTF8 "$layout\AppxManifest.xml"

$msix = "$outDir\$name`_$Version`_x64.msix"
& $makeappx pack /o /d $layout /p $msix
if ($LASTEXITCODE -ne 0) { throw 'makeappx failed' }

# ---- signing: a self-signed certificate, made once and kept in xbox\cert\
$certDir = "$here\cert"
$pfx = "$certDir\mtm2recomp.pfx"
$pw = ConvertTo-SecureString 'mtm2recomp' -AsPlainText -Force
if (-not (Test-Path $pfx)) {
  New-Item -ItemType Directory -Force $certDir | Out-Null
  $cert = New-SelfSignedCertificate -Type Custom -Subject 'CN=MTM2Recomp' -KeyUsage DigitalSignature `
    -FriendlyName 'MTM2 Recomp (sideloading)' -CertStoreLocation 'Cert:\CurrentUser\My' `
    -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}')
  Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $pw | Out-Null
  Export-Certificate -Cert $cert -FilePath "$certDir\mtm2recomp.cer" | Out-Null
}
& $signtool sign /fd SHA256 /f $pfx /p mtm2recomp $msix
if ($LASTEXITCODE -ne 0) { throw 'signtool failed' }
Copy-Item "$certDir\mtm2recomp.cer" $outDir -Force

# ---- the C++ runtime package the app depends on
$vclibs = "${env:ProgramFiles(x86)}\Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs\14.0\Appx\Retail\x64\Microsoft.VCLibs.x64.14.00.appx"
if (Test-Path $vclibs) { Copy-Item $vclibs $outDir -Force }
else { Write-Warning "Microsoft.VCLibs.x64.14.00.appx not found: install it on the Xbox from Visual Studio's UWP tools" }

Write-Host "package: $msix"
