#include <stdexcept>
#include <cstring>

#include "io.h"

namespace matryona
{

IOWindow::IOWindow()
	: parent(nullptr)
	, start(0)
	, length(0)
	, pos(0)
{
}

void IOWindow::init(IO *parent, size_t start, size_t length)
{
	this->parent = parent;
	this->start = start;
	this->length = length;
	if (start+length > parent->getLength())
		throw IOError();
}

void IOWindow::init(IO *parent, size_t length)
{
	this->parent = parent;
	this->length = length;
	start = parent->tell();
	if (start+length > parent->getLength())
		throw IOError();
}

size_t IOWindow::read(char *buffer, size_t length)
{
	parent->seek(start+pos);

	if (pos+length > this->length)
		length = this->length - pos;

	size_t read = parent->read(buffer, length);
	pos += read;
	return read;
}

bool IOWindow::seek(size_t position)
{
	if (position >= length)
		return false;
	pos = position;
	return true;
}

size_t IOWindow::tell()
{
	return pos;
}

size_t IOWindow::getLength()
{
	return length;
}

uint8_t readVintLength(IO *io)
{
	uint8_t value;
	if (io->read(reinterpret_cast<char*>(&value), 1) != 1)
		throw IOError();

	uint8_t length = 1;
	while (length <= 8 && value >> (8-length) == 0)
		++length;
	return length;
}

uint64_t readVint(IO *io)
{
	// Determine the length
	uint8_t length;
	{
		size_t pos = io->tell();
		length = readVintLength(io);
		io->seek(pos);
	}

	// Read the file
	char buffer[8];
	if (io->read(buffer, length) != length)
		throw IOError();

	// Read the first byte, and remove the leading 1
	uint64_t value = static_cast<uint8_t>(buffer[0]);
	value &= (0xFF >> length);

	// Read the rest, in big endian format
	for (uint8_t i = 1; i < length; ++i)
		value = (value << 8) | static_cast<uint8_t>(buffer[i]);

	return value;
}

int64_t readSVint(IO *io)
{
	static const int64_t subtr[8] = {
		0x3F, 0x1FFF, 0x0FFFFF, 0x07FFFFFF,
		0x03FFFFFFFF, 0x01FFFFFFFFFF,
		0x00FFFFFFFFFFFF, 0x007FFFFFFFFFFFFF,
	};

	size_t pos = io->tell();
	uint8_t length = readVintLength(io);
	io->seek(pos);
	uint64_t vint = readVint(io);
	return int64_t(vint)-subtr[length-1];
}

CIO::CIO(const char *filename)
	: length(0)
{
	f = std::fopen(filename, "r");
	if (!f)
		throw std::runtime_error("Could not open file");
}

CIO::~CIO()
{
	std::fclose(f);
}

size_t CIO::read(char *buffer, size_t length)
{
	return std::fread(buffer, 1, length, f);
}

bool CIO::seek(size_t position)
{
	return std::fseek(f, position, SEEK_SET) == 0;
}

size_t CIO::tell()
{
	return std::ftell(f);
}

size_t CIO::getLength()
{
	if (!length)
	{
		size_t old = std::ftell(f);
		std::fseek(f, 0, SEEK_END);
		length = std::ftell(f);
		std::fseek(f, old, SEEK_SET);
	}

	return length;
}

MemIO::MemIO(const char *buffer, size_t length)
	: buffer(buffer)
	, pos(0)
	, length(length)
{
}

size_t MemIO::read(char *buffer, size_t length)
{
	if (pos + length > this->length)
		length = this->length - pos;

	std::memcpy(buffer, this->buffer+pos, length);
	pos += length;
	return length;
}

bool MemIO::seek(size_t position)
{
	if (position >= length)
		return false;
	pos = position;
	return true;
}

size_t MemIO::tell()
{
	return pos;
}

size_t MemIO::getLength()
{
	return length;
}

} // matryona
