param(
    [Parameter(Mandatory = $true)][string]$SourceDirectory,
    [Parameter(Mandatory = $true)][string]$ArchivePath
)

$ErrorActionPreference = 'Stop'
Compress-Archive -LiteralPath $SourceDirectory -DestinationPath $ArchivePath -Force
