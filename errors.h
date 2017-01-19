#pragma once

#include <stdexcept>

namespace matryona
{

struct ParseError : public std::exception
{
	virtual const char *what() const noexcept = 0;
};

struct IOError : public ParseError
{
	const char *what() const noexcept;
};

struct InvalidFileFormatError : public ParseError
{
	InvalidFileFormatError(const char *description = "File format is unknown or unsupported");
	const char *what() const noexcept;
	const char *message;
};

} // matryona

