#pragma once

#include <cstddef>
#include <cstdint>

#include "errors.h"

namespace matryona
{

class IO
{
public:
	virtual std::size_t read(char *buffer, std::size_t length) = 0;
	virtual bool seek(std::size_t position) = 0;
	virtual std::size_t tell() = 0;
	virtual std::size_t getLength() = 0;
};

class IOWindow : public IO
{
public:
	IOWindow();

	void init(IO *parent, std::size_t start, std::size_t length);
	void init(IO *parent, std::size_t length);

	std::size_t read(char *buffer, std::size_t length);
	bool seek(std::size_t position);
	std::size_t tell();
	std::size_t getLength();

private:
	IO *parent;
	std::size_t start;
	std::size_t length;
	std::size_t pos;
};

std::uint8_t readVintLength(IO *io);
std::uint64_t readVint(IO *io);
std::int64_t readSVint(IO *io);

template<typename T, unsigned int size = sizeof(T)>
T swapEndianness(T value)
{
	T result = 0;
	const std::uint8_t *source = reinterpret_cast<std::uint8_t*>(&value);
	std::uint8_t *destination = reinterpret_cast<std::uint8_t*>(&result);

	for (unsigned int i = 0; i < size; i++)
		destination[i] = source[size-1-i];

	return result;
}

template <typename T = std::uint64_t>
T readUint(std::uint64_t len, IO *io)
{
	T value = 0;
	if (len > sizeof(T))
		len = sizeof(T);
	char buffer[len];
	if (io->read(buffer, len) != len)
		throw IOError();

	for (std::uint64_t i = 0; i < len; i++)
		value = (value << 8) | static_cast<std::uint8_t>(buffer[i]);

	// FIXME: Do this only if little endian
	value <<= (sizeof(T)-len)*8;
	return swapEndianness(value);
}

template <typename T>
struct equivalent_sized_uint
{
};

template <>
struct equivalent_sized_uint<float>
{
	typedef std::uint32_t type;
};

template <>
struct equivalent_sized_uint<double>
{
	typedef std::uint64_t type;
};

template <typename T = float>
T readFloat(std::uint64_t len, IO *io)
{
	typedef typename equivalent_sized_uint<T>::type U;
	char buffer[sizeof(T)];
	*reinterpret_cast<U*>(buffer) = readUint<U>(len, io);
	return *reinterpret_cast<T*>(buffer);
}

class CIO : public IO
{
public:
	CIO(const char *filename);
	~CIO();

	std::size_t read(char *buffer, std::size_t length);
	bool seek(std::size_t position);
	std::size_t tell();
	std::size_t getLength();

private:
	std::FILE *f;
	std::size_t length;
};

class MemIO : public IO
{
public:
	MemIO(const char *buffer, std::size_t length);

	std::size_t read(char *buffer, std::size_t length);
	bool seek(std::size_t position);
	std::size_t tell();
	std::size_t getLength();

private:
	const char *buffer;
	std::size_t pos;
	std::size_t length;
};

} // matryona
