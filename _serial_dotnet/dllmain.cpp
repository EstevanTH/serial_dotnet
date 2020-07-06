#include "pch.h"
#include <Python.h>
#include <vcclr.h>
#include <cstdint>
#include <exception>

#ifdef x86_64
	#define MODULE_PYTHON_NAME "_serial_dotnet_x86_64"
	#define MODULE_PYTHON_INIT PyInit__serial_dotnet_x86_64
#else
	#ifdef x86_32
		#define MODULE_PYTHON_NAME "_serial_dotnet_x86_32"
		#define MODULE_PYTHON_INIT PyInit__serial_dotnet_x86_32
	#else
		#define MODULE_PYTHON_NAME "_serial_dotnet"
		#define MODULE_PYTHON_INIT PyInit__serial_dotnet
	#endif
#endif

#using <System.dll>
using namespace System;
using namespace System::IO::Ports;

void acquireGil(PyThreadState** const gilThreadState) {
	// Acquires Python's Global Interpreter Lock if it was released
	// gilThreadState is NULL if no GIL release is involved.
	// *gilThreadState is NULL if the GIL is not released.
	// IMPORTANT: gilThreadState must be stored on the call stack to allow concurrent operations in the module!
	if (gilThreadState && *gilThreadState) {
		PyEval_RestoreThread(*gilThreadState);
		*gilThreadState = NULL;
	}
}

namespace text {
	// String & exception interoperability tools for .NET & Python
	
	gcroot<Text::UTF8Encoding^> encoder_utf8 = gcnew Text::UTF8Encoding;

	char const* const unknownError = "Unknown C++ error";

	char const* fromDotNetString(String^ toConvert) {
		// The returned value must be freed with delete[].
		array<unsigned char>^ converted1 = encoder_utf8->GetBytes(toConvert);
		int messageLength = converted1->Length;
		char* converted2 = new char[messageLength + 1];
		{
			int i = 0;
			for (; i < messageLength; ++i) {
				converted2[i] = converted1[i];
			}
			converted2[i] = 0;
		}
		return converted2;
	}

	PyObject* pyFromDotNetString(String^ toConvert) {
		array<unsigned char>^ converted1 = encoder_utf8->GetBytes(toConvert);
		cli::pin_ptr<unsigned char> converted1_ = &converted1[0];
		unsigned char* converted2 = converted1_;
		return PyUnicode_FromString((char*)converted2);
	}

	enum ExceptionDetails {
		MESSAGE,
		TYPE_MESSAGE,
		TYPE_MESSAGE_STACK
	};

	void setPyException(PyObject* exception, Exception^ e, ExceptionDetails details, PyThreadState** const gilThreadState = NULL) {
		String^ message_;
		switch (details) {
			case MESSAGE:
				message_ = e->Message;
				break;
			case TYPE_MESSAGE:
				message_ = String::Format("{0}: {1}", e->GetType()->Name, e->Message);
				break;
			default:
				message_ = e->ToString();
				break;
		}
		char const* message = fromDotNetString(message_);
		acquireGil(gilThreadState);
		PyErr_SetString(exception, message);
		delete[] message;
	}
};

// Exceptions that were introduced in Python 3.3 (not present in the stable ABI):
// Their values are set in MODULE_PYTHON_INIT().
// The C4273 warning is disabled because we cannot use the same type qualification as defined in "pyerrors.h".
#pragma warning(disable: 4273)
PyObject* PyExc_ConnectionError;
PyObject* PyExc_FileNotFoundError;
PyObject* PyExc_PermissionError;
PyObject* PyExc_TimeoutError;
#pragma warning(default: 4273)

