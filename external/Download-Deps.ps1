# Stop execution on first error
$global:ErrorActionPreference = 'Stop'

# Hide progress bars (significant perf improvement for web requests)
$global:ProgressPreference = 'SilentlyContinue'

# Setup paths
$extDir = $PSScriptRoot
$includeDir = "$extDir/include/SDL2"
$libDir = "$extDir/lib"

# Wipe out and recreate directories
Remove-Item -Path $libDir -Recurse -ErrorAction Ignore
Remove-Item -Path $includeDir -Recurse -ErrorAction Ignore
New-Item $libDir -ItemType Directory
New-Item $includeDir -ItemType Directory

#####
# SDL2
#####
$pkgUri = "https://www.libsdl.org/release/SDL2-devel-2.0.14-VC.zip"
$pkgZip = "$extDir/SDL2-devel-2.0.14-VC.zip"
$unzipDir = "$extDir/sdl2-temp"

Write-Host "[SDL2] Downloading..."
Remove-Item $pkgZip -ErrorAction Ignore
Invoke-WebRequest -Uri $pkgUri -OutFile $pkgZip

Write-Host "[SDL2] Extracting..."
Remove-Item $unzipDir -Recurse -ErrorAction Ignore
Expand-Archive -Path $pkgZip -DestinationPath $unzipDir

Write-Host "[SDL2] Setting up directories..."
Copy-Item -Path "$unzipDir/SDL2-2.0.14/lib/x64/*" -Destination $libDir
Copy-Item -Path "$unzipDir/SDL2-2.0.14/include/*" -Destination $includeDir

Write-Host "[SDL2] Cleaning up..."
Remove-Item $pkgZip -ErrorAction Ignore
Remove-Item $unzipDir -Recurse -ErrorAction Ignore

#####
# SDL2 Image
#####
$pkgUri = "https://www.libsdl.org/projects/SDL_image/release/SDL2_image-devel-2.0.5-VC.zip"
$pkgZip = "$extDir/SDL2_image-devel-2.0.5-VC.zip"
$unzipDir = "$extDir/sdl2_image-temp"

Write-Host "[SDL2_Image] Downloading..."
Remove-Item $pkgZip -ErrorAction Ignore
Invoke-WebRequest -Uri $pkgUri -OutFile $pkgZip

Write-Host "[SDL2_Image] Extracting..."
Remove-Item $unzipDir -Recurse -ErrorAction Ignore
Expand-Archive -Path $pkgZip -DestinationPath $unzipDir

Write-Host "[SDL2_Image] Setting up directories..."
Copy-Item -Path "$unzipDir/SDL2_image-2.0.5/lib/x64/*" -Destination $libDir
Copy-Item -Path "$unzipDir/SDL2_image-2.0.5/include/*" -Destination $includeDir

Write-Host "[SDL2_Image] Cleaning up..."
Remove-Item $pkgZip -ErrorAction Ignore
Remove-Item $unzipDir -Recurse -ErrorAction Ignore

#####
# SDL2 TTF
#####
$pkgUri = "https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-devel-2.0.15-VC.zip"
$pkgZip = "$extDir/SDL2_ttf-devel-2.0.15-VC.zip"
$unzipDir = "$extDir/sdl2_ttf-temp"

Write-Host "[SDL2_TTF] Downloading..."
Remove-Item $pkgZip -ErrorAction Ignore
Invoke-WebRequest -Uri $pkgUri -OutFile $pkgZip

Write-Host "[SDL2_TTF] Extracting..."
Remove-Item $unzipDir -Recurse -ErrorAction Ignore
Expand-Archive -Path $pkgZip -DestinationPath $unzipDir

Write-Host "[SDL2_TTF] Setting up directories..."
Copy-Item -Path "$unzipDir/SDL2_ttf-2.0.15/lib/x64/*" -Destination $libDir
Copy-Item -Path "$unzipDir/SDL2_ttf-2.0.15/include/*" -Destination $includeDir

Write-Host "[SDL2_TTF] Cleaning up..."
Remove-Item $pkgZip -ErrorAction Ignore
Remove-Item $unzipDir -Recurse -ErrorAction Ignore

Write-Host "`n[DONE]"
