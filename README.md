# serial_dotnet

This Python module is composed of 2 parts:

- a Python module written in C++, using .NET features (including the [`SerialPort`](https://docs.microsoft.com/en-us/dotnet/api/system.io.ports.serialport) class)
- a Python module written in Python, which wraps the latter module into a [pySerial](https://pypi.org/project/pyserial/)-compatible API

When you have pySerial installed, the `serial_dotnet.Serial` class derives from pySerial's `serial.Serial`.

## What is the point?

At some point, I thought that pySerial introduces a CPU penality, but I proved that I was wrong. Actually, **serial_dotnet** suffers from an overhead caused by .NET with the [Common Language Runtime](https://docs.microsoft.com/en-us/dotnet/standard/clr), which causes much more CPU usage.

I decided to finish the work anyway because I think it is relevent for educational purposes. I cleaned the code and I added [Global Interpeter Lock](https://wiki.python.org/moin/GlobalInterpreterLock) releases to optimize calls.

The code is not valuable for what it does, but it is for the good example is represents. Mixing Python, .NET and C++ altogether is a costly and risky task, so I give anyone a base code that works.

## Improvements

- It is possible to avoid most of the code in the wrapper `serial_dotnet\__init__.py` and move almost everything into C++. But it will complicate the code, which is bad for both my time and learners.
- When running the linker, a [`LNK4248`](https://docs.microsoft.com/en-us/cpp/error-messages/tool-errors/linker-tools-warning-lnk4248) warning shows up. I do not understand it. The mentioned token changed after modifications. My opinion is that it is a CLR-related bug.
- A crash may occur in your program when the thread that created a `Serial` instance is terminated. My analysis is the following: the destruction of the `SerialPort` is asynchronous and when it is over, an event callback is run in a destructed context, causing a `System.ObjectDisposedException` error, which crashes the Python interpreter subsequently. Because event callback is run from a separate thread, the error cannot be caught. Please help!

## Contents

The module folder should be named `serial_dotnet`, it is not an obligation. In this folder, you have:

- the wrapper module `__init__.py`
- the C++ interface module for 32-bit x86 editions of Python `_serial_dotnet_x86_32.pyd`
- the C++ interface module for 64-bit x86 editions of Python `_serial_dotnet_x86_64.pyd`

## Notes

This module is intended for Windows XP and Python 3.4 minimum. The C++ Python module should run on Python 3.2 minimum because the stable Python 3 ABI was used. The wrapper Python module may fail on versions below Python 3.4 because I was lazy enough not to install Python 3.2.

The module was tested, but it was not deeply tested. Using it in a production environment might be insecure.

If you need any help, I please go to the Issues tab.
