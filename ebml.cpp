#include "ebml.h"

using std::size_t;

namespace matryona
{

EBMLElement::EBMLElement()
{
}

EBMLElement::EBMLElement(IO *parent)
{
	id = readVint(parent);
	size = readVint(parent);
	io.init(parent, size);
}

EBMLElementIterator::EBMLElementIterator()
	: io(nullptr)
	, pos(0)
	, isValid(false)
{
}

EBMLElementIterator::EBMLElementIterator(IO *io)
	: io(io)
	, pos(0)
	, isValid(true)
{
	++(*this);
}

EBMLElement &EBMLElementIterator::operator*()
{
	return current;
}

EBMLElement *EBMLElementIterator::operator->()
{
	return &current;
}

EBMLElementIterator &EBMLElementIterator::operator++()
{
	if (!isValid)
		return *this;

	size_t oldPos = io->tell();
	if (!io->seek(pos))
	{
		isValid = false;
		return *this;
	}

	current = EBMLElement(io);
	pos = io->tell() + current.size;

	io->seek(oldPos);

	return *this;
}

bool EBMLElementIterator::operator==(const EBMLElementIterator &other) const
{
	if (isValid || other.isValid)
		return false;
	return true;
}

bool EBMLElementIterator::operator!=(const EBMLElementIterator &other) const
{
	return !(*this == other);
}

EBMLElementIterator &EBMLElementIterator::until(uint64_t id)
{
	while (true)
	{
		if (!isValid || current.id == id)
			return *this;

		++(*this);
	}
}

EBMLElementIterator &EBMLElementIterator::until(uint64_t id1, uint64_t id2)
{
	while (true)
	{
		if (!isValid || current.id == id1 || current.id == id2)
			return *this;

		++(*this);
	}
}

EBMLElementIterator EBMLElementIterator::end;

}
