param(
    # test case
    [Parameter(Mandatory=$true)][string] $file,
    # module name
    [Parameter(Mandatory=$true)][string] $module
)

Function cleanup {
    Remove-Item -Recurse ./out
}

$compilerLocation = "../build/Compiler/Debug/Compiler.exe"
$vmLocation = "../build/VM/Debug/VM.exe"

# Invoke Compiler
$compilerExpression = "$compilerLocation project $file $module"
Write-Output "Running compiler as: $compilerExpression"
Invoke-Expression $compilerExpression
if ($LASTEXITCODE -ne 0) {
    Cleanup
    exit 1
}

# Invoke VM
$vmExpression = "$vmLocation ./out/$module.bc"
Write-Output "Running vm as: $vmExpression"
Invoke-Expression $vmExpression
if ($LASTEXITCODE -ne 0) {
    Cleanup
    exit 2
}

Cleanup

exit 0

