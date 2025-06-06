# Tool for comms with Mnemo (survey device)

## Compiling

- Linux `make -f Makefile.linux`
- MacOS `make -f Makefile.macos`


## Usage

Help
```
./mnemo 
Usage:
  ./mnemo import [--format raw|dmp] [--v2] [<tty>] <file.dmp>
      Import surveys from Mnemo and store it to file

  ./mnemo update [--baud <rate>] [<tty>] <file.hex>
      Upload firmware to Mnemo from Intel HEX file

  ./mnemo --version
      Show version information

  ./mnemo --help
      Show this help message

```

Import help
```
./mnemo import
Usage:
  ./mnemo import [--format raw|dmp] [--v2] [<tty>] <file.dmp>

Description:
  Retrieve survey data from the Nemo and save it to a file.
  If no TTY is specified, the tool attempts to autodetect it.

Options:
  --format raw|dmp   Output format (default: dmp)
  --v2               Use Mnemo protocol version 2
```

Update help
```
./mnemo update
Usage:
  ./mnemo update [--baud <rate>] [<tty>] <file.hex>

Description:
  Upload a firmware update to the Nemo using the specified
  Intel HEX (.hex) file. If no TTY is specified, the tool attempts to
  autodetect it.

Options:
  --baud <rate>      Serial baud rate (default: 460800)
```