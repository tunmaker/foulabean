#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <cstddef>
#include <iostream>
#include <thread>
#include <chrono>

#include "renodeInterface.h"
#include "renodeMachine.h"

using namespace renode;

void printSeparator(const char* title) {
  std::cout << "\n========== " << title << " ==========\n";
}

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv);

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
  engine.loadFromModule("digitwin", "Main");

  // =========================================================================
  // AUTO-LAUNCH RENODE
  // =========================================================================
  printSeparator("AUTO-LAUNCH RENODE");

  RenodeConfig config;
  config.renode_path = "~/packages/renode_portable/renode";
  config.script_path = "~/projects/digitwin/src/renodeAPI/renodeTestScripts/test-machine.resc";
  config.port = 5555;
  config.monitor_port = 5556;
  config.startup_timeout_ms = 15000;

  std::unique_ptr<ExternalControlClient> client;

  try {
    client = ExternalControlClient::launchAndConnect(config);
    std::cout << "Renode launched and connected!\n";
  } catch (const RenodeException& e) {
    std::cerr << "Failed to launch Renode: " << e.what() << '\n';
    std::cerr << "Falling back to manual connection...\n";

    // Fallback: try connecting to already-running Renode
    try {
      client = ExternalControlClient::connect("127.0.0.1", 5555);
      std::cout << "Connected to existing Renode instance\n";
    } catch (const std::exception& e2) {
      std::cerr << "Connection failed: " << e2.what() << '\n';
      return 1;
    }
  }

  // =========================================================================
  // HANDSHAKE
  // =========================================================================
  printSeparator("HANDSHAKE");

  if (client->performHandshake()) {
    std::cout << "Handshake successful\n";
  } else {
    std::cerr << "Handshake failed\n";
    return 1;
  }

  // =========================================================================
  // CONNECT MONITOR (after handshake)
  // =========================================================================
  printSeparator("CONNECT MONITOR");

  if (client->connectMonitor("127.0.0.1", config.monitor_port)) {
    std::cout << "Monitor connected on port " << config.monitor_port << "\n";
  } else {
    std::cerr << "Warning: Monitor connection failed, some features unavailable\n";
  }

  // =========================================================================
  // MONITOR COMMANDS (direct access)
  // =========================================================================
  printSeparator("MONITOR COMMANDS");

  Monitor* monitor = client->getMonitor();
  if (monitor) {
    std::cout << "Monitor connection available\n";

    // Execute custom command
    auto result = monitor->execute("version");
    if (!result.error) {
      std::cout << "Renode version: " << result.value << '\n';
    }

    // Query emulation state
    result = monitor->execute("emulation IsStarted");
    if (!result.error) {
      std::cout << "Emulation started: " << result.value << '\n';
    }

  } else {
    std::cout << "No monitor connection available\n";
  }

  // =========================================================================
  // GET MACHINE
  // =========================================================================
  printSeparator("GET MACHINE");

  Error err;
  auto machine = client->getMachine("stm32-machine", err);

  if (err) {
    std::cerr << "getMachine failed - code:" << err.code
              << " message:" << err.message << '\n';
    return 1;
  }

  std::cout << "Machine acquired:\n";
  std::cout << "  Name: " << machine->name() << '\n';
  std::cout << "  ID:   " << machine->id() << '\n';
  std::cout << "  Path: " << machine->path() << '\n';

  // =========================================================================
  // LIST PERIPHERALS
  // =========================================================================
  printSeparator("LIST PERIPHERALS");

  auto peripheralsResult = machine->listPeripherals();
  if (peripheralsResult.error) {
    std::cerr << "listPeripherals failed: " << peripheralsResult.error.message << '\n';
  } else {
    std::cout << "Found " << peripheralsResult.value.size() << " peripherals:\n";
    for (const auto& p : peripheralsResult.value) {
      std::cout << "  " << p.path << " (" << p.type << ")\n";
    }
  }

  // =========================================================================
  // LIFECYCLE CONTROL (pause/resume/reset)
  // =========================================================================
  printSeparator("LIFECYCLE CONTROL");

  // Check running state
  auto runningResult = machine->isRunning();
  if (!runningResult.error) {
    std::cout << "Machine running: " << (runningResult.value ? "Yes" : "No") << '\n';
  }

  // Pause
  std::cout << "Pausing simulation...\n";
  Error pauseErr = machine->pause();
  if (pauseErr) {
    std::cerr << "Pause failed: " << pauseErr.message << '\n';
  } else {
    std::cout << "Simulation paused\n";
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Resume
  std::cout << "Resuming simulation...\n";
  Error resumeErr = machine->resume();
  if (resumeErr) {
    std::cerr << "Resume failed: " << resumeErr.message << '\n';
  } else {
    std::cout << "Simulation resumed\n";
  }

  // =========================================================================
  // TIME CONTROL
  // =========================================================================
  printSeparator("TIME CONTROL");

  // Get current time
  auto timeResult = machine->getTime(TimeUnit::TU_MICROSECONDS);
  if (!timeResult.error) {
    std::cout << "Current simulation time: " << timeResult.value << " us\n";
  }

  // Run for 100ms
  std::cout << "Running simulation for 100ms...\n";
  Error runErr = machine->runFor(100, TimeUnit::TU_MILLISECONDS);
  if (runErr) {
    std::cerr << "runFor failed: " << runErr.message << '\n';
  } else {
    std::cout << "runFor completed\n";
  }

  // Get time again
  timeResult = machine->getTime(TimeUnit::TU_MICROSECONDS);
  if (!timeResult.error) {
    std::cout << "Simulation time after runFor: " << timeResult.value << " us\n";
  }

  // =========================================================================
  // GPIO OPERATIONS
  // =========================================================================
  printSeparator("GPIO OPERATIONS");

  Error gpioErr;
  auto gpio = machine->getGpio("sysbus.gpioPortA", gpioErr);

  if (gpio) {
    std::cout << "GPIO peripheral acquired (gpioPortA)\n";

    // Get initial state
    GpioState state;
    Error stateErr = gpio->getState(0, state);
    if (!stateErr) {
      std::cout << "GPIO pin 0 initial state: " << static_cast<int>(state)
                << " (0=Low, 1=High, 2=HighZ)\n";
    }

    // Set pin to High
    Error setErr = gpio->setState(0, GpioState::High);
    if (!setErr) {
      std::cout << "GPIO pin 0 set to High\n";

      // Read back
      gpio->getState(0, state);
      std::cout << "GPIO pin 0 readback: " << static_cast<int>(state) << '\n';
    } else {
      std::cerr << "GPIO setState failed: " << setErr.message << '\n';
    }

    // Set pin to Low
    setErr = gpio->setState(0, GpioState::Low);
    if (!setErr) {
      std::cout << "GPIO pin 0 set to Low\n";
    }

  } else {
    std::cerr << "Failed to get GPIO: " << gpioErr.message << '\n';
  }

  // =========================================================================
  // ADC OPERATIONS
  // =========================================================================
  printSeparator("ADC OPERATIONS");

  Error adcErr;
  auto adc = machine->getAdc("sysbus.adc1", adcErr);

  if (adc) {
    std::cout << "ADC peripheral acquired (adc1)\n";

    // Get channel count
    int channelCount = 0;
    Error countErr = adc->getChannelCount(channelCount);
    if (!countErr) {
      std::cout << "ADC channel count: " << channelCount << '\n';
    }

    // Read channel 0
    AdcValue value = 0;
    Error readErr = adc->getChannelValue(0, value);
    if (!readErr) {
      std::cout << "ADC channel 0 value: " << value << '\n';
    }

    // Set channel 0 value
    Error writeErr = adc->setChannelValue(0, 2.5);
    if (!writeErr) {
      std::cout << "ADC channel 0 set to 2.5V\n";

      // Read back
      adc->getChannelValue(0, value);
      std::cout << "ADC channel 0 readback: " << value << '\n';
    }

  } else {
    std::cerr << "Failed to get ADC: " << adcErr.message << '\n';
  }

  // =========================================================================
  // SYSBUS MEMORY OPERATIONS
  // =========================================================================
  printSeparator("SYSBUS MEMORY OPERATIONS");

  Error busErr;
  auto sysbus = machine->getSysBus("sysbus", busErr);

  if (sysbus) {
    std::cout << "SysBus acquired\n";

    Error ctxErr;
    auto busCtx = sysbus->getBusContext("", ctxErr);

    if (busCtx) {
      std::cout << "BusContext acquired\n";

      // Read from SRAM base (0x20000000 for STM32)
      uint64_t memValue = 0;
      Error readErr = busCtx->read(0x20000000, AccessWidth::AW_DWord, memValue);
      if (!readErr) {
        std::cout << "Memory @ 0x20000000: 0x" << std::hex << memValue << std::dec << '\n';
      } else {
        std::cerr << "Memory read failed: " << readErr.message << '\n';
      }

      // Write to memory
      Error writeErr = busCtx->write(0x20000000, AccessWidth::AW_DWord, 0xDEADBEEF);
      if (!writeErr) {
        std::cout << "Wrote 0xDEADBEEF to 0x20000000\n";

        // Read back
        busCtx->read(0x20000000, AccessWidth::AW_DWord, memValue);
        std::cout << "Memory @ 0x20000000 readback: 0x" << std::hex << memValue << std::dec << '\n';
      }

    } else {
      std::cerr << "Failed to get BusContext: " << ctxErr.message << '\n';
    }

  } else {
    std::cerr << "Failed to get SysBus: " << busErr.message << '\n';
  }

  // =========================================================================
  // RESET AND CLEANUP
  // =========================================================================
  printSeparator("RESET AND CLEANUP");

  // Reset machine
  std::cout << "Resetting machine...\n";
  Error resetErr = machine->reset();
  if (resetErr) {
    std::cerr << "Reset failed: " << resetErr.message << '\n';
  } else {
    std::cout << "Machine reset complete\n";
  }

  // Disconnect
  std::cout << "Disconnecting...\n";
  client->disconnect();
  std::cout << "Disconnected from Renode\n";

  // When client goes out of scope, Renode process is terminated (RAII)
  printSeparator("DEMO COMPLETE");
  std::cout << "Renode will be terminated when client is destroyed\n";

  // For demo purposes, exit immediately instead of running Qt event loop
  // return app.exec();
  return 0;
}
