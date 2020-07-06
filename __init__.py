"""Replacement for pySerial using .NET's SerialPort class
This is probably useless because CLR's overhead significantly increases the CPU usage.
This module is mostly intended for educational purposes.
"""


try:
    import serial_dotnet._serial_dotnet_x86_32 as _serial_dotnet
except ImportError:
    import serial_dotnet._serial_dotnet_x86_64 as _serial_dotnet

from enum import IntEnum
from io import RawIOBase


# From pySerial

try:
    from serial.serialutil import \
        XON, XOFF, \
        CR, LF, \
        PARITY_NONE, PARITY_EVEN, PARITY_ODD, PARITY_MARK, PARITY_SPACE, \
        STOPBITS_ONE, STOPBITS_ONE_POINT_FIVE, STOPBITS_TWO, \
        FIVEBITS, SIXBITS, SEVENBITS, EIGHTBITS
except ImportError:
    XON = b'\x11'
    XOFF = b'\x13'
    CR = b'\r'
    LF = b'\n'
    PARITY_NONE, PARITY_EVEN, PARITY_ODD, PARITY_MARK, PARITY_SPACE = \
        'N', 'E', 'O', 'M', 'S'
    STOPBITS_ONE, STOPBITS_ONE_POINT_FIVE, STOPBITS_TWO = (1, 1.5, 2)
    FIVEBITS, SIXBITS, SEVENBITS, EIGHTBITS = (5, 6, 7, 8)

try:
    from serial.serialutil import \
        SerialException, \
        SerialTimeoutException, \
        writeTimeoutError, \
        portNotOpenError, \
        SerialBase
except ImportError:
    class SerialException(IOError):
        pass

    class SerialTimeoutException(SerialException):
        pass

    writeTimeoutError = SerialTimeoutException('Write timeout')
    portNotOpenError = SerialException('Attempting to use a port that is not open')

    class SerialBase():
        """Minimalistic class containing missing important features"""
        def __repr__(self):
            return "%s<id=0x%x, open=%s>(port=%r, baudrate=%r, bytesize=%r, parity=%r, stopbits=%r, timeout=%r, xonxoff=%r, rtscts=%r, dsrdtr=%r)" % (
                    self.__class__.__name__,
                    id(self),
                    self.is_open,
                    self.portstr,
                    self.baudrate,
                    self.bytesize,
                    self.parity,
                    self.stopbits,
                    self.timeout,
                    self.xonxoff,
                    self.rtscts,
                    self.dsrdtr,
            )

        def readable(self):
            return True

        def writable(self):
            return True

        def seekable(self):
            return False

        def readinto(self, b):
            data = self.read(len(b))
            n = len(data)
            try:
                b[:n] = data
            except TypeError as err:
                import array
                if not isinstance(b, array.array):
                    raise err
                b[:n] = array.array('b', data)
            return n

        def __enter__(self):
            return self

        def __exit__(self, *args, **kwargs):
            self.close()

        def send_break(self, duration=0.25):
            if not self.is_open:
                raise portNotOpenError
            self.break_condition = True
            time.sleep(duration)
            self.break_condition = False

        def read_until(self, terminator=LF, size=None):
            lenterm = len(terminator)
            line = bytearray()
            while True:
                c = self.read(1)
                if c:
                    line += c
                    if line[-lenterm:] == terminator:
                        break
                    if size is not None and len(line) >= size:
                        break
                else:
                    break
            return bytes(line)

        def iread_until(self, *args, **kwargs):
            while True:
                line = self.read_until(*args, **kwargs)
                if not line:
                    break
                yield line


# From .NET

get_port_names = _serial_dotnet.GetPortNames

class Handshake(IntEnum):
    """System.IO.Ports.Handshake"""
    None_ = 0
    RequestToSend = 2
    RequestToSendXOnXOff = 3
    XOnXOff = 1
    
    @classmethod
    def from_pyserial(cls, xonxoff, rtscts):
        result = cls.valuesFromPyserial.get((xonxoff, rtscts), None)
        if result is None:
            raise ValueError(
                "Invalid pySerial values: (xonxoff=%s, rtscts=%s)" %
                (str(xonxoff), str(rtscts))
            )
        return result
    
    @classmethod
    def to_pyserial(cls, handshake):
        values = cls.valuesToPyserial.get(handshake, None)
        if values is None:
            raise ValueError(
                "Invalid .NET handshake value: %s" % str(handshake))
        return values
    
    @classmethod
    def to_pyserial_xonxoff(cls, handshake):
        return cls.to_pyserial(handshake)[0]
    
    @classmethod
    def to_pyserial_rtscts(cls, handshake):
        return cls.to_pyserial(handshake)[1]
    
Handshake.valuesFromPyserial = {
    # (xonxoff, rtscts): Handshake_value
    (False, False): Handshake.None_,
    (False, True): Handshake.RequestToSend,
    (True, True): Handshake.RequestToSendXOnXOff,
    (True, False): Handshake.XOnXOff,
}
Handshake.valuesToPyserial = {
    # Handshake_value: (xonxoff, rtscts)
    Handshake.None_: (False, False),
    Handshake.RequestToSend: (False, True),
    Handshake.RequestToSendXOnXOff: (True, True),
    Handshake.XOnXOff: (True, False),
}

