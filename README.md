# time-sync

A simple C++ program to synchronize Windows system time.

## Note

- The program **must be run as Administrator** since it uses Windows system components to resync the time.
- Without admin privileges, it will not work.

## How to Use

1. Right-click the program and select **“Run as administrator.”**
2. The program will automatically synchronize your system time using the Windows Time Service.

## Auto Start

To make it run automatically on logon:

1. Open **Task Scheduler** (`taskschd.msc`).
2. Create a **new task** (not a basic task).
3. Under **General**, check **“Run with highest privileges.”**
4. Under **Triggers**, select **“At log on.”**
5. Under **Actions**, choose **“Start a program”** and browse to your `time-sync.exe`.

## Build

Using MinGW (g++):
`g++ .\time-sync.cpp -o time-sync.exe -ladvapi32`