// Alphabetical sorting of different exception types that may occur:
// gilThreadState is only for calls that release the Global Interpreter Lock, set it to NULL otherwise.
#define CATCH_USUAL(e, gilThreadState) \
	catch (System::ArgumentException^ e) { \
		/* unlikely to be a type error because type checks are done in PyArg_ParseTuple() */\
		text::setPyException(PyExc_ValueError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::DivideByZeroException^ e) { \
		text::setPyException(PyExc_ZeroDivisionError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::OverflowException^ e) { \
		text::setPyException(PyExc_OverflowError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::ArithmeticException^ e) { \
		text::setPyException(PyExc_ArithmeticError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::FormatException^ e) { \
		text::setPyException(PyExc_TypeError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::IndexOutOfRangeException^ e) { \
		text::setPyException(PyExc_IndexError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::InvalidCastException^ e) { \
		text::setPyException(PyExc_TypeError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::IO::EndOfStreamException^ e) { \
		text::setPyException(PyExc_EOFError, e, text::MESSAGE, gilThreadState); \
		return NULL; \
	} catch (System::IO::FileNotFoundException^ e) { \
		text::setPyException(PyExc_FileNotFoundError, e, text::TYPE_MESSAGE, gilThreadState); \
		return NULL; \
	} catch (System::IO::IOException^ e) { \
		text::setPyException(PyExc_OSError, e, text::TYPE_MESSAGE, gilThreadState); \
		return NULL; \
	} catch (System::Collections::Generic::KeyNotFoundException^ e) { \
		text::setPyException(PyExc_KeyError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::MemberAccessException^ e) { \
		text::setPyException(PyExc_AttributeError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::NotImplementedException^ e) { \
		text::setPyException(PyExc_NotImplementedError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::NotSupportedException^ e) { \
		text::setPyException(PyExc_OSError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::NullReferenceException^ e) { \
		text::setPyException(PyExc_TypeError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::OutOfMemoryException^ e) { \
		text::setPyException(PyExc_MemoryError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::RankException^ e) { \
		text::setPyException(PyExc_TypeError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::Security::SecurityException^ e) { \
		text::setPyException(PyExc_PermissionError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::Net::Sockets::SocketException^ e) { \
		text::setPyException(PyExc_ConnectionError, e, text::TYPE_MESSAGE, gilThreadState); \
		return NULL; \
	} catch (System::TimeoutException^ e) { \
		text::setPyException(PyExc_TimeoutError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::UnauthorizedAccessException^ e) { \
		text::setPyException(PyExc_PermissionError, e, text::TYPE_MESSAGE, gilThreadState); \
		return NULL; \
	} catch (System::ComponentModel::Win32Exception^ e) { \
		text::setPyException(PyExc_OSError, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (System::Exception^ e) { \
		text::setPyException(PyExc_Exception, e, text::TYPE_MESSAGE_STACK, gilThreadState); \
		return NULL; \
	} catch (std::exception const& e) { \
		acquireGil(gilThreadState); \
		PyErr_SetString(PyExc_Exception, e.what()); \
		return NULL; \
	} catch (...) { \
		acquireGil(gilThreadState); \
		PyErr_SetString(PyExc_Exception, text::unknownError); \
		return NULL; \
	}

gcroot<SerialPort^>* simpleParseInstance(PyObject* args) {
	unsigned PY_LONG_LONG instance;
	if (!PyArg_ParseTuple(args, "K", &instance)) {
		return NULL;
	}
	return (gcroot<SerialPort^>*)instance;
}

SerialPort^ simpleParseInstanceForPort(PyObject* args) {
	gcroot<SerialPort^>* instance = simpleParseInstance(args);
	if (instance) {
		return *instance;
	} else {
		return nullptr;
	}
}

#define simpleParseForPortOnly(args, exitNow, port) { \
	port = simpleParseInstanceForPort(args); \
	if (port == nullptr) { \
		if (exitNow) return NULL; \
	} \
}

#define simpleParseForPortAndMore(args, argsLayout, exitNow, port, ...) { \
	unsigned PY_LONG_LONG instance; \
	if (!PyArg_ParseTuple(args, argsLayout, &instance, __VA_ARGS__)) { \
		if (exitNow) return NULL; \
		port = nullptr; \
	} else { \
		port = *((gcroot<SerialPort^>*)instance); \
	} \
}

static PyObject* newInstance(PyObject* self, PyObject* args) {
	PyThreadState* gilThreadState = NULL;
	try {
		char const* portName;
		int32_t baudRate;
		uint8_t parity;
		int32_t dataBits;
		uint8_t stopBits;
		if (PyArg_ParseTuple(args, "sibib", &portName, &baudRate, &parity, &dataBits, &stopBits)) {
			gilThreadState = PyEval_SaveThread();
			gcroot<SerialPort^>* port = new gcroot<SerialPort^>;
			*port = gcnew SerialPort(
				gcnew String(portName),
				baudRate,
				(Parity)Enum::ToObject(Parity::typeid, parity),
				dataBits,
				(StopBits)Enum::ToObject(StopBits::typeid, stopBits)
			);
			acquireGil(&gilThreadState);
			return PyLong_FromVoidPtr(port);
		}
		else {
			return NULL;
		}
	} CATCH_USUAL(e, &gilThreadState)
}

static PyObject* deleteInstance(PyObject* self, PyObject* args) {
	try {
		gcroot<SerialPort^>* port = simpleParseInstance(args);
		if (port) {
			try {
				(*port)->Close();
			} catch (Exception^) {
				// nothing
			}
			delete port;
			Py_RETURN_NONE;
		}
		else {
			return NULL;
		}
	} CATCH_USUAL(e, NULL)
}

///
/// Accessors
///

// Properties

#define PROPERTY_SETTER_INT(name) \
static PyObject* set ## name(PyObject* self, PyObject* args) { \
	PyThreadState* gilThreadState = NULL; \
	try { \
		SerialPort^ port = nullptr; \
		int32_t value; \
		simpleParseForPortAndMore(args, "Ki", true, port, &value); \
		gilThreadState = PyEval_SaveThread(); \
		port->name = value; \
		acquireGil(&gilThreadState); \
		Py_RETURN_NONE; \
	} CATCH_USUAL(e, &gilThreadState) \
}
#define PROPERTY_GETTER_INT(name) \
static PyObject* get ## name(PyObject* self, PyObject* args) { \
	PyThreadState* gilThreadState = NULL; \
	try { \
		SerialPort^ port = nullptr; \
		simpleParseForPortOnly(args, true, port); \
		gilThreadState = PyEval_SaveThread(); \
		long value = port->name; \
		acquireGil(&gilThreadState); \
		return PyLong_FromLong(value); \
	} CATCH_USUAL(e, &gilThreadState) \
}

#define PROPERTY_SETTER_BOOL(name) \
static PyObject* set ## name(PyObject* self, PyObject* args) { \
	PyThreadState* gilThreadState = NULL; \
	try { \
		SerialPort^ port = nullptr; \
		int value; /*bool*/ \
		simpleParseForPortAndMore(args, "Kp", true, port, &value); \
		gilThreadState = PyEval_SaveThread(); \
		port->name = value; \
		acquireGil(&gilThreadState); \
		Py_RETURN_NONE; \
	} CATCH_USUAL(e, &gilThreadState) \
}
#define PROPERTY_GETTER_BOOL(name) \
static PyObject* get ## name(PyObject* self, PyObject* args) { \
	PyThreadState* gilThreadState = NULL; \
	try { \
		SerialPort^ port = nullptr; \
		simpleParseForPortOnly(args, true, port); \
		gilThreadState = PyEval_SaveThread(); \
		long value = port->name; \
		acquireGil(&gilThreadState); \
		return PyBool_FromLong(value); \
	} CATCH_USUAL(e, &gilThreadState) \
}

#define PROPERTY_SETTER_ENUM(Type, name) \
static PyObject* set ## name(PyObject* self, PyObject* args) { \
	PyThreadState* gilThreadState = NULL; \
	try { \
		SerialPort^ port = nullptr; \
		uint8_t value; \
		simpleParseForPortAndMore(args, "Kb", true, port, &value); \
		gilThreadState = PyEval_SaveThread(); \
		port->name = (Type)Enum::ToObject(Type::typeid, value); \
		acquireGil(&gilThreadState); \
		Py_RETURN_NONE; \
	} CATCH_USUAL(e, &gilThreadState) \
}
#define PROPERTY_GETTER_ENUM(Type, name) \
static PyObject* get ## name(PyObject* self, PyObject* args) { \
	PyThreadState* gilThreadState = NULL; \
	try { \
		SerialPort^ port = nullptr; \
		simpleParseForPortOnly(args, true, port); \
		gilThreadState = PyEval_SaveThread(); \
		long value = (uint8_t)port->name; \
		acquireGil(&gilThreadState); \
		return PyLong_FromLong(value); \
	} CATCH_USUAL(e, &gilThreadState) \
}

#define PROPERTY_SETTER_STR(name) \
static PyObject* set ## name(PyObject* self, PyObject* args) { \
	PyThreadState* gilThreadState = NULL; \
	try { \
		SerialPort^ port = nullptr; \
		char const* value; \
		simpleParseForPortAndMore(args, "Ks", true, port, &value); \
		gilThreadState = PyEval_SaveThread(); \
		port->name = gcnew String(value); \
		acquireGil(&gilThreadState); \
		Py_RETURN_NONE; \
	} CATCH_USUAL(e, &gilThreadState) \
}
#define PROPERTY_GETTER_STR(name) \
static PyObject* get ## name(PyObject* self, PyObject* args) { \
	PyThreadState* gilThreadState = NULL; \
	try { \
		SerialPort^ port = nullptr; \
		simpleParseForPortOnly(args, true, port); \
		gilThreadState = PyEval_SaveThread(); \
		String^ value = port->name; \
		acquireGil(&gilThreadState); \
		return text::pyFromDotNetString(value); \
	} CATCH_USUAL(e, &gilThreadState) \
}

PROPERTY_SETTER_INT(BaudRate)
PROPERTY_GETTER_INT(BaudRate)

PROPERTY_SETTER_BOOL(BreakState)
PROPERTY_GETTER_BOOL(BreakState)

PROPERTY_GETTER_INT(BytesToRead) // read-only

PROPERTY_GETTER_INT(BytesToWrite) // read-only

//CanRaiseEvents

PROPERTY_GETTER_BOOL(CDHolding) // read-only

//Container

PROPERTY_GETTER_BOOL(CtsHolding) // read-only

PROPERTY_SETTER_INT(DataBits)
PROPERTY_GETTER_INT(DataBits)

//DesignMode

PROPERTY_SETTER_BOOL(DiscardNull)
PROPERTY_GETTER_BOOL(DiscardNull)

PROPERTY_GETTER_BOOL(DsrHolding) // read-only

PROPERTY_SETTER_BOOL(DtrEnable)
PROPERTY_GETTER_BOOL(DtrEnable)

//Encoding

//Events

PROPERTY_SETTER_ENUM(Handshake, Handshake)
PROPERTY_GETTER_ENUM(Handshake, Handshake)

PROPERTY_GETTER_BOOL(IsOpen) // read-only

// NewLine

PROPERTY_SETTER_ENUM(Parity, Parity)
PROPERTY_GETTER_ENUM(Parity, Parity)

static PyObject* setParityReplace(PyObject* self, PyObject* args) {
	PyThreadState* gilThreadState = NULL;
	try {
		SerialPort^ port = nullptr;
		unsigned char parityReplace;
		simpleParseForPortAndMore(args, "KB", true, port, &parityReplace);
		gilThreadState = PyEval_SaveThread();
		port->ParityReplace = parityReplace;
		acquireGil(&gilThreadState);
		Py_RETURN_NONE;
	} CATCH_USUAL(e, &gilThreadState)
}
static PyObject* getParityReplace(PyObject* self, PyObject* args) {
	PyThreadState* gilThreadState = NULL;
	try {
		SerialPort^ port = nullptr;
		simpleParseForPortOnly(args, true, port);
		gilThreadState = PyEval_SaveThread();
		unsigned char parityReplace = port->ParityReplace;
		acquireGil(&gilThreadState);
		return PyLong_FromLong(parityReplace);
	} CATCH_USUAL(e, &gilThreadState)
}

PROPERTY_SETTER_STR(PortName)
PROPERTY_GETTER_STR(PortName)

PROPERTY_SETTER_INT(ReadBufferSize)
PROPERTY_GETTER_INT(ReadBufferSize)

PROPERTY_SETTER_INT(ReadTimeout)
PROPERTY_GETTER_INT(ReadTimeout)

// event configuration
PROPERTY_SETTER_INT(ReceivedBytesThreshold)
PROPERTY_GETTER_INT(ReceivedBytesThreshold)

PROPERTY_SETTER_BOOL(RtsEnable)
PROPERTY_GETTER_BOOL(RtsEnable)

//Site

PROPERTY_SETTER_ENUM(StopBits, StopBits)
PROPERTY_GETTER_ENUM(StopBits, StopBits)

PROPERTY_SETTER_INT(WriteBufferSize)
PROPERTY_GETTER_INT(WriteBufferSize)

PROPERTY_SETTER_INT(WriteTimeout)
PROPERTY_GETTER_INT(WriteTimeout)

// Methods

#define METHOD_NO_ARGUMENTS_RETURN_NONE(name) \
static PyObject* name(PyObject* self, PyObject* args) { \
	PyThreadState* gilThreadState = NULL; \
	try { \
		SerialPort^ port = nullptr; \
		simpleParseForPortOnly(args, true, port); \
		gilThreadState = PyEval_SaveThread(); \
		port->name(); \
		acquireGil(&gilThreadState); \
		Py_RETURN_NONE; \
	} CATCH_USUAL(e, &gilThreadState) \
}

#define METHOD_NO_ARGUMENTS_RETURN_STR(name) \
static PyObject* name(PyObject* self, PyObject* args) { \
	PyThreadState* gilThreadState = NULL; \
	try { \
		SerialPort^ port = nullptr; \
		simpleParseForPortOnly(args, true, port); \
		gilThreadState = PyEval_SaveThread(); \
		String^ value = port->name(); \
		acquireGil(&gilThreadState); \
		return text::pyFromDotNetString(value); \
	} CATCH_USUAL(e, &gilThreadState) \
}

static PyObject* Close(PyObject* self, PyObject* args) {
	PyThreadState* gilThreadState = NULL;
	try {
		SerialPort^ port = nullptr;
		simpleParseForPortOnly(args, true, port);
		gilThreadState = PyEval_SaveThread();
		try {
			port->Close();
		} catch (Exception^) {
			// nothing
		}
		acquireGil(&gilThreadState);
		Py_RETURN_NONE;
	} CATCH_USUAL(e, &gilThreadState)
}

//CreateObjRef(Type)

METHOD_NO_ARGUMENTS_RETURN_NONE(DiscardInBuffer)

METHOD_NO_ARGUMENTS_RETURN_NONE(DiscardOutBuffer)

//Dispose()

//Dispose(Boolean)

//Equals(Object)

//GetHashCode()

//GetLifetimeService()

static PyObject* GetPortNames(PyObject* self, PyObject* args) {
	PyThreadState* gilThreadState = NULL;
	try {
		gilThreadState = PyEval_SaveThread();
		cli::array<String^>^ ports_ = SerialPort::GetPortNames();
		int port_count = ports_->Length;
		acquireGil(&gilThreadState);
		PyObject* ports = PyList_New(port_count);
		for (int i = 0; i < port_count; ++i) {
			PyList_SetItem(ports, i, text::pyFromDotNetString(ports_[i]));
		}
		return ports;
	} CATCH_USUAL(e, &gilThreadState)
} // static

//GetService(Type)

//GetType()

//InitializeLifetimeService()

//MemberwiseClone()

//MemberwiseClone(Boolean)

METHOD_NO_ARGUMENTS_RETURN_NONE(Open)

static PyObject* _Read(SerialPort^ port, int32_t count, PyThreadState** const gilThreadState) {
	*gilThreadState = PyEval_SaveThread();
	unsigned char* buffer_buf = NULL;
	cli::pin_ptr<unsigned char> buffer_ptr = nullptr;
	cli::array<unsigned char>^ buffer = gcnew cli::array<unsigned char>(count);
	try {
		count = port->Read(buffer, 0, count);
		if (count) {
			buffer_ptr = &buffer[0]; // throws IndexOutOfRangeException if count == 0
			buffer_buf = buffer_ptr;
		}
	} catch (TimeoutException ^) {
		count = 0;
	}
	acquireGil(gilThreadState);
	return PyBytes_FromStringAndSize((char*)buffer_buf, count);
}

static PyObject* Read(PyObject* self, PyObject* args) {
	PyThreadState* gilThreadState = NULL;
	try {
		SerialPort^ port = nullptr;
		int32_t count;
		simpleParseForPortAndMore(args, "Ki", true, port, &count);
		return _Read(port, count, &gilThreadState);
	} CATCH_USUAL(e, &gilThreadState)
}

static PyObject* read_all(PyObject* self, PyObject* args) {
	PyThreadState* gilThreadState = NULL;
	try {
		SerialPort^ port = nullptr;
		simpleParseForPortOnly(args, true, port);
		return _Read(port, port->BytesToRead, &gilThreadState);
	} CATCH_USUAL(e, &gilThreadState)
}

//Read(Char[], Int32, Int32)

static PyObject* ReadByte(PyObject* self, PyObject* args) {
	PyThreadState* gilThreadState = NULL;
	try {
		SerialPort^ port = nullptr;
		simpleParseForPortOnly(args, true, port);
		gilThreadState = PyEval_SaveThread();
		int data_ = -1;
		try {
			data_ = port->ReadByte();
		} catch (TimeoutException ^) {
			// nothing
		}
		char data = data_;
		acquireGil(&gilThreadState);
		if (data_ >= 0) {
			return PyBytes_FromStringAndSize(&data, 1);
		}
		else {
			return PyBytes_FromStringAndSize(&data, 0);
		}
	} CATCH_USUAL(e, &gilThreadState)
}

//ReadChar()

METHOD_NO_ARGUMENTS_RETURN_STR(ReadExisting)

//ReadLine()

/* useless?
static PyObject* ReadTo(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		int32_t xxx;
		simpleParseForPortAndMore(args, "Ki", true, port, &xxx);
		port->ReadTo(); // TODO - convert & return
		Py_RETURN_NONE;
	} CATCH_USUAL(e, NULL)
}//(String)
*/

//ToString()

static PyObject* Write(PyObject* self, PyObject* args) {
	PyThreadState* gilThreadState = NULL;
	try {
		SerialPort^ port = nullptr;
		// Py_buffer unavailable with Py_LIMITED_API!
		//Py_buffer buffer_;
		//simpleParseForPortAndMore(args, "Ky*", true, port, &buffer_);
		//cli::array<unsigned char>^ buffer = gcnew cli::array<unsigned char>(buffer_.len);
		//for (int i = 0, end = buffer_.len; i < end; ++i) {
		//	buffer[i] = ((unsigned char*)buffer_.buf)[i];
		//}
		//PyBuffer_Release(&buffer_);
		char const* buffer_buf = NULL;
		int buffer_len = 0;
		simpleParseForPortAndMore(args, "Ky#", true, port, &buffer_buf, &buffer_len);
		gilThreadState = PyEval_SaveThread();
		cli::array<unsigned char>^ buffer = gcnew cli::array<unsigned char>(buffer_len);
		for (int i = 0; i < buffer_len; ++i) {
			buffer[i] = ((unsigned char*)buffer_buf)[i];
		}
		port->Write(buffer, 0, buffer->Length);
		acquireGil(&gilThreadState);
		Py_RETURN_NONE;
	} CATCH_USUAL(e, &gilThreadState)
}

//Write(Char[], Int32, Int32)

//Write(String)

//WriteLine(String)

// BaseStream

static PyObject* setBaseStreamReadTimeout(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		int32_t writeTimeout;
		simpleParseForPortAndMore(args, "Ki", true, port, &writeTimeout);
		port->BaseStream->ReadTimeout = writeTimeout;
		Py_RETURN_NONE;
	} CATCH_USUAL(e, NULL)
}
static PyObject* getBaseStreamReadTimeout(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		simpleParseForPortOnly(args, true, port);
		return PyLong_FromLong(port->BaseStream->ReadTimeout);
	} CATCH_USUAL(e, NULL)
}

static PyObject* setBaseStreamWriteTimeout(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		int32_t writeTimeout;
		simpleParseForPortAndMore(args, "Ki", true, port, &writeTimeout);
		port->BaseStream->WriteTimeout = writeTimeout;
		Py_RETURN_NONE;
	} CATCH_USUAL(e, NULL)
}
static PyObject* getBaseStreamWriteTimeout(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		simpleParseForPortOnly(args, true, port);
		return PyLong_FromLong(port->BaseStream->WriteTimeout);
	} CATCH_USUAL(e, NULL)
}

/* TODO
static PyObject* baseStreamBeginRead(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		int32_t xxx;
		simpleParseForPortAndMore(args, "Ki", true, port, &xxx);
		port->BaseStream->BeginRead(nullptr,0,0,nullptr,nullptr);
		Py_RETURN_NONE;
	} CATCH_USUAL(e, NULL)
}//(buffer As Byte(), offset As Integer, count As Integer, callback As System.AsyncCallback, state As Object) As System.IAsyncResult
*/

/* TODO
static PyObject* baseStreamBeginWrite(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		int32_t xxx;
		simpleParseForPortAndMore(args, "Ki", true, port, &xxx);
		port->BaseStream->BeginWrite(nullptr,0,0,nullptr,nullptr);
		Py_RETURN_NONE;
	} CATCH_USUAL(e, NULL)
}//(buffer As Byte(), offset As Integer, count As Integer, callback As System.AsyncCallback, state As Object) As System.IAsyncResult
*/

/* TODO useless?
static PyObject* baseStreamEndRead(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		int32_t xxx;
		simpleParseForPortAndMore(args, "Ki", true, port, &xxx);
		port->BaseStream->EndRead();
		Py_RETURN_NONE;
	} CATCH_USUAL(e, NULL)
}//(asyncResult As System.IAsyncResult) As Integer
*/

/* TODO useless?
static PyObject* baseStreamEndWrite(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		int32_t xxx;
		simpleParseForPortAndMore(args, "Ki", true, port, &xxx);
		port->BaseStream->EndWrite();
		Py_RETURN_NONE;
	} CATCH_USUAL(e, NULL)
}//(asyncResult As System.IAsyncResult)
*/

static PyObject* baseStreamFlush(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		simpleParseForPortOnly(args, true, port);
		port->BaseStream->Flush();
		Py_RETURN_NONE;
	} CATCH_USUAL(e, NULL)
}

/* TODO
static PyObject* baseStreamRead(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		int32_t xxx;
		simpleParseForPortAndMore(args, "Ki", true, port, &xxx);
		port->BaseStream->Read(nullptr,0,0);
		Py_RETURN_NONE;
	} CATCH_USUAL(e, NULL)
}//(buffer As Byte(), offset As Integer, count As Integer) As Integer
*/

static PyObject* baseStreamReadByte(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		simpleParseForPortOnly(args, true, port);
		return PyLong_FromLong(port->BaseStream->ReadByte()); // -1 if reached end
	} CATCH_USUAL(e, NULL)
}

/* TODO
static PyObject* baseStreamWrite(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		Py_buffer buffer_;
		simpleParseForPortAndMore(args, "Ky*", true, port, &buffer_);
		port->BaseStream->Write(nullptr,0,0);
		Py_RETURN_NONE;
	} CATCH_USUAL(e, NULL)
}//(buffer As Byte(), offset As Integer, count As Integer)
*/

static PyObject* baseStreamWriteByte(PyObject* self, PyObject* args) {
	try {
		SerialPort^ port = nullptr;
		unsigned char value;
		simpleParseForPortAndMore(args, "KB", true, port, &value);
		port->BaseStream->WriteByte(value);
		Py_RETURN_NONE;
	} CATCH_USUAL(e, NULL)
}


static PyObject* _test(PyObject* self, PyObject* args) {
	// Only for debugging:
	// nothing
	return NULL;
}


static PyMethodDef SerialDotnetMethods[] = {
	{"newInstance", newInstance, METH_VARARGS, "newInstance(portName: str, baudRate: int, parity: System.IO.Ports.Parity, dataBits: int, stopBits: System.IO.Ports.StopBits) -> int\n\
Create a new SerialPort with specified .NET arguments and return its address"},
	{"deleteInstance", deleteInstance, METH_VARARGS, "deleteInstance(instance: int)\n\
Destroy an existing SerialPort and free associated resources\n\
The Global Interpreter Lock is not released during this operation."},
	{"setBaudRate", setBaudRate, METH_VARARGS, ""},
	{"getBaudRate", getBaudRate, METH_VARARGS, ""},
	{"setBreakState", setBreakState, METH_VARARGS, ""},
	{"getBreakState", getBreakState, METH_VARARGS, ""},
	//{"setBytesToRead", setBytesToRead, METH_VARARGS, ""},
	{"getBytesToRead", getBytesToRead, METH_VARARGS, ""},
	//{"setBytesToWrite", setBytesToWrite, METH_VARARGS, ""},
	{"getBytesToWrite", getBytesToWrite, METH_VARARGS, ""},
	//{"setCDHolding", setCDHolding, METH_VARARGS, ""},
	{"getCDHolding", getCDHolding, METH_VARARGS, ""},
	//{"setCtsHolding", setCtsHolding, METH_VARARGS, ""},
	{"getCtsHolding", getCtsHolding, METH_VARARGS, ""},
	{"setDataBits", setDataBits, METH_VARARGS, ""},
	{"getDataBits", getDataBits, METH_VARARGS, ""},
	{"setDiscardNull", setDiscardNull, METH_VARARGS, ""},
	{"getDiscardNull", getDiscardNull, METH_VARARGS, ""},
	//{"setDsrHolding", setDsrHolding, METH_VARARGS, ""},
	{"getDsrHolding", getDsrHolding, METH_VARARGS, ""},
	{"setDtrEnable", setDtrEnable, METH_VARARGS, ""},
	{"getDtrEnable", getDtrEnable, METH_VARARGS, ""},
	{"setHandshake", setHandshake, METH_VARARGS, ""},
	{"getHandshake", getHandshake, METH_VARARGS, ""},
	//{"setIsOpen", setIsOpen, METH_VARARGS, ""},
	{"getIsOpen", getIsOpen, METH_VARARGS, ""},
	//{"setNewLine", setNewLine, METH_VARARGS, ""},
	//{"getNewLine", getNewLine, METH_VARARGS, ""},
	{"setParity", setParity, METH_VARARGS, ""},
	{"getParity", getParity, METH_VARARGS, ""},
	{"setParityReplace", setParityReplace, METH_VARARGS, ""},
	{"getParityReplace", getParityReplace, METH_VARARGS, ""},
	{"setPortName", setPortName, METH_VARARGS, ""},
	{"getPortName", getPortName, METH_VARARGS, ""},
	{"setReadBufferSize", setReadBufferSize, METH_VARARGS, ""},
	{"getReadBufferSize", getReadBufferSize, METH_VARARGS, ""},
	{"setReadTimeout", setReadTimeout, METH_VARARGS, ""},
	{"getReadTimeout", getReadTimeout, METH_VARARGS, ""},
	{"setReceivedBytesThreshold", setReceivedBytesThreshold, METH_VARARGS, ""},
	{"getReceivedBytesThreshold", getReceivedBytesThreshold, METH_VARARGS, ""},
	{"setRtsEnable", setRtsEnable, METH_VARARGS, ""},
	{"getRtsEnable", getRtsEnable, METH_VARARGS, ""},
	{"setStopBits", setStopBits, METH_VARARGS, ""},
	{"getStopBits", getStopBits, METH_VARARGS, ""},
	{"setWriteBufferSize", setWriteBufferSize, METH_VARARGS, ""},
	{"getWriteBufferSize", getWriteBufferSize, METH_VARARGS, ""},
	{"setWriteTimeout", setWriteTimeout, METH_VARARGS, ""},
	{"getWriteTimeout", getWriteTimeout, METH_VARARGS, ""},
	{"Close", Close, METH_VARARGS, ""},
	{"DiscardInBuffer", DiscardInBuffer, METH_VARARGS, ""},
	{"DiscardOutBuffer", DiscardOutBuffer, METH_VARARGS, ""},
	{"GetPortNames", GetPortNames, METH_VARARGS, ""},
	{"Open", Open, METH_VARARGS, ""},
	{"Read", Read, METH_VARARGS, "ReadByte(instance: int, count: int) -> bytes"},
	{"read_all", read_all, METH_VARARGS, "read_all(instance: int) -> bytes"},
	{"ReadByte", ReadByte, METH_VARARGS, "ReadByte(instance: int) -> bytes"},
	{"ReadExisting", ReadExisting, METH_VARARGS, ""},
	//{"ReadLine", ReadLine, METH_VARARGS, ""},
	//{"ReadTo", ReadTo, METH_VARARGS, ""},
	{"Write", Write, METH_VARARGS, ""},

	// These methods do not release the Global Interpreter Lock:
	{"setBaseStreamReadTimeout", setBaseStreamReadTimeout, METH_VARARGS, ""},
	{"getBaseStreamReadTimeout", getBaseStreamReadTimeout, METH_VARARGS, ""},
	{"setBaseStreamWriteTimeout", setBaseStreamWriteTimeout, METH_VARARGS, ""},
	{"getBaseStreamWriteTimeout", getBaseStreamWriteTimeout, METH_VARARGS, ""},
	//{"baseStreamBeginRead", baseStreamBeginRead, METH_VARARGS, ""}, // TODO
	//{"baseStreamBeginWrite", baseStreamBeginWrite, METH_VARARGS, ""}, // TODO
	//{"baseStreamEndRead", baseStreamEndRead, METH_VARARGS, ""},
	//{"baseStreamEndWrite", baseStreamEndWrite, METH_VARARGS, ""},
	{"baseStreamFlush", baseStreamFlush, METH_VARARGS, ""},
	//{"baseStreamRead", baseStreamRead, METH_VARARGS, ""}, // TODO
	{"baseStreamReadByte", baseStreamReadByte, METH_VARARGS, ""},
	//{"baseStreamWrite", baseStreamWrite, METH_VARARGS, ""}, // TODO
	{"baseStreamWriteByte", baseStreamWriteByte, METH_VARARGS, ""},

	{"_test", _test, METH_VARARGS, ""},

	{NULL, NULL, 0, NULL} // end mark
};

static struct PyModuleDef _serial_dotnetmodule = {
	PyModuleDef_HEAD_INIT,
	MODULE_PYTHON_NAME,
	NULL,
	-1,
	SerialDotnetMethods
};

PyMODINIT_FUNC MODULE_PYTHON_INIT(void) {
	// Initialize C++ variables:
	{
		PyObject* builtins_ = PyImport_ImportModule("builtins"); // module
		PyObject* builtins = NULL; // dict
		if (builtins_) {
			builtins = PyModule_GetDict(builtins_);
		}
		if (builtins) {
			PyObject* grabbedObject;
			#define grabBuiltinsVariable(keyInContainer, cppVariable, fallback) { \
				grabbedObject = PyDict_GetItemString(builtins, keyInContainer); \
				if (grabbedObject) { \
					cppVariable = grabbedObject; \
					Py_INCREF(grabbedObject); \
				} \
				else { \
					cppVariable = fallback; \
					Py_INCREF(fallback); \
				} \
			}
			grabBuiltinsVariable("ConnectionError", PyExc_ConnectionError, PyExc_OSError);
			grabBuiltinsVariable("FileNotFoundError", PyExc_FileNotFoundError, PyExc_OSError);
			grabBuiltinsVariable("PermissionError", PyExc_PermissionError, PyExc_OSError);
			grabBuiltinsVariable("TimeoutError", PyExc_TimeoutError, PyExc_OSError);
		}
		if (builtins_) {
			Py_DECREF(builtins_);
		}
	}

	// Initialize the Python module:
	return PyModule_Create(&_serial_dotnetmodule);
}
