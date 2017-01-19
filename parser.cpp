#include <cstring>

#include "parser.h"

using std::size_t;
using std::uint64_t;

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

	EBMLElementIterator clusterIt;
	EBMLElementIterator blockIt;
	bool firstCluster;
	uint64_t clusterTimecode;
	uint8_t *buffer;
	uint64_t bufferSize;
};

Parser::StreamState::StreamState()
	: clusterIt(EBMLElementIterator::end)
	, blockIt(EBMLElementIterator::end)
	, firstCluster(true)
	, buffer(nullptr)
	, bufferSize(0)
{
}

Parser::StreamState::~StreamState()
{
	delete buffer;
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
		info.isDefault = true;
		info.isEnabled = true; 

		// Handle optional flags
		for (EBMLElementIterator j(&it->io); j != EBMLElementIterator::end; ++j)
		{
			if (j->id == id::FlagDefault)
				info.isDefault = readUint(j->size, &j->io) == 1;
			if (j->id == id::FlagEnabled)
				info.isEnabled = readUint(j->size, &j->io) == 1;
		}

		streams.push_back(info);
		StreamState state;
		state.clusterIt = EBMLElementIterator(&segment.io);
		states.push_back(state);
	}
}

bool Parser::readData(uint64_t stream, uint8_t *&data, uint64_t &size)
{
	StreamInfo &info = streams[stream];
	StreamState &state = states[stream];
	EBMLElement block;
	uint64_t trackNumber;

	do
	{
		++state.blockIt;
		state.blockIt.until(id::BlockGroup, id::SimpleBlock);

		while (state.blockIt == EBMLElementIterator::end)
		{
			if (!state.firstCluster)
				++state.clusterIt;
			state.firstCluster = false;
			state.clusterIt.until(id::Cluster);

			if (state.clusterIt == EBMLElementIterator::end)
				return false;

			state.blockIt = EBMLElementIterator(&state.clusterIt->io).until(id::BlockGroup, id::SimpleBlock);

			state.clusterTimecode = 0;
			for (EBMLElementIterator it(&state.clusterIt->io); it != EBMLElementIterator::end; ++it)
				if (it->id == id::Timecode)
				{
					state.clusterTimecode = readUint(it->size, &it->io);
					break;
				}
		}

		block = *state.blockIt;
		if (block.id == id::BlockGroup)
			block = findElement(&state.blockIt->io, id::Block);
		trackNumber = readVint(&block.io);
	} while (trackNumber != info.trackNumber);

	int16_t timeOffset;
	if (block.io.read(reinterpret_cast<char*>(&timeOffset), 2) != 2)
		throw IOError();
	// FIXME: Do this only if little endian
	timeOffset = swapEndianness(timeOffset);

	uint8_t flags;
	if (block.io.read(reinterpret_cast<char*>(&flags), 1) != 1)
		throw IOError();

	// TODO: lacing support
	if (flags & 0x06)
		throw InvalidFileFormatError("File uses lacing, which is not yet implemented");

	size = block.io.getLength() - block.io.tell();
	if (state.bufferSize < size)
	{
		delete state.buffer;
		state.buffer = new uint8_t[state.bufferSize = size];
	}

	if (block.io.read(reinterpret_cast<char*>(state.buffer), size) != size)
		throw IOError();

	data = state.buffer;
	return true;
}

} // matryona
