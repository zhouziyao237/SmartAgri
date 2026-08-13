param(
  [Parameter(Mandatory=$true)][string]$Port,
  [int]$Baud = 115200
)
$ErrorActionPreference = "Stop"
$sp = New-Object System.IO.Ports.SerialPort $Port,$Baud,'None',8,'One'
$sp.NewLine = "`r`n"
$sp.ReadTimeout = 800
$sp.WriteTimeout = 800
$sp.Open()
try {
  Start-Sleep -Milliseconds 250
  $null = $sp.ReadExisting()
  foreach($cmd in @('AT','AT+GMR','AT+PINS?','AT+STATUS?','AT+MAC?')) {
    Write-Host "> $cmd"
    $sp.Write("$cmd`r`n")
    Start-Sleep -Milliseconds 250
    $r = $sp.ReadExisting()
    if($r) { Write-Host $r.Trim() }
  }
} finally {
  $sp.Close()
}
