#pragma once

#include <cstddef>
#include <cstdint>

#include "io.h"

namespace matryona
{

struct EBMLElement
{
	EBMLElement();
	EBMLElement(IO *io);

	uint64_t id;
	uint64_t size;
	IOWindow io;
};

class EBMLElementIterator
{
public:
	EBMLElementIterator(IO *io);
	EBMLElement &operator*();
	EBMLElement *operator->();
	EBMLElementIterator &operator++();
	bool operator==(const EBMLElementIterator &other) const;
	bool operator!=(const EBMLElementIterator &other) const;
	EBMLElementIterator &until(uint64_t id);
	EBMLElementIterator &until(uint64_t id1, uint64_t id2);

	static EBMLElementIterator end;
private:
	IO *io;
	size_t pos;
	bool isValid;
	EBMLElement current;

	EBMLElementIterator();
};

namespace id
{
	const uint64_t EBML = 0xA45DFA3;
	const uint64_t EBMLVersion = 0x286;
	const uint64_t EBMLReadVersion = 0x2F7;
	const uint64_t EBMLMaxIDLength = 0x2F2;
	const uint64_t EBMLMaxSizeLength = 0x2F3;
	const uint64_t DocType = 0x282;
	const uint64_t DocTypeVersion = 0x287;
	const uint64_t DocTypeReadVersion = 0x285;
	const uint64_t Void = 0x6C;
	const uint64_t CRC32 = 0x3F;
	const uint64_t Segment = 0x8538067;
	const uint64_t SegmentInfo = 0x549A966;
	const uint64_t SeekHead = 0x14D9B74;
	const uint64_t Cluster = 0xF43B675;
	const uint64_t Tracks = 0x654AE6B;
	const uint64_t Cues = 0xC53BB6B;
	const uint64_t Attachments = 0x941A469;
	const uint64_t Chapters = 0x43A770;
	const uint64_t Tags = 0x254C367;
	const uint64_t TrackEntry = 0x2E;
	const uint64_t TrackNumber = 0x57;
	const uint64_t TrackUID = 0x33C5;
	const uint64_t TrackType = 0x03;
	const uint64_t FlagEnabled = 0x39;
	const uint64_t FlagDefault = 0x08;
	const uint64_t FlagLacing = 0x1C;
	const uint64_t CodecID = 0x06;
	const uint64_t CodecName = 0x58688;
	const uint64_t BlockGroup = 0x20;
	const uint64_t SimpleBlock = 0x23;
	const uint64_t Block = 0x21;
	const uint64_t Timecode = 0x67;
	const uint64_t DefaultDuration = 0x3E383;
	const uint64_t TrackTimecodeScale = 0x3314F;
	const uint64_t BlockDuration = 0x1B;
} // id

} // matryona
