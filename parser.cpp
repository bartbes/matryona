#include <cstring>

#include "parser.h"

using std::size_t;
using std::uint64_t;
using std::uint8_t;
using std::int8_t;

static const uint64_t Codec_Audio_Vorbis = 0x534942524f565f41;
static const uint64_t Codec_Video_Vp8 = 0x3850565f56;
static const uint64_t Codec_Video_Theora = 0x41524f4548545f56;

namespace matryona
{

const char *streamTypeName(StreamType type)
{
	switch(type)
	{
	case VIDEO_VP8:
		return "VP8";
	case VIDEO_THEORA:
		return "Theora";
	case AUDIO_VORBIS:
		return "Vorbis";
	default:
		return "Unknown";
	}
}

Parser::Parser(IO *input)
	: input(input)
{
	readHeader();
}

Parser::~Parser()
{
}

size_t Parser::getNumStreams() const
{
	return streams.size();
}

const StreamInfo &Parser::getStreamInfo(size_t stream) const
{
	return streams[stream];
}

static EBMLElement findElement(IO *io, uint64_t id)
{
	for (EBMLElementIterator it(io); it != EBMLElementIterator::end; ++it)
		if (it->id == id)
			return *it;
	throw InvalidFileFormatError("Missing required element");
}

static StreamType typeFromId(uint64_t id)
{
	switch(id)
	{
	case Codec_Video_Theora:
		return VIDEO_THEORA;
	case Codec_Video_Vp8:
		return VIDEO_VP8;
	case Codec_Audio_Vorbis:
		return AUDIO_VORBIS;
	default:
		return UNKNOWN;
	}
}

struct Parser::StreamState
{
	StreamState();
	~StreamState();
	StreamState(const StreamState &other) = delete;
	StreamState(StreamState &&other);

	// Make sure the buffer is at least 'size' long
	void grow(uint64_t size);

	// Our position in the file
	EBMLElementIterator clusterIt;
	EBMLElementIterator blockIt;
	bool firstCluster;

	// Our grow-only per-stream buffer
	uint8_t *buffer;
	uint64_t bufferSize;

	// Track info
	float timecodeScale;

	// Cluster info
	uint64_t clusterTimecode;

	// Block info, saved because of lacing
	uint64_t timecode;
	uint64_t duration;

	// Our position in the block, for lacing
	EBMLElement block;
	size_t blockSize;
	ssize_t subpacketPos;
	ssize_t subpackets;

