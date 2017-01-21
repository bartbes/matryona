#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "io.h"
#include "ebml.h"

namespace matryona
{

enum StreamType
{
	VIDEO_VP8,
	VIDEO_THEORA,
	AUDIO_VORBIS,
	UNKNOWN,
};

const char *streamTypeName(StreamType type);

struct StreamInfo
{
	StreamType type;
	std::uint64_t id;
	std::uint64_t trackNumber;
	std::uint64_t defaultDuration;
	bool isEnabled;
	bool isDefault;
};

class Parser
{
public:
	Parser(IO *input);
	~Parser();

	std::size_t getNumStreams() const;
	const StreamInfo &getStreamInfo(std::size_t stream) const;

	bool readData(std::uint64_t stream, std::uint8_t *&data, std::uint64_t &size, std::uint64_t &timecode, std::uint64_t &duration);

private:
	struct StreamState;

	IO *input;
	std::vector<StreamInfo> streams;
	std::vector<StreamState> states;
	EBMLElement segment;

	void readHeader();
	bool readBlock(std::uint64_t stream);
};

} // matryona
