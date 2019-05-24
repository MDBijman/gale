param(
    # test case
    [Parameter(Mandatory=$true)][string] $file,
    # module name
    [Parameter(Mandatory=$true)][string] $module
)

Function cleanup {
    Remove-Item -Recurse ./out
}

$compilerLocation = "../build/Compiler/Debug/galec.exe"
$vmLocation = "../build/VM/Debug/galevm.exe"

# Invoke Compiler
$compilerExpression = "$compilerLocation project $file $module"
Write-Debug "Running compiler as: $compilerExpression"
$out = Invoke-Expression $compilerExpression
if ($LASTEXITCODE -ne 0) {
    Cleanup
    exit 1
}

# Invoke VM
$vmExpression = "$vmLocation ./out/test.bc"
Write-Debug "Running vm as: $vmExpression"
Invoke-Expression $vmExpression
if ($LASTEXITCODE -ne 0) {
    Cleanup
    exit 2
}

Cleanup

exit 0