class Parity(IntEnum):
    """System.IO.Ports.Parity"""
    Even = 2
    Mark = 3
    None_ = 0
    Odd = 1
    Space = 4
    
    @classmethod
    def from_pyserial(cls, parity):
        result = cls.valuesFromPyserial.get(parity, None)
        if result is None:
            raise ValueError("Invalid pySerial parity value: %s" % str(parity))
        return result
    
    @classmethod
    def to_pyserial(cls, parity):
        result = cls.valuesToPyserial.get(parity, None)
        if result is None:
            raise ValueError("Invalid .NET parity value: %s" % str(parity))
        return result
    
Parity.valuesFromPyserial = {
    PARITY_NONE: Parity.None_,
    PARITY_EVEN: Parity.Even,
    PARITY_ODD: Parity.Odd,
    PARITY_MARK: Parity.Mark,
    PARITY_SPACE: Parity.Space,
}
Parity.valuesToPyserial = {
    Parity.None_: PARITY_NONE,
    Parity.Even: PARITY_EVEN,
    Parity.Odd: PARITY_ODD,
    Parity.Mark: PARITY_MARK,
    Parity.Space: PARITY_SPACE,
}

class StopBits(IntEnum):
    """System.IO.Ports.StopBits"""
    None_ = 0 # invalid value
    One = 1
    OnePointFive = 3
    Two = 2
    
    @classmethod
    def from_pyserial(cls, stopBits):
        result = cls.valuesFromPyserial.get(stopBits, None)
        if result is None:
            raise ValueError("Invalid pySerial stop bits value: %s" % str(stopBits))
        return result
    
    @classmethod
    def to_pyserial(cls, stopBits):
        result = cls.valuesToPyserial.get(stopBits, None)
        if result is None:
            raise ValueError("Invalid .NET stop bits value: %s" % str(stopBits))
        return result
    
StopBits.valuesFromPyserial = {
    STOPBITS_ONE: StopBits.One,
    STOPBITS_ONE_POINT_FIVE: StopBits.OnePointFive,
    STOPBITS_TWO: StopBits.Two,
}
StopBits.valuesToPyserial = {
    StopBits.None_: None,
    StopBits.One: STOPBITS_ONE,
    StopBits.OnePointFive: STOPBITS_ONE_POINT_FIVE,
    StopBits.Two: STOPBITS_TWO,
}


# Brand new

