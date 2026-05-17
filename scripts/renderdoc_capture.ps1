param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath
)

$exeFullPath = Resolve-Path -Path $ExePath -ErrorAction Stop
$exeDir = Split-Path -Parent $exeFullPath
$captureDir = Join-Path $exeDir "temp"

function Resolve-RenderDocCmd {
    $candidates = @()
    if ($env:RENDERDOCCMD) { $candidates += $env:RENDERDOCCMD }
    if ($env:RENDERDOC_CMD) { $candidates += $env:RENDERDOC_CMD }
    $cmd = Get-Command renderdoccmd.exe -ErrorAction SilentlyContinue
    if ($cmd) { $candidates += $cmd.Source }
    if ($env:ProgramFiles) { $candidates += (Join-Path $env:ProgramFiles "RenderDoc\renderdoccmd.exe") }
    if (${env:ProgramFiles(x86)}) { $candidates += (Join-Path ${env:ProgramFiles(x86)} "RenderDoc\renderdoccmd.exe") }
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) { return $candidate }
    }
    return $null
}

function Resolve-QRenderDoc {
    $candidates = @()
    if ($env:QRENDERDOC) { $candidates += $env:QRENDERDOC }
    $cmd = Get-Command qrenderdoc.exe -ErrorAction SilentlyContinue
    if ($cmd) { $candidates += $cmd.Source }
    if ($env:ProgramFiles) { $candidates += (Join-Path $env:ProgramFiles "RenderDoc\qrenderdoc.exe") }
    if (${env:ProgramFiles(x86)}) { $candidates += (Join-Path ${env:ProgramFiles(x86)} "RenderDoc\qrenderdoc.exe") }
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) { return $candidate }
    }
    return $null
}

$renderdocCmd = Resolve-RenderDocCmd
if (-not $renderdocCmd) {
    Write-Error "renderdoccmd not found. Set RENDERDOCCMD/RENDERDOC_CMD or install RenderDoc."
    exit 1
}

$qrenderdoc = Resolve-QRenderDoc
if (-not $qrenderdoc) {
    Write-Error "qrenderdoc not found. Set QRENDERDOC or install RenderDoc."
    exit 1
}

& $renderdocCmd capture -w $exeFullPath
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$latestCapture = Get-ChildItem -Path $captureDir -Filter *.rdc -File -ErrorAction SilentlyContinue |
Sort-Object LastWriteTime -Descending |
Select-Object -First 1

if (-not $latestCapture) {
    Write-Error "No .rdc captures found in $captureDir."
    exit 1
}

Start-Process -FilePath $qrenderdoc -ArgumentList @($latestCapture.FullName)
