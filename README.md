# I2C
C-based Keylogger project for the HSLU course I2C

Modules and tasks to be done:

| **Module**                               | **Task Description**                                                                                      | **Assignee**       |
|------------------------------------------|-----------------------------------------------------------------------------------------------------------|--------------------|
| System Hooks Initialization              | Initialize keyboard, mouse, and window hooks using Windows API.                                           |                    |
| Keyboard Hook                            | Implement keyboard hook to capture keystrokes.                                                            |                    |
| Mouse Hook                               | Implement mouse hook to capture movements and clicks.                                                     |                    |
| Window Tracking                          | Detect and log active window changes.                                                                     |                    |
| Keyboard Event Capture                   | Capture and process individual keystrokes.                                                                |                    |
| Mouse Event Capture                      | Capture and process mouse movements and clicks.                                                           |                    |
| Window Event Capture                     | Capture and process active window changes.                                                                |                    |
| Data Formatting with Timestamps          | Format captured events with timestamps for logging.                                                       |                    |
| Buffer Initialization                    | Initialize in-memory log buffer.                                                                          |                    |
| Adding Events to Buffer                  | Add formatted events to the log buffer.                                                                   |                    |
| Buffer Size Management                   | Monitor buffer size and determine when to flush data to the log file.                                     |                    |
| Writing Logs to File                     | Write buffered events to `keylog.txt` in append mode.                                                     |                    |
| Log File Encryption/Obfuscation          | Implement encryption or obfuscation for the log file.                                                     |                    |
| User-Friendly Termination (Hotkey)       | Implement a termination hotkey (e.g., Ctrl + Alt + Q).                                                    |                    |
| Graceful Shutdown and Resource Cleanup   | Ensure proper shutdown by flushing buffers, unhooking hooks, and releasing resources.                     |                    |
| Unit Testing for Each Module             | Develop and execute test cases for individual modules.                                                    |                    |
| Integration Testing                      | Test interaction between integrated modules.                                                              |                    |
| Performance Testing                      | Assess and optimize the keylogger's impact on system resources.                                           |                    |
| Code Documentation                       | Document the codebase with comments and descriptions.                                                     |                    |
| Project Report Compilation               | Compile a comprehensive project report.                                                                   |                    |
| User Guides and Ethical Guidelines       | Create user manuals and ethical usage guidelines.                                                         |                    |