class Serial(SerialBase):
    def __init__(
            self,
            port=None,
            baudrate=9600,
            bytesize=EIGHTBITS,
            parity=PARITY_NONE,
            stopbits=STOPBITS_ONE,
            timeout=None,
            xonxoff=False,
            rtscts=False,
            write_timeout=None,
            dsrdtr=False,
            inter_byte_timeout=None,
            **kwargs
        ):
        self._port_mgr = _serial_dotnet.newInstance(
            port is None and "NUL" or port,
            baudrate,
            Parity.from_pyserial(parity),
            bytesize,
            StopBits.from_pyserial(stopbits)
        )
        if timeout is not None:
            self.timeout = timeout
        self.xonxoff = xonxoff
        self.rtscts = rtscts
        if write_timeout is not None:
            self.write_timeout = write_timeout
        if port is not None:
            self.open()
    
    def __del__(self):
        _serial_dotnet.deleteInstance(self._port_mgr)

    @property
    def is_open(self):
        return _serial_dotnet.getIsOpen(self._port_mgr)
    
    def open(self):
        _serial_dotnet.Open(self._port_mgr)

    def _reconfigure_port(self):
        """Does nothing"""
        pass

    @property
    def bytesize(self):
        return _serial_dotnet.getIsOpen(self._port_mgr)
    
    def close(self):
        _serial_dotnet.Close(self._port_mgr)

    @property
    def port(self):
        port = _serial_dotnet.getPortName(self._port_mgr)
        if port == "NUL":
            port = None
        return port

    @port.setter
    def port(self, port):
        was_open = self.is_open
        if was_open:
            self.close()
        _serial_dotnet.setPortName(
            self._port_mgr, port is None and "NUL" or port)
        if was_open:
            self.open()


    @property
    def baudrate(self):
        return _serial_dotnet.getBaudRate(self._port_mgr)

    @baudrate.setter
    def baudrate(self, baudrate):
        _serial_dotnet.setBaudRate(self._port_mgr, baudrate)


    @property
    def bytesize(self):
        return _serial_dotnet.getDataBits(self._port_mgr)

    @bytesize.setter
    def bytesize(self, bytesize):
        _serial_dotnet.setDataBits(self._port_mgr, bytesize)



    @property
    def parity(self):
        return Parity.to_pyserial(_serial_dotnet.getParity(self._port_mgr))

    @parity.setter
    def parity(self, parity):
        _serial_dotnet.setParity(self._port_mgr, Parity.from_pyserial(parity))



    @property
    def stopbits(self):
        return StopBits.to_pyserial(_serial_dotnet.getStopBits(self._port_mgr))

    @stopbits.setter
    def stopbits(self, stopbits):
        _serial_dotnet.setStopBits(self._port_mgr, StopBits.from_pyserial(stopbits))


    @property
    def timeout(self):
        timeout = _serial_dotnet.getReadTimeout(self._port_mgr)
        if timeout == -1:
            timeout = None
        else:
            timeout = timeout * 0.001
        return timeout

    @timeout.setter
    def timeout(self, timeout):
        if timeout == None:
            timeout = -1
        else:
            timeout = int(timeout * 1000.)
        _serial_dotnet.setReadTimeout(self._port_mgr, timeout)


    @property
    def write_timeout(self):
        timeout = _serial_dotnet.getWriteTimeout(self._port_mgr)
        if timeout == -1:
            timeout = None
        else:
            timeout = timeout * 0.001
        return timeout

    @write_timeout.setter
    def write_timeout(self, timeout):
        if timeout == None:
            timeout = -1
        else:
            timeout = int(timeout * 1000.)
        _serial_dotnet.setWriteTimeout(self._port_mgr, timeout)


    @property
    def inter_byte_timeout(self):
        """TODO (always None)"""
        return None

    @inter_byte_timeout.setter
    def inter_byte_timeout(self, ic_timeout):
        """TODO"""
        pass


    @property
    def xonxoff(self):
        return Handshake.to_pyserial_xonxoff(_serial_dotnet.getHandshake(self._port_mgr))

    @xonxoff.setter
    def xonxoff(self, xonxoff):
        _serial_dotnet.setHandshake(self._port_mgr, Handshake.from_pyserial(xonxoff, self.rtscts))


    @property
    def rtscts(self):
        return Handshake.to_pyserial_rtscts(_serial_dotnet.getHandshake(self._port_mgr))

    @rtscts.setter
    def rtscts(self, rtscts):
        _serial_dotnet.setHandshake(self._port_mgr, Handshake.from_pyserial(self.xonxoff, rtscts))


    @property
    def dsrdtr(self):
        """Always False (possible incompatibilities)"""
        return False

    @dsrdtr.setter
    def dsrdtr(self, dsrdtr=None):
        pass


    @property
    def rts(self):
        return _serial_dotnet.getRtsEnable(self._port_mgr)

    @rts.setter
    def rts(self, value):
        _serial_dotnet.setRtsEnable(self._port_mgr, value)

    @property
    def dtr(self):
        return _serial_dotnet.getDtrEnable(self._port_mgr)

    @dtr.setter
    def dtr(self, value):
        _serial_dotnet.setDtrEnable(self._port_mgr, value)

    @property
    def break_condition(self):
        return _serial_dotnet.getBreakState(self._port_mgr)

    @break_condition.setter
    def break_condition(self, value):
        _serial_dotnet.setBreakState(self._port_mgr, value)

    @property
    def rs485_mode(self):
        """Not implemented"""
        return None

    @rs485_mode.setter
    def rs485_mode(self, rs485_settings):
        """Not implemented"""
        raise NotImplementedError()

    def get_settings(self):
        """Not implemented"""
        raise NotImplementedError()

    def apply_settings(self, d):
        """Not implemented"""
        raise NotImplementedError()

    def read_all(self):
        return _serial_dotnet.read_all(self._port_mgr)

    @property
    def in_waiting(self):
        return _serial_dotnet.getBytesToRead(self._port_mgr)

    def read(self, size=1):
        data = None
        if size == 1:
            return _serial_dotnet.ReadByte(self._port_mgr)
        else:
            data = _serial_dotnet.Read(self._port_mgr, size)
            #print(data)
            return data
    
    def write(self, data):
        _serial_dotnet.Write(self._port_mgr, data)

    def flush(self):
        while self.out_waiting:
            time.sleep(0.05)

    def reset_input_buffer(self):
        _serial_dotnet.DiscardInBuffer(self._port_mgr)

    def reset_output_buffer(self):
        _serial_dotnet.DiscardOutBuffer(self._port_mgr)
    

    @property
    def cts(self):
        return _serial_dotnet.getCtsHolding(self._port_mgr)

    @property
    def dsr(self):
        return _serial_dotnet.getDsrHolding(self._port_mgr)

    @property
    def ri(self):
        """TODO (always False)"""
        return False

    @property
    def cd(self):
        return _serial_dotnet.getCDHolding(self._port_mgr)

    def set_buffer_size(self, rx_size=4096, tx_size=None):
        if tx_size is None:
            tx_size = rx_size
        _serial_dotnet.setReadBufferSize(self._port_mgr, rx_size)
        _serial_dotnet.setWriteBufferSize(self._port_mgr, tx_size)

    #def set_output_flow_control(self, enable=True)

    @property
    def out_waiting(self):
        return _serial_dotnet.getBytesToWrite(self._port_mgr)