	enum {
		LACING_NONE,
		LACING_EBML,
		LACING_XIPH,
		LACING_FIXED,
	} lacing;
};

Parser::StreamState::StreamState()
	: clusterIt(EBMLElementIterator::end)
	, blockIt(EBMLElementIterator::end)
	, firstCluster(true)
	, buffer(nullptr)
	, bufferSize(0)
	, timecodeScale(1)
	, clusterTimecode(0)
	, timecode(0)
	, duration(0)
	, blockSize(0)
	, subpacketPos(1)
	, subpackets(1)
	, lacing(LACING_NONE)
{
}

Parser::StreamState::StreamState(StreamState &&other)
	: clusterIt(other.clusterIt)
	, blockIt(other.blockIt)
	, firstCluster(other.firstCluster)
	, buffer(other.buffer)
	, bufferSize(other.bufferSize)
	, timecodeScale(other.timecodeScale)
	, clusterTimecode(other.clusterTimecode)
	, timecode(other.timecode)
	, duration(other.duration)
	, blockSize(other.blockSize)
	, subpacketPos(other.subpacketPos)
	, subpackets(other.subpackets)
	, lacing(other.lacing)
{
	other.buffer = nullptr;
	other.bufferSize = 0;
}

Parser::StreamState::~StreamState()
{
	delete[] buffer;
}

void Parser::StreamState::grow(uint64_t size)
{
	if (bufferSize < size)
		delete[] buffer;
	buffer = new uint8_t[bufferSize = size];
}

void Parser::readHeader()
{
	EBMLElement header = findElement(input, id::EBML);

	// Check EBML version
	{
		EBMLElement readVersion = findElement(&header.io, id::EBMLReadVersion);
		uint64_t version = readUint(readVersion.size, &readVersion.io);
		if (version > 1)
			throw InvalidFileFormatError("Invalid EBML version");
	}

	// Check if DocType is "matroska" or "webm"
	{
		EBMLElement docType = findElement(&header.io, id::DocType);
		if (docType.size > 16)
			throw InvalidFileFormatError("Format not recognized");
		char buffer[16];
		docType.io.read(buffer, docType.size);
		if (std::strncmp(buffer, "matroska", docType.size) != 0 &&
				std::strncmp(buffer, "webm", docType.size) != 0)
			throw InvalidFileFormatError("Format not recognized");
	}

	// Find our various tracks
	segment = findElement(input, id::Segment);
	EBMLElement tracks = findElement(&segment.io, id::Tracks);

	for (EBMLElementIterator it(&tracks.io); it != EBMLElementIterator::end; ++it)
	{
		if (it->id != id::TrackEntry)
			continue;

		EBMLElement codecId = findElement(&it->io, id::CodecID);
		EBMLElement trackUid = findElement(&it->io, id::TrackUID);
		EBMLElement trackNumber = findElement(&it->io, id::TrackNumber);

		StreamInfo info;
		info.type = typeFromId(readUint(codecId.size, &codecId.io));
		info.id = readUint(trackUid.size, &trackUid.io);
		info.trackNumber = readUint(trackNumber.size, &trackNumber.io);
		info.defaultDuration = 0;
		info.isDefault = true;
		info.isEnabled = true; 

		StreamState state;
		state.clusterIt = EBMLElementIterator(&segment.io);

		// Handle optional data
		for (EBMLElementIterator j(&it->io); j != EBMLElementIterator::end; ++j)
		{
			if (j->id == id::FlagDefault)
				info.isDefault = readUint(j->size, &j->io) == 1;
			if (j->id == id::FlagEnabled)
				info.isEnabled = readUint(j->size, &j->io) == 1;
			if (j->id == id::DefaultDuration)
				info.defaultDuration = readUint(j->size, &j->io);
			if (j->id == id::TrackTimecodeScale)
				state.timecodeScale = readFloat(j->size, &j->io);
			if (j->id == id::CodecPrivate && info.type == VIDEO_THEORA)
			{
				// Theora uses Xiph style lacing in the CodecPrivate field
				// We can reuse the existing lacing code by pretending this is a block
				// See readData and readBlock for more info on lacing
				state.block = *j;
				uint8_t frameCount;
				if (state.block.io.read(reinterpret_cast<char*>(&frameCount), 1) != 1)
					throw IOError();

				state.subpacketPos = 0;
				state.subpackets = frameCount+1;

				state.blockSize = state.block.io.getLength() - state.block.io.tell();
				state.grow(state.blockSize);
				if (state.block.io.read(reinterpret_cast<char*>(state.buffer), state.blockSize) != state.blockSize)
					throw IOError();

				state.lacing = StreamState::LACING_XIPH;
			}
		}

		streams.push_back(info);
		states.push_back(std::move(state));
	}
}

bool Parser::readData(uint64_t stream, uint8_t *&data, uint64_t &size, uint64_t &timecode, uint64_t &duration)
{
	StreamState &state = states[stream];

	// In case we have CodecPrivate data (that we know how to use), the state is
	// set up to match that data.
	// Otherwise, subpacketPos = subpacks = 1, and we start by reading a block.
	if (state.subpacketPos >= state.subpackets)
		if (!readBlock(stream))
			return false;

	// We know the timecode and duration, which is per-block
	timecode = state.timecode;
	duration = state.duration;

	switch(state.lacing)
	{
	case StreamState::LACING_NONE:
		// No lacing means 1 block is 1 "datum", usually one frame
		data = state.buffer;
		size = state.blockSize;
		break;
	case StreamState::LACING_XIPH:
	{
		// Walk the size data until we got to our subpacket's size
		// Keep track of the offset so far, that marks the start of the subpacket
		uint64_t offset = 0;
		uint8_t *readPtr = state.buffer;
		for (ssize_t seen = 0; seen < state.subpacketPos && readPtr < state.buffer + state.bufferSize; ++readPtr)
		{
			uint8_t sz = *readPtr;
			offset += sz;
			if (sz < 255)
				++seen;
		}

		// If we hit the end, abort
		if (readPtr >= state.buffer + state.bufferSize)
			throw IOError();

		// If this is the last subpacket, the size is the remainder,
		// otherwise, read one more xiph-style length for the size, then
		// finish to advance readPtr
		if (state.subpacketPos + 1 != state.subpackets)
		{
			size = 0;
			for (; readPtr < state.buffer + state.bufferSize; ++readPtr)
			{
				uint8_t sz = *readPtr;
				size += sz;
				if (sz < 255)
					break;
			}
			++readPtr;
			for (ssize_t seen = state.subpacketPos + 1; seen < state.subpackets - 1 && readPtr < state.buffer + state.bufferSize; ++readPtr)
				if (*readPtr < 255)
					++seen;
		}
		else
			size = state.bufferSize - (readPtr - state.buffer) - offset;

		// Again, abort if we hit the end
		if (readPtr >= state.buffer + state.bufferSize)
			throw IOError();

		data = readPtr + offset;

		// One last sanity check
		if (data + size > state.buffer + state.bufferSize)
			throw IOError();

		break;
	}
	case StreamState::LACING_EBML:
		throw InvalidFileFormatError("File uses EBML lacing, which is not yet implemented");
	case StreamState::LACING_FIXED:
		// Fixed size lacing is also simple, all blocks are constant size
		size = state.blockSize/state.subpackets;
		data = state.buffer + state.subpacketPos * size;
		break;
	}

	++state.subpacketPos;
	return true;
}

bool Parser::readBlock(uint64_t stream)
{
	StreamInfo &info = streams[stream];
	StreamState &state = states[stream];
	uint64_t trackNumber;
	state.duration = info.defaultDuration;

	do
	{
		// Advance to the next block, either a BlockGroup or a SimpleBlock
		++state.blockIt;
		state.blockIt.until(id::BlockGroup, id::SimpleBlock);

		// If there is no such block in this Cluster, go to the next cluster
		while (state.blockIt == EBMLElementIterator::end)
		{
			// In case we haven't read this Cluster yet, do that first
			if (!state.firstCluster)
				++state.clusterIt;
			state.firstCluster = false;
			state.clusterIt.until(id::Cluster);

			// If there are no more Clusters, we're done, no more blocks.
			if (state.clusterIt == EBMLElementIterator::end)
				return false;

			// Initialise our blockIterator, in this new Cluster
			state.blockIt = EBMLElementIterator(&state.clusterIt->io).until(id::BlockGroup, id::SimpleBlock);

			// Find the optional cluster TimeCode
			state.clusterTimecode = 0;
			for (EBMLElementIterator it(&state.clusterIt->io); it != EBMLElementIterator::end; ++it)
				if (it->id == id::Timecode)
				{
					state.clusterTimecode = readUint(it->size, &it->io);
					break;
				}
		}

		// We have a new block, is it a SimpleBlock or a BlockGroup?
		state.block = *state.blockIt;
		if (state.block.id == id::BlockGroup)
		{
			// If it's a BlockGroup we get its duration, then navigate to its Block
			EBMLElement blockDuration = findElement(&state.block.io, id::BlockDuration);
			state.duration = readUint(blockDuration.size, &blockDuration.io);
			state.block = findElement(&state.blockIt->io, id::Block);
		}

		// Read the track number of this block, if it doesn't match, try again
		trackNumber = readVint(&state.block.io);
	} while (trackNumber != info.trackNumber);

	// We now have a Block! Read the time offset
	int16_t timeOffset;
	if (state.block.io.read(reinterpret_cast<char*>(&timeOffset), 2) != 2)
		throw IOError();
	// FIXME: Do this only if little endian
	timeOffset = swapEndianness(timeOffset);

	// TODO: Figure out what to do with timecodeScale.
	state.timecode = state.clusterTimecode + timeOffset;

	// Read the flags
	uint8_t flags;
	if (state.block.io.read(reinterpret_cast<char*>(&flags), 1) != 1)
		throw IOError();

	// Lacing, woo
	switch ((flags & 0x06) >> 1)
	{
	case 0b00:
		state.lacing = StreamState::LACING_NONE;
		break;
	case 0b01:
		state.lacing = StreamState::LACING_XIPH;
		break;
	case 0b10:
		state.lacing = StreamState::LACING_FIXED;
		break;
	case 0b11:
		throw InvalidFileFormatError("File uses EBML lacing, which is not yet implemented");
	default:
		throw InvalidFileFormatError("Invalid lacing specified");
	}

	// If there is any kind of lacing, we first get a frameCount (-1)
	if (state.lacing != StreamState::LACING_NONE)
	{
		uint8_t frameCount;
		if (state.block.io.read(reinterpret_cast<char*>(&frameCount), 1) != 1)
			throw IOError();
		state.subpacketPos = 0;
		state.subpackets = frameCount+1;
	}

	// Now the remainder is the actual data, possibly with laced subpacket sizes
	state.blockSize = state.block.io.getLength() - state.block.io.tell();
	state.grow(state.blockSize);
	if (state.block.io.read(reinterpret_cast<char*>(state.buffer), state.blockSize) != state.blockSize)
		throw IOError();

	return true;
}

} // matryona
