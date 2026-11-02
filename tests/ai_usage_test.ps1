$ErrorActionPreference='Stop'
. "$PSScriptRoot/../helpers/AIUsage/AIUsage.Refresh.ps1" -LibraryOnly
function Assert($condition,$message) { if (!$condition) { throw $message } }
$data = '{"rateLimits":{"primary":{"usedPercent":25,"windowDurationMins":300,"resetsAt":1780000000},"secondary":{"usedPercent":5,"windowDurationMins":10080}}}' | ConvertFrom-Json
$r = Parse-Codex $data
Assert ($r.codex5h.value -eq '75%') 'Codex used-to-remaining conversion'
Assert ($r.codexWeek.value -eq '95%') 'Codex weekly conversion'
$data.rateLimits.primary.windowDurationMins=15
$r = Parse-Codex $data
Assert (!$r.ContainsKey('codex5h')) 'Do not mislabel a 15 minute window as 5h'
$settings=@{MiniMaxModel='general';MiniMaxCountMeaning='remaining'}
$data='{"base_resp":{"status_code":0},"model_remains":[{"model_name":"general","current_interval_total_count":100,"current_interval_usage_count":80,"current_weekly_total_count":1000,"current_weekly_usage_count":600,"end_time":1780000000000}]}' | ConvertFrom-Json
$r=Parse-MiniMax $data $settings
Assert ($r.minimax5h.value -eq '80%') 'MiniMax remaining semantics'
Assert ($r.minimaxWeek.value -eq '60%') 'MiniMax weekly semantics'
$settings.MiniMaxCountMeaning='used'
$r=Parse-MiniMax $data $settings
Assert ($r.minimax5h.value -eq '20%') 'MiniMax configurable used semantics'
$data.model_remains[0] | Add-Member current_interval_remaining_count 30
$r=Parse-MiniMax $data $settings
Assert ($r.minimax5h.value -eq '30%') 'Explicit remaining field takes precedence'
$data='{"base_resp":{"status_code":0},"model_remains":[{"model_name":"general","current_interval_total_count":0,"current_interval_usage_count":0,"current_weekly_total_count":0,"current_weekly_usage_count":0,"current_interval_remaining_percent":99,"current_weekly_remaining_percent":99,"end_time":1790924400000,"weekly_end_time":1791129600000}]}' | ConvertFrom-Json
$r=Parse-MiniMax $data $settings
Assert ($r.minimax5h.value -eq '99%') 'New MiniMax percentages with zero counts'
Assert ($r.minimaxWeek.value -eq '99%') 'New MiniMax weekly percentage'
$data.model_remains[0].current_interval_remaining_percent=0
$r=Parse-MiniMax $data $settings
Assert ($r.minimax5h.value -eq '0%') 'Zero remaining percentage is valid'
$data.model_remains[0].current_interval_total_count=100
$data.model_remains[0].current_interval_usage_count=50
$r=Parse-MiniMax $data $settings
Assert ($r.minimax5h.value -eq '0%') 'Percentage takes precedence over legacy counts'
Assert ((Percent 150) -eq '100%') 'Clamp percentage'
Assert ((Percent -1) -eq '0%') 'Clamp negative percentage'
$failed=$false
try { Parse-MiniMax ('{"base_resp":{"status_code":1004}}'|ConvertFrom-Json) $settings } catch { $failed=$true }
Assert $failed 'Authentication error must not show full quota'
Assert ((Window 99 1791129600000).resetEpoch -eq 1791129600) 'Normalize MiniMax reset milliseconds'
Assert ((Window 99 1791129600).resetEpoch -eq 1791129600) 'Preserve Codex reset seconds'
Assert ((Window 99 $null).resetEpoch -eq 0) 'Missing reset must remain unavailable'
Write-Host 'PASS: quota parsing, duration selection, missing windows, API errors.'
$cursor='{"billingCycleEnd":"2026-10-17T09:00:59.000Z","individualUsage":{"plan":{"enabled":true,"used":2000,"limit":2000,"remaining":0,"totalPercentUsed":82.78,"autoPercentUsed":82.67,"apiPercentUsed":84.98}}}'|ConvertFrom-Json
$r=Parse-Cursor $cursor
Assert ($r.cursorPlan.value -eq '17%') 'Cursor total percentage overrides nominal exhausted dollar budget'
Assert ($r.Count -eq 1) 'Only expose Cursor overall quota'
Assert ($r.cursorPlan.resetEpoch -eq [DateTimeOffset]::Parse('2026-10-17T09:00:59Z').ToUnixTimeSeconds()) 'Cursor ISO billing reset'
$cursor.individualUsage.plan.totalPercentUsed=100
$r=Parse-Cursor $cursor
Assert ($r.cursorPlan.value -eq '0%') 'Cursor exhausted percentage is valid'
$cursor='{"individualUsage":{"plan":{"enabled":true,"limit":100,"remaining":40}}}'|ConvertFrom-Json
$r=Parse-Cursor $cursor
Assert ($r.cursorPlan.value -eq '40%') 'Cursor legacy numeric budget'
Assert (!$r.ContainsKey('cursorAuto') -and !$r.ContainsKey('cursorApi')) 'Do not invent missing Cursor pools'
$failed=$false
try { Parse-Cursor ('{"individualUsage":{"plan":{"enabled":false,"totalPercentUsed":0}}}'|ConvertFrom-Json) } catch { $failed=$true }
Assert $failed 'Disabled Cursor plan must not show full quota'
Write-Host 'PASS: Cursor overall percentage, billing reset, old budget, missing/disabled quota.'
$claude='{"five_hour":{"utilization":25,"resets_at":"2026-10-13T12:00:00Z"},"seven_day":{"utilization":80,"resets_at":"2026-10-18T12:00:00Z"}}'|ConvertFrom-Json
$r=Parse-Claude $claude
Assert ($r.claude5h.value -eq '75%' -and $r.claudeWeek.value -eq '20%') 'Claude used-to-remaining conversion'
Assert ($r.claudeWeek.resetEpoch -eq [DateTimeOffset]::Parse('2026-10-18T12:00:00Z').ToUnixTimeSeconds()) 'Claude ISO weekly reset'
$claude.five_hour.utilization=100
$claude.seven_day=$null
$r=Parse-Claude $claude
Assert ($r.claude5h.value -eq '0%' -and !$r.ContainsKey('claudeWeek')) 'Claude zero and missing weekly window'
$failed=$false
try { Parse-Claude ('{"error":{"type":"authentication_error"}}'|ConvertFrom-Json) } catch { $failed=$true }
Assert $failed 'Claude error response is not full quota'
Write-Host 'PASS: Claude percentages, reset timestamps, missing quota and error responses.'
$kimi='{"usage":{"limit":"100","used":"26","remaining":"74","resetTime":"2026-11-09T12:00:00Z"},"limits":[{"duration":300,"detail":{"limit":"100","used":"25","remaining":"75","resetTime":1793913600000}}]}'|ConvertFrom-Json
$r=Parse-Kimi $kimi
Assert ($r.kimi5h.value -eq '75%' -and $r.kimiWeek.value -eq '74%') 'Kimi remaining values'
Assert ($r.kimi5h.resetEpoch -eq 1793913600 -and $r.kimiWeek.resetEpoch -eq [DateTimeOffset]::Parse('2026-11-09T12:00:00Z').ToUnixTimeSeconds()) 'Kimi reset timestamps'
$kimi='{"usages":{"limit_5h":{"used_ratio":0.25,"reset_time":1793913600},"limit_month_total":{"used_ratio":0.4,"reset_time":1796515200}}}'|ConvertFrom-Json
$r=Parse-Kimi $kimi
Assert ($r.kimi5h.value -eq '75%' -and $r.kimiWeek.value -eq '60%') 'Kimi ratio response and monthly fallback'
$kimi.usages.limit_5h.used_ratio=1
$r=Parse-Kimi $kimi
Assert ($r.kimi5h.value -eq '0%') 'Kimi exhausted quota is valid'
Write-Host 'PASS: Kimi legacy/ratio responses, remaining quota, monthly fallback and reset times.'
$antigravity='{"available_models":{"response":{"clientModelConfigs":[{"label":"Gemini","quotaInfo":{"remainingFraction":0.8,"resetTime":"2026-11-03T12:00:00Z"}},{"label":"Claude","quotaInfo":{"remainingFraction":0.2,"resetTime":"2026-11-04T12:00:00Z"}}]}}}'|ConvertFrom-Json
$r=Parse-Antigravity $antigravity
Assert ($r.antigravityPlan.value -eq '20%') 'Antigravity exposes the most constrained model'
Assert ($r.antigravityPlan.resetEpoch -eq [DateTimeOffset]::Parse('2026-11-04T12:00:00Z').ToUnixTimeSeconds()) 'Antigravity matching reset time'
$antigravity.available_models.response.clientModelConfigs[1].quotaInfo.remainingFraction=0
$r=Parse-Antigravity $antigravity
Assert ($r.antigravityPlan.value -eq '0%') 'Antigravity exhausted quota is valid'
$failed=$false
try { Parse-Antigravity ('{"available_models":{"response":{"clientModelConfigs":[]}}}'|ConvertFrom-Json) } catch { $failed=$true }
Assert $failed 'Antigravity missing model quota is not full quota'
$agyCloud='{"models":{"chat_internal":{"isInternal":true,"quotaInfo":{"remainingFraction":0.1,"resetTime":"2026-11-02T07:59:13Z"}},"gemini-3-flash":{"displayName":"Gemini 3 Flash","quotaInfo":{"remainingFraction":0.91,"resetTime":"2026-11-02T07:59:13Z"}}}}'|ConvertFrom-Json
$r=Parse-Antigravity $agyCloud
Assert ($r.antigravityPlan.value -eq '91%') 'Antigravity cloud API ignores internal models and parses remainingFraction'
Assert ($r.antigravityPlan.resetEpoch -eq [DateTimeOffset]::Parse('2026-11-02T07:59:13Z').ToUnixTimeSeconds()) 'Antigravity cloud API reset time'
Write-Host 'PASS: Antigravity minimum model quota, reset times, exhausted and missing quota.'
