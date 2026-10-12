# Copyright (c) 2026 VT7Pty contributors
# SPDX-License-Identifier: MIT
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
. (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..\platform\Windows7Compat.ps1')
try {
    [void][IO.Directory]::CreateDirectory($OutputDirectory)
    $inputPath=Join-Path $OutputDirectory 'input.json'
    $outputPath=Join-Path $OutputDirectory 'roundtrip.json'
    # Named dictionary members and empty/nonempty arrays must survive nesting.
    $inputJson='{"ChildReady":{"Keys":[],"Values":"value","Count":4,"Item":{"Keys":[{"Character":3},{"Character":122}]},"GetEnumerator":"shadow","Flag":true,"Null":null,"Text":"\u0107","ControlType":4294967295},"ChildObservation":{"Keys":[{"Character":3,"Modifiers":8},{"Character":122,"Modifiers":0}]},"EmptyObject":{},"EmptyArray":[]}'
    [IO.File]::WriteAllText($inputPath,$inputJson,$script:vt7Utf8)
    $native=Read-VT7Json $inputPath
    Write-VT7Json $outputPath $native
    $found=Read-VT7Json $outputPath
    $ready=$found['ChildReady']; $observed=$found['ChildObservation']
    if (@($ready.GetEnumerator()).Count -ne 9 -or $ready['Keys'].Count -ne 0 -or $ready['Values'] -ne 'value' -or
        $ready['Count'] -ne 4 -or $ready['GetEnumerator'] -ne 'shadow' -or
        $ready['Item']['Keys'].Count -ne 2 -or $ready['Item']['Keys'][0]['Character'] -ne 3 -or
        $ready['Item']['Keys'][1]['Character'] -ne 122 -or $ready['Flag'] -ne $true -or
        $null -ne $ready['Null'] -or $ready['Text'] -cne ([string][char]0x107) -or
        $ready['ControlType'] -ne 4294967295 -or $observed['Keys'].Count -ne 2 -or
        $observed['Keys'][0]['Modifiers'] -ne 8 -or $observed['Keys'][1]['Modifiers'] -ne 0 -or
        $found['EmptyObject'].Count -ne 0 -or $found['EmptyArray'].Count -ne 0) {
        throw 'Portable JSON round-trip lost or changed an expected field.'
    }
    Write-Host 'VT7Pty portable JSON oracle passed.'
    exit 0
} catch {
    Write-Error $_.Exception.Message
    exit 1
}
