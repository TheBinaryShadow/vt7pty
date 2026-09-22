[CmdletBinding()]
param(
    [switch]$DetailedOutput
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

Write-Host 'Running MSVC native static analysis: Release|x64'
& (Join-Path $PSScriptRoot 'Build-VT7Pty.ps1') `
    -Configuration Release `
    -Target Rebuild `
    -StaticAnalysis `
    -DetailedOutput:$DetailedOutput
