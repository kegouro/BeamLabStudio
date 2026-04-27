param(
    [Parameter(Mandatory=$true)]
    [string]$RunDir
)

$ErrorActionPreference = "Stop"

function Normalize-PathForJson([string]$p) {
    return $p.Replace("\", "/")
}

function Read-Csv-Lite([string]$path) {
    if (!(Test-Path $path)) { return @() }
    return Import-Csv $path
}

function Get-Percentile([double[]]$values, [double]$q) {
    if ($values.Count -eq 0) { return 0.0 }
    $sorted = $values | Sort-Object
    $idx = [int][Math]::Floor(($sorted.Count - 1) * $q)
    return [double]$sorted[$idx]
}

$run = Resolve-Path $RunDir
$run = $run.Path

$visDir = Join-Path $run "visualization"
$geoDir = Join-Path $run "geometry"
New-Item -ItemType Directory -Force $visDir | Out-Null
New-Item -ItemType Directory -Force $geoDir | Out-Null

$manifestPath = Join-Path $visDir "visualization_manifest.json"

# -----------------------------
# 1) Generate trajectories OBJ
# -----------------------------
$trajCsv = Join-Path $visDir "trajectories_preview.csv"
$trajObj = Join-Path $geoDir "trajectories_preview.obj"

if (Test-Path $trajCsv) {
    Write-Host "Building trajectories OBJ from $trajCsv"

    $rows = Import-Csv $trajCsv
    if ($rows.Count -gt 0) {
        $cols = $rows[0].PSObject.Properties.Name

        $xCol = @("x_m","x","X","global_x_m") | Where-Object { $cols -contains $_ } | Select-Object -First 1
        $yCol = @("y_m","y","Y","global_y_m") | Where-Object { $cols -contains $_ } | Select-Object -First 1
        $zCol = @("z_m","z","Z","global_z_m") | Where-Object { $cols -contains $_ } | Select-Object -First 1
        $idCol = @("trajectory_id","track_id","event_id","particle_id","id") | Where-Object { $cols -contains $_ } | Select-Object -First 1

        if ($xCol -and $yCol -and $zCol) {
            $sw = New-Object System.IO.StreamWriter($trajObj, $false)
            $sw.WriteLine("# BeamLabStudio Windows-generated trajectories OBJ")

            $groups = @{}
            $vIndex = 1

            foreach ($r in $rows) {
                $tid = "0"
                if ($idCol) { $tid = [string]$r.$idCol }

                if (!$groups.ContainsKey($tid)) {
                    $groups[$tid] = New-Object System.Collections.Generic.List[int]
                }

                $x = [double]::Parse(([string]$r.$xCol), [Globalization.CultureInfo]::InvariantCulture)
                $y = [double]::Parse(([string]$r.$yCol), [Globalization.CultureInfo]::InvariantCulture)
                $z = [double]::Parse(([string]$r.$zCol), [Globalization.CultureInfo]::InvariantCulture)

                $sw.WriteLine(("v {0:R} {1:R} {2:R}" -f $x,$y,$z))
                $groups[$tid].Add($vIndex)
                $vIndex++
            }

            foreach ($key in $groups.Keys) {
                $list = $groups[$key]
                if ($list.Count -ge 2) {
                    $sw.WriteLine("l " + (($list | ForEach-Object { [string]$_ }) -join " "))
                }
            }

            $sw.Close()
            Copy-Item $trajObj (Join-Path $visDir "trajectories_preview.obj") -Force
            Write-Host "Created $trajObj"
        }
    }
}

# -------------------------------------------------------
# 2) Fix effective lens OBJ if only flat disk was exported
#    Rebuilds a curved, beam-scale lens body from disk OBJ.
# -------------------------------------------------------
$focalCsv = Join-Path $visDir "focal_slice_points.csv"
$diskObjCandidates = @(
    (Join-Path $geoDir "effective_lens_disk_preview.obj"),
    (Join-Path $geoDir "effective_lens_disk.obj")
)

$diskObj = $diskObjCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if ((Test-Path $focalCsv) -and $diskObj) {
    Write-Host "Rebuilding effective lens body from focal slice scale"

    $focalRows = Import-Csv $focalCsv
    $radii = New-Object System.Collections.Generic.List[double]

    if ($focalRows.Count -gt 0) {
        $cols = $focalRows[0].PSObject.Properties.Name
        $uCol = @("u_m","x_m","x","X") | Where-Object { $cols -contains $_ } | Select-Object -First 1
        $vCol = @("v_m","y_m","y","Y") | Where-Object { $cols -contains $_ } | Select-Object -First 1

        if ($uCol -and $vCol) {
            foreach ($r in $focalRows) {
                $u = [double]::Parse(([string]$r.$uCol), [Globalization.CultureInfo]::InvariantCulture)
                $v = [double]::Parse(([string]$r.$vCol), [Globalization.CultureInfo]::InvariantCulture)
                $radii.Add([Math]::Sqrt($u*$u + $v*$v))
            }
        }
    }

    $targetR = Get-Percentile $radii.ToArray() 0.98
    if ($targetR -le 0) { $targetR = 0.01 }
    $targetR = $targetR * 1.15
    $thickness = [Math]::Max($targetR * 0.35, 0.001)

    $vertices = New-Object System.Collections.Generic.List[double[]]
    $faces = New-Object System.Collections.Generic.List[int[]]

    Get-Content $diskObj | ForEach-Object {
        $line = $_.Trim()
        if ($line.StartsWith("v ")) {
            $parts = $line.Split(" ", [System.StringSplitOptions]::RemoveEmptyEntries)
            if ($parts.Count -ge 4) {
                $vertices.Add(@(
                    [double]::Parse($parts[1], [Globalization.CultureInfo]::InvariantCulture),
                    [double]::Parse($parts[2], [Globalization.CultureInfo]::InvariantCulture),
                    [double]::Parse($parts[3], [Globalization.CultureInfo]::InvariantCulture)
                ))
            }
        } elseif ($line.StartsWith("f ")) {
            $parts = $line.Split(" ", [System.StringSplitOptions]::RemoveEmptyEntries)
            $idx = @()
            for ($i=1; $i -lt $parts.Count; $i++) {
                $idx += [int](($parts[$i].Split("/"))[0])
            }
            if ($idx.Count -ge 3) { $faces.Add($idx) }
        }
    }

    if ($vertices.Count -gt 0) {
        $min = @(1e99,1e99,1e99)
        $max = @(-1e99,-1e99,-1e99)
        $center = @(0.0,0.0,0.0)

        foreach ($v in $vertices) {
            for ($i=0; $i -lt 3; $i++) {
                if ($v[$i] -lt $min[$i]) { $min[$i] = $v[$i] }
                if ($v[$i] -gt $max[$i]) { $max[$i] = $v[$i] }
                $center[$i] += $v[$i]
            }
        }

        for ($i=0; $i -lt 3; $i++) { $center[$i] /= $vertices.Count }

        $span = @($max[0]-$min[0], $max[1]-$min[1], $max[2]-$min[2])
        $normalAxis = 0
        if ($span[1] -lt $span[$normalAxis]) { $normalAxis = 1 }
        if ($span[2] -lt $span[$normalAxis]) { $normalAxis = 2 }

        $axisA = @(0,1,2) | Where-Object { $_ -ne $normalAxis } | Select-Object -First 1
        $axisB = @(0,1,2) | Where-Object { $_ -ne $normalAxis -and $_ -ne $axisA } | Select-Object -First 1

        $currentR = 0.0
        foreach ($v in $vertices) {
            $a = $v[$axisA] - $center[$axisA]
            $b = $v[$axisB] - $center[$axisB]
            $rr = [Math]::Sqrt($a*$a + $b*$b)
            if ($rr -gt $currentR) { $currentR = $rr }
        }
        if ($currentR -le 0) { $currentR = 1.0 }

        $scale = $targetR / $currentR

        $outObj = Join-Path $geoDir "effective_lens_disk_preview.obj"
        $outObjFull = Join-Path $geoDir "effective_lens_disk.obj"

        $sw = New-Object System.IO.StreamWriter($outObj, $false)
        $sw.WriteLine("# BeamLabStudio Windows rebuilt curved effective lens body")

        foreach ($side in @(1.0,-1.0)) {
            foreach ($v in $vertices) {
                $nv = @($center[0],$center[1],$center[2])
                $a = ($v[$axisA] - $center[$axisA]) * $scale
                $b = ($v[$axisB] - $center[$axisB]) * $scale
                $rho = [Math]::Min(1.0, [Math]::Sqrt($a*$a + $b*$b) / $targetR)
                $bulge = $side * $thickness * (1.0 - $rho*$rho)

                $nv[$axisA] = $center[$axisA] + $a
                $nv[$axisB] = $center[$axisB] + $b
                $nv[$normalAxis] = $center[$normalAxis] + $bulge

                $sw.WriteLine(("v {0:R} {1:R} {2:R}" -f $nv[0],$nv[1],$nv[2]))
            }
        }

        $n = $vertices.Count
        foreach ($f in $faces) {
            $sw.WriteLine("f " + (($f | ForEach-Object { [string]$_ }) -join " "))
            $rev = @()
            foreach ($i in ($f | Sort-Object -Descending)) {
                $rev += ($i + $n)
            }
            $sw.WriteLine("f " + (($rev | ForEach-Object { [string]$_ }) -join " "))
        }

        $sw.Close()
        Copy-Item $outObj $outObjFull -Force
        Copy-Item $outObj (Join-Path $visDir "effective_lens_disk_preview.obj") -Force
        Write-Host "Rebuilt effective lens using target radius $targetR m"
    }
}

# -----------------------------
# 3) Patch manifest paths
# -----------------------------
if (Test-Path $manifestPath) {
    $json = Get-Content $manifestPath -Raw | ConvertFrom-Json

    if ($null -eq $json.files) {
        $json | Add-Member -NotePropertyName files -NotePropertyValue ([PSCustomObject]@{})
    }

    $trajVisObj = Join-Path $visDir "trajectories_preview.obj"
    if (Test-Path $trajVisObj) {
        $json.files | Add-Member -Force -NotePropertyName trajectories_preview_obj -NotePropertyValue (Normalize-PathForJson $trajVisObj)
    }

    $lensVisObj = Join-Path $visDir "effective_lens_disk_preview.obj"
    if (Test-Path $lensVisObj) {
        $json.files | Add-Member -Force -NotePropertyName effective_lens_disk_preview_obj -NotePropertyValue (Normalize-PathForJson $lensVisObj)
        $json.files | Add-Member -Force -NotePropertyName effective_lens_body_preview_obj -NotePropertyValue (Normalize-PathForJson $lensVisObj)
    }

    $json | ConvertTo-Json -Depth 20 | Set-Content $manifestPath -Encoding UTF8
    Write-Host "Patched manifest $manifestPath"
}

Write-Host "Windows postprocess complete."
exit 0
