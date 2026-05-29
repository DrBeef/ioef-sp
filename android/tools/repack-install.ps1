# Repackage the gradle-built debug APK with freshly ndk-built .so, re-sign, and
# install -- so the native loop (edit -> ndk-build -> this -> launch) doesn't
# need an Android Studio rebuild each time.
# Note: native tools (adb/zipalign) write progress to stderr; don't treat that as
# fatal.  The zip/sign logic below is checked explicitly.
$ErrorActionPreference = 'Continue'
$proj = "C:\DEV\GitHub\Public\ioef-sp\android"
$bt   = "C:\Users\simon\AppData\Local\Android\Sdk\build-tools\34.0.0"
$adb  = "C:\Users\simon\AppData\Local\Android\Sdk\platform-tools\adb.exe"
$ks   = "$env:USERPROFILE\.android\debug.keystore"
$src  = "$proj\build\outputs\apk\debug\efxr-debug.apk"
$libs = "$proj\libs\arm64-v8a"
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"

$work = "$env:TEMP\efxrpatch"
Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $work | Out-Null
$patched = "$work\patched.apk"
Copy-Item $src $patched

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open($patched, 'Update')
# strip the old signature
@($zip.Entries | Where-Object { $_.FullName -like 'META-INF/*' -and $_.FullName -match '\.(RSA|SF|MF)$' }) | ForEach-Object { $_.Delete() }
# swap in the freshly built .so
foreach ($n in 'libefxr','libefgameaarch64','libefuiaarch64','libgl4es','libopenxr_loader','libc++_shared') {
    $ename = "lib/arm64-v8a/$n.so"
    $e = $zip.GetEntry($ename); if ($e) { $e.Delete() }
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, "$libs\$n.so", $ename) | Out-Null
}
$zip.Dispose()

& "$bt\zipalign.exe" -f 4 $patched "$work\aligned.apk"
& "$bt\apksigner.bat" sign --ks $ks --ks-pass pass:android --ks-key-alias androiddebugkey --key-pass pass:android "$work\aligned.apk"
& $adb install -r "$work\aligned.apk"
