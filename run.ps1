[CmdletBinding()]
param (
    [Parameter(Position = 1, Mandatory = $true)]
    [String]
    $file,
    [Parameter(ValueFromRemainingArguments)]
    $restArgs
)

cargo run --release --bin galevm -- -i ($file) $restArgs
