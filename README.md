# I2C
Project for the HSLU course I2C.
---
Alvin Veliyath, Flora Gashi, Fabian Lehmann


# Keylogger Project
A keylogger application that captures keyboard and mouse input events and logs them. The project includes event recording mechanisms, logging features, and efficient memory usage.


## Table of Contents

1. [Features](#features)
2. [Technical Details](#technical-details)
3. [Requirements](#requirements)
4. [Installation](#installation)
5. [Usage](#usage)
6. [Project Structure](#project-structure)
7. [File Structure](#project-structure)
8. [Configuration](#configuration)


***
## 1. Features

- Logs keyboard input and mouse clicks.
- Saves data into a log file.
- Configurable options such as buffer usage and log file rotation.
- Debugging support for developers.

***
## 2. Technical Details

- **Programming Language:** C
- **Target Platform:** Windows (Windows 7 or later)
- **Dependencies:** Windows API, Makefile system
- **Logging:** Logs are stored in a file under the `logs` directory.

***
## 3. Requirements

- **Operating System:** Windows
- **Compiler:** MinGW or any compatible GCC compiler
- **Tools:** `make` (to compile the project)

***
## 4. Installation

- **Clone the repository:**
   ```bash
   git clone https://github.com/aveliyath/i2c.git
   cd i2c-1

- **Install dependencies:**
GCC compiler and make need to be installed.

- **Compile the project:**
   run the following command in the root directory:

   ```bash
   make debug
   
- **Ready to run:**
   The executable file keylogger.exe will be created and can be run from the terminal.
   ```bash
   ./keylogger.exe


***
## 5. Usage

The keylogger starts and records events. Logs are saved in the logs directory.

To stop the programm press **Ctrl+C**. Logs will be automatically saved.

Open the file logs/keylog.txt to see recorded events.


***
## 6. Project Structure

- `src/`: Contains source code files (`.c`).
- `include/`: Contains header files (`.h`).
- `obj/`: Contains object files (`.o`) generated during compilation.
- `logs/`: Contains generated log files.
- `Makefile`: Instructions for compiling the project.

***
## 7. File Structure

- **src/hooks.c:** Contains the implementation of hooks for capturing keyboard, mouse, and window events.
- **src/buffer.c:** Manages buffering of captured events for efficient logging.
- **src/logger.c:** Handles logging events to files with optional rotation and buffering.
- **include/hooks.h:** Header file defining the structure and API for event hooks.
- **include/buffer.h:** Header file for buffer management functions and configuration.
- **include/logger.h:** Header file defining the logger interface and configuration.

***
## 8. Configuration

You can modify configuration options such as log file paths, buffer sizes, and event filters by editing the following files:

- src/logger.c
- src/buffer.c
- src/capture.c


   






