# HDMotion2
## What is this?
HDMotion2 is port of [HDMotion](http://hdmotion.pingerthinger.com/) for modern systems.

## System requirements
* Hard disk drive
* Windows

## How to use
1. Run terminal as administrator
2. HDmotion2.exe `<diskID>`

### How to get disk ID
1. Open PowerShell
2. `get-disk`
3. `Number` Column is for diskID

## Build requirements
* C Compiler
  * Clang 19 (Tested)
  * GCC 14 (Tested)

## TODOs
* Make POSIX compatible
* Measure seektime benchmark during the run

## Notes
* The program will not run as intended if any disk cache software is running
  * If disk is attached via iSCSI, the program will not run as intended as well
    due to iSCSI's nature. (iSCSI target will cache drive access anyway)

