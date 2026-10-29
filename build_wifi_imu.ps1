param(
    [string]$SdkRoot = "C:\ti\mspm0_sdk_2_11_00_07",
    [string]$Toolchain = "C:\ti\ti_cgt_arm_llvm_4.0.2.LTS",
    [string]$ExtraDefine = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = $PSScriptRoot
$repoRoot = Split-Path -Parent $projectRoot
$buildDir = Join-Path $PSScriptRoot "build"
$objectDir = Join-Path $buildDir "obj"
$configDir = Join-Path $projectRoot "config"
$compiler = Join-Path $Toolchain "bin\tiarmclang.exe"
$objcopy = Join-Path $Toolchain "bin\tiarmobjcopy.exe"
$sizeTool = Join-Path $Toolchain "bin\tiarmsize.exe"
$startup = Join-Path $SdkRoot "source\ti\devices\msp\m0p\startup_system_files\ticlang\startup_mspm0g351x_ticlang.c"

foreach ($required in @($compiler, $objcopy, $sizeTool, $startup)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required tool or file not found: $required"
    }
}

if (Test-Path -LiteralPath $buildDir) {
    Remove-Item -LiteralPath $buildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $objectDir | Out-Null

Push-Location $repoRoot
try {
    $includes = @(
        "-I$configDir",
        "-I$(Join-Path $repoRoot 'libraries\zf_common')",
        "-I$(Join-Path $repoRoot 'libraries\zf_components')",
        "-I$(Join-Path $repoRoot 'libraries\zf_device')",
        "-I$(Join-Path $repoRoot 'libraries\zf_driver')",
        "-I$(Join-Path $projectRoot 'user\inc')",
        "-I$(Join-Path $SdkRoot 'source\third_party\CMSIS\Core\Include')",
        "-I$(Join-Path $SdkRoot 'source')"
    )
    $compileFlags = @(
        "@$configDir\device.opt",
        $includes,
        "-D__MSPM0G3519__",
        "-mcpu=cortex-m0plus",
        "-march=thumbv6m",
        "-mfloat-abi=soft",
        "-mthumb",
        "-std=c99",
        "-O2",
        "-gdwarf-3",
        "-Wall",
        "-Wextra"
    )
    if ($ExtraDefine) {
        $compileFlags += "-D$ExtraDefine"
    }

    $sources = @(
        @{ Source = (Join-Path $projectRoot "user\src\main.c"); Object = "main.obj" },
        @{ Source = (Join-Path $projectRoot "user\src\pid.c"); Object = "pid.obj" },
        @{ Source = (Join-Path $projectRoot "user\src\encoder.c"); Object = "encoder.obj" },
        @{ Source = (Join-Path $projectRoot "user\src\motor.c"); Object = "motor.obj" },
        @{ Source = (Join-Path $projectRoot "user\src\imu963.c"); Object = "imu963.obj" },
        @{ Source = (Join-Path $projectRoot "user\src\wifi.c"); Object = "wifi.obj" },
        @{ Source = (Join-Path $configDir "ti_msp_dl_config.c"); Object = "ti_msp_dl_config.obj" },
        @{ Source = (Join-Path $repoRoot "libraries\zf_common\zf_common_debug.c"); Object = "zf_common_debug.obj" },
        @{ Source = (Join-Path $repoRoot "libraries\zf_common\zf_common_fifo.c"); Object = "zf_common_fifo.obj" },
        @{ Source = (Join-Path $repoRoot "libraries\zf_common\zf_common_interrupt.c"); Object = "zf_common_interrupt.obj" },
        @{ Source = (Join-Path $repoRoot "libraries\zf_driver\zf_driver_delay.c"); Object = "zf_driver_delay.obj" },
        @{ Source = (Join-Path $repoRoot "libraries\zf_driver\zf_driver_gpio.c"); Object = "zf_driver_gpio.obj" },
        @{ Source = (Join-Path $repoRoot "libraries\zf_driver\zf_driver_spi.c"); Object = "zf_driver_spi.obj" },
        @{ Source = (Join-Path $repoRoot "libraries\zf_driver\zf_driver_uart.c"); Object = "zf_driver_uart.obj" },
        @{ Source = (Join-Path $repoRoot "libraries\zf_device\zf_device_imu963ra.c"); Object = "zf_device_imu963ra.obj" },
        @{ Source = (Join-Path $repoRoot "libraries\zf_device\zf_device_wifi_spi.c"); Object = "zf_device_wifi_spi.obj" },
        @{ Source = (Join-Path $repoRoot "libraries\zf_device\zf_device_type.c"); Object = "zf_device_type.obj" },
        @{ Source = $startup; Object = "startup_mspm0g351x_ticlang.obj" }
    )

    foreach ($source in $sources) {
        $objectPath = Join-Path $objectDir $source.Object
        Write-Host "Compiling $($source.Source)"
        & $compiler @compileFlags -c $source.Source -o $objectPath
        if ($LASTEXITCODE -ne 0) {
            throw "Compilation failed: $($source.Source)"
        }
    }

    $objects = $sources | ForEach-Object { Join-Path $objectDir $_.Object }
    $output = Join-Path $buildDir "wifi_imu_vofa.out"
    $map = Join-Path $buildDir "wifi_imu_vofa.map"
    Write-Host "Linking wifi_imu_vofa.out"
    & $compiler `
        "-Wl,-u,_c_int00" `
        @objects `
        "-ldevice.cmd.genlibs" `
        "-L$configDir" `
        "-L$(Join-Path $SdkRoot 'source')" `
        (Join-Path $configDir "device_linker.cmd") `
        "-Wl,-m,$map" `
        "-Wl,--rom_model" `
        "-Wl,--warn_sections" `
        "-L$(Join-Path $Toolchain 'lib')" `
        "-llibc.a" `
        -o $output
    if ($LASTEXITCODE -ne 0) {
        throw "Link failed with exit code $LASTEXITCODE"
    }

    & $objcopy -O ihex $output (Join-Path $buildDir "wifi_imu_vofa.hex")
    if ($LASTEXITCODE -ne 0) {
        throw "HEX conversion failed"
    }
    Copy-Item -LiteralPath (Join-Path $buildDir "wifi_imu_vofa.hex") `
        -Destination (Join-Path $repoRoot "wifi_imu963ra_vofa.hex") -Force
    & $objcopy -O binary $output (Join-Path $buildDir "wifi_imu_vofa.bin")
    if ($LASTEXITCODE -ne 0) {
        throw "BIN conversion failed"
    }
    & $sizeTool $output
    Write-Host "Firmware artifacts are in $buildDir"
}
finally {
    Pop-Location
}
