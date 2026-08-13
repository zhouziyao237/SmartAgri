param(
  [Parameter(Mandatory=$true)][string]$Port,
  [Parameter(Mandatory=$true)][string]$File,
  [int]$Baud = 115200
)
$sp = New-Object System.IO.Ports.SerialPort $Port,$Baud,'None',8,'One'
$sp.NewLine = "`r`n"
$sp.ReadTimeout = 600
$sp.Open()
try {
  Get-Content $File | ForEach-Object {
    $line=$_.Trim()
    if($line -and -not $line.StartsWith('#')) {
      Write-Host "> $line"
      $sp.Write("$line`r`n")
      Start-Sleep -Milliseconds 180
      try { $r=$sp.ReadExisting(); if($r){Write-Host $r.Trim()} } catch {}
    }
  }
} finally { $sp.Close() }
