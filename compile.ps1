[CmdletBinding()]
param (
    [Parameter(Position = 1, Mandatory = $true)]
    [String]
    $path,
    [Parameter(Position = 2)]
    [String]
    $compilerArgs
)
$base = (Split-Path -Path $path)
$filename = ([io.path]::GetFileNameWithoutExtension($path))
Write-Host $restArgs
cargo run --release --bin gale -- -i ($path) -o ($base + "\" + $filename + ".gbc") $compilerArgs
