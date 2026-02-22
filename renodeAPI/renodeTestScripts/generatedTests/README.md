# Renode .resc Test Script Suite

Complete peripheral validation scripts for testing software that uses Renode
to simulate whole machines. All scripts use the External Control Server API.

## Conventions (all scripts)

- Machine name: **`"test-machine"`** (multi-machine scripts use `"test-machine-1"` etc.)
- External Control Server: **port 5555** (`emulation CreateExternalControlServer "api-server" 5555`)
- Designed for **headless mode**: `renode --console --disable-gui`
- All interactive inputs **pre-set to nominal values** (no hanging prompts)
- All binaries **auto-download** from `dl.antmicro.com`
- All scripts end with **`start`** (emulation begins immediately)

## Quick Start

```bash
# Headless (connect to monitor separately)
renode --console --disable-gui -e "include @scripts/stm32h753zi/01_gpio_blinky.resc"

# With time limit
renode --console --disable-gui \
  -e "include @scripts/stm32h753zi/01_gpio_blinky.resc" \
  -e 'emulation RunFor "10"' \
  -e "quit"
```

## Script Inventory (41 scripts)

### STM32H753ZI / Nucleo Board — `stm32h753zi/` (12 scripts)

| # | Script | Peripheral(s) |
|---|--------|--------------|
| 01 | `01_gpio_blinky.resc` | GPIO output (LED blink) |
| 02 | `02_gpio_button.resc` | GPIO input (button) |
| 03 | `03_ethernet_echo_server.resc` | Ethernet MAC (TCP/UDP server) |
| 04 | `04_ethernet_echo_client.resc` | Ethernet MAC (TCP/UDP client) |
| 05 | `05_ethernet_ptp.resc` | Ethernet PTP |
| 06 | `06_crypto_aesgcm.resc` | Crypto (AES-GCM) |
| 07 | `07_crypto_aesgcm_it.resc` | Crypto (AES-GCM interrupt) |
| 08 | `08_crypto_aes_dma.resc` | Crypto + DMA |
| 09 | `09_qspi_readwrite.resc` | QSPI flash read/write |
| 10 | `10_qspi_memorymapped.resc` | QSPI memory-mapped |
| 11 | `11_qspi_xip.resc` | QSPI execute-in-place |
| 12 | `12_flash_erase_program.resc` | Internal flash |

### CAN Bus — `can/` (12 scripts)

| # | Script | Mode |
|---|--------|------|
| 01 | `01_mcan_loopback_socket.resc` | MCAN socket (loopback) |
| 02 | `02_mcan_driver_api.resc` | CAN API (loopback) |
| 03 | `03_mcan_timing.resc` | CAN timing (loopback) |
| 04 | `04_mcan_shell.resc` | CAN shell (loopback) |
| 05 | `05_isotp_loopback.resc` | ISOTP (loopback) |
| 06 | `06_isotp_conformance.resc` | ISOTP conformance (loopback) |
| 07 | `07_isotp_implementation.resc` | ISOTP implementation (loopback) |
| 08 | `08_can_counter_loopback.resc` | CAN counter (loopback) |
| 09 | `09_can_sockets_loopback.resc` | CAN sockets (loopback) |
| 10 | `10_multi_machine_can_exchange.resc` | **2 machines** CAN sockets |
| 11 | `11_multi_machine_can_counter.resc` | **2 machines** CAN counter |
| 12 | `12_multi_machine_isotp.resc` | **2 machines** ISOTP |

### Renesas DA14592 — `renesas_da14592/` (10 scripts)

| # | Script | Peripheral(s) |
|---|--------|--------------|
| 01 | `01_uart_hello_world.resc` | UART |
| 02 | `02_gpio.resc` | GPIO |
| 03 | `03_adc_gpadc.resc` | ADC (GPADC) + RESD |
| 04 | `04_watchdog.resc` | Watchdog |
| 05 | `05_timer_gpt.resc` | Timer (GPT) |
| 06 | `06_dma_mem_to_mem.resc` | DMA |
| 07 | `07_spi_adxl372.resc` | SPI + ADXL372 sensor |
| 08 | `08_i2c_echo.resc` | I2C + mock echo slave |
| 09 | `09_i2c_dma.resc` | I2C + DMA |
| 10 | `10_freertos_retarget.resc` | UART (FreeRTOS) |

### Renesas RA8M1 — `renesas_ra8m1/` (4 scripts)

| # | Script | Peripheral(s) |
|---|--------|--------------|
| 01 | `01_sci_uart.resc` | SCI UART (Segger RTT) |
| 02 | `02_agt_led_blink.resc` | AGT timer + GPIO LED |
| 03 | `03_sci_spi.resc` | SCI SPI |
| 04 | `04_sci_i2c.resc` | SCI I2C |

### Advanced / Cross-Cutting — `advanced/` (3 scripts)

| # | Script | Description |
|---|--------|-------------|
| 00 | `00_ci_runner_template.resc` | CI execution patterns and CLI flags |
| 01 | `01_python_assertions.resc` | Register checks, watchpoints, UART capture |
| 02 | `02_ethernet_echo_two_machines.resc` | 2-machine Ethernet echo test |

## Peripheral Coverage Matrix

| Peripheral | STM32H753ZI | DA14592 | RA8M1 | CAN Scripts |
|------------|:-----------:|:-------:|:-----:|:-----------:|
| GPIO       | ✅ 01,02    | ✅ 02   | ✅ 02 |             |
| UART       | ✅ (all)    | ✅ 01,10| ✅ 01 |             |
| ADC        |             | ✅ 03   |       |             |
| CAN        |             |         |       | ✅ 01-12    |
| SPI        |             | ✅ 07   | ✅ 03 |             |
| I2C        |             | ✅ 08,09| ✅ 04 |             |
| DMA        | ✅ 08       | ✅ 06,09|       |             |
| Timer      |             | ✅ 05   | ✅ 02 |             |
| Watchdog   |             | ✅ 04   |       |             |
| Ethernet   | ✅ 03-05    |         |       |             |
| Crypto     | ✅ 06-08    |         |       |             |
| QSPI       | ✅ 09-11    |         |       |             |
| Flash      | ✅ 12       |         |       |             |
