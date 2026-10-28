param([Parameter(Mandatory=$true)][string]$Worker)
$ErrorActionPreference='Stop'
$assembly=[Reflection.Assembly]::LoadFrom([IO.Path]::GetFullPath($Worker))
$type=$assembly.GetType('TemperatureWorker',$true)
$method=$type.GetMethod('Numeric',[Reflection.BindingFlags]'NonPublic,Static')
foreach($value in @($null,[single]0,[single](-1),[single]151,[single]::NaN,[single]::PositiveInfinity)) {
    if($method.Invoke($null,[object[]]@($value)) -ne '-1') {throw "Invalid/unavailable temperature accepted: $value"}
}
foreach($value in @([single]72.5,[single]86)) {
    if($method.Invoke($null,[object[]]@($value)) -ne $value.ToString('0.0',[Globalization.CultureInfo]::InvariantCulture)) {throw 'Valid temperature formatting changed'}
}
Write-Host 'PASS: unavailable/zero/nonfinite sensor readings remain unknown; valid CPU/GPU readings preserved.'
