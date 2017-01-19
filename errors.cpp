#include <cstring>
#include <cstddef>

#include "errors.h"

using std::size_t;

namespace matryona
{

const char *IOError::what() const noexcept
{
	return "Read failed. File might be broken.";
}

InvalidFileFormatError::InvalidFileFormatError(const char *description)
	: message(description)
{
}

const char *InvalidFileFormatError::what() const noexcept
{
	return message;
}

} // matryona
