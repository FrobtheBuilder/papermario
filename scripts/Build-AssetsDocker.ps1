[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath,

    [ValidateSet("us", "jp", "pal", "ique")]
    [string]$Version = "us",

    [switch]$BuildRom,

    [string]$ImageTag = "papermario-assets:latest",

    [string]$DecompRepoUrl = "https://github.com/pmret/papermario.git",

    [string]$DecompRef = "main",

    [int]$CloneDepth = 1,

    [string]$SplatUrl = "https://github.com/ethteck/splat.git",

    [string]$SplatRef = "0.31.0",

    [string]$ConfigureArgs = "",

    [string]$HostDecompPath = "",

    [switch]$NoSync
)

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$dockerfilePath = Join-Path $repoRoot "docker\papermario-assets\Dockerfile"
$runnerPath = Join-Path $repoRoot "docker\papermario-assets\run-assets.sh"
$resolvedRomPath = (Resolve-Path $RomPath).Path

if ([string]::IsNullOrWhiteSpace($HostDecompPath)) {
    $HostDecompPath = $repoRoot
}

if (-not (Test-Path $dockerfilePath)) {
    throw "Dockerfile was not found at '$dockerfilePath'."
}

if (-not (Test-Path $runnerPath)) {
    throw "Runner script was not found at '$runnerPath'."
}

if (-not (Test-Path $resolvedRomPath)) {
    throw "ROM path does not exist: '$resolvedRomPath'."
}

$buildRomValue = if ($BuildRom.IsPresent) { "1" } else { "0" }
$syncToHostValue = if ($NoSync.IsPresent) { "0" } else { "1" }
$romBind = "${resolvedRomPath}:/roms/baserom.z64:ro"

$dockerArgs = @(
    "run", "--rm",
    "--volume", $romBind,
    "--env", "ROM_PATH=/roms/baserom.z64",
    "--env", "VERSION=$Version",
    "--env", "BUILD_ROM=$buildRomValue",
    "--env", "REPO_URL=$DecompRepoUrl",
    "--env", "REPO_REF=$DecompRef",
    "--env", "CLONE_DEPTH=$CloneDepth",
    "--env", "SPLAT_URL=$SplatUrl",
    "--env", "SPLAT_REF=$SplatRef",
    "--env", "CONFIGURE_ARGS=$ConfigureArgs",
    "--env", "SYNC_TO_HOST=$syncToHostValue",
    "--env", "HOST_DECOMP_DIR=/host-decomp"
)

if (-not $NoSync.IsPresent) {
    if (-not (Test-Path $HostDecompPath)) {
        throw "Host decomp path does not exist: '$HostDecompPath'."
    }

    $resolvedHostDecompPath = (Resolve-Path $HostDecompPath).Path
    $dockerArgs += @("--volume", "${resolvedHostDecompPath}:/host-decomp")
}

$dockerArgs += @($ImageTag, "/usr/local/bin/run-assets.sh")

Write-Host "Building Docker image: $ImageTag"
docker build `
    -t $ImageTag `
    -f $dockerfilePath `
    $repoRoot
if ($LASTEXITCODE -ne 0) {
    throw "Docker build failed."
}

Write-Host "Running asset pipeline in container"
& docker @dockerArgs
if ($LASTEXITCODE -ne 0) {
    throw "Asset pipeline failed."
}

Write-Host "Asset pipeline completed successfully."
