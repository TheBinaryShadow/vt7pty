# Target-side helpers shared by the acceptance and diagnostic scripts.
# Windows 7 includes .NET Framework 3.5.1 and Windows PowerShell 2.0.
Add-Type -AssemblyName System.Web.Extensions
$script:vt7Json = New-Object System.Web.Script.Serialization.JavaScriptSerializer
$script:vt7Json.MaxJsonLength = 67108864
$script:vt7Utf8 = New-Object Text.UTF8Encoding($false)

function Read-VT7Json {
    param([string]$Path)
    return $script:vt7Json.DeserializeObject([IO.File]::ReadAllText($Path))
}

function Write-VT7Json {
    param([string]$Path, $Value)
    $builder = New-Object Text.StringBuilder
    Add-VT7JsonValue $builder $Value
    [IO.File]::WriteAllText($Path, $builder.ToString(), $script:vt7Utf8)
}

function Add-VT7JsonString {
    param([Text.StringBuilder]$Builder, [string]$Value)
    [void]$Builder.Append('"')
    foreach ($character in $Value.ToCharArray()) {
        $code = [int]$character
        if ($code -eq 34) { [void]$Builder.Append('\"') }
        elseif ($code -eq 92) { [void]$Builder.Append('\\') }
        elseif ($code -eq 8) { [void]$Builder.Append('\b') }
        elseif ($code -eq 9) { [void]$Builder.Append('\t') }
        elseif ($code -eq 10) { [void]$Builder.Append('\n') }
        elseif ($code -eq 12) { [void]$Builder.Append('\f') }
        elseif ($code -eq 13) { [void]$Builder.Append('\r') }
        elseif ($code -lt 32) {
            [void]$Builder.Append('\u')
            [void]$Builder.Append($code.ToString('x4'))
        } else { [void]$Builder.Append($character) }
    }
    [void]$Builder.Append('"')
}

function Add-VT7JsonValue {
    param([Text.StringBuilder]$Builder, $Value)
    if ($null -eq $Value) {
        [void]$Builder.Append('null')
    } elseif ($Value -is [string]) {
        Add-VT7JsonString $Builder ([string]$Value)
    } elseif ($Value -is [bool]) {
        [void]$Builder.Append($(if ($Value) { 'true' } else { 'false' }))
    } elseif ($Value -is [System.Collections.IDictionary]) {
        [void]$Builder.Append('{')
        $first = $true
        foreach ($key in $Value.Keys) {
            if (-not $first) { [void]$Builder.Append(',') }
            $first = $false
            Add-VT7JsonString $Builder ([string]$key)
            [void]$Builder.Append(':')
            Add-VT7JsonValue $Builder $Value[$key]
        }
        [void]$Builder.Append('}')
    } elseif ($Value -is [System.Collections.IEnumerable]) {
        [void]$Builder.Append('[')
        $first = $true
        foreach ($item in $Value) {
            if (-not $first) { [void]$Builder.Append(',') }
            $first = $false
            Add-VT7JsonValue $Builder $item
        }
        [void]$Builder.Append(']')
    } elseif ($Value -is [datetime]) {
        Add-VT7JsonString $Builder ($Value.ToUniversalTime().ToString('o'))
    } elseif ($Value -is [guid]) {
        Add-VT7JsonString $Builder ($Value.ToString())
    } elseif ($Value -is [System.IFormattable]) {
        [void]$Builder.Append($Value.ToString($null,
            [Globalization.CultureInfo]::InvariantCulture))
    } else {
        throw "Unsupported JSON value type: $($Value.GetType().FullName)"
    }
}

function Get-VT7Sha256 {
    param([string]$Path)
    $stream = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant()
    } finally {
        $sha.Clear()
        $stream.Close()
    }
}
