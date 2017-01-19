#include <cstdio>
#include <stdexcept>

#include "matryona.h"

using namespace std;
using namespace matryona;

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Usage: %s <filename>\n", argv[0]);
		return 0;
	}

	CIO io(argv[1]);
	Parser p(&io);

	printf("%zd streams\n", p.getNumStreams());
	uint8_t *buffer;
	uint64_t size;
	for (size_t i = 0; i < p.getNumStreams(); i++)
	{
		auto &info = p.getStreamInfo(i);
		printf("Stream %zd:\n", i);
		printf("\tID: %lx\n", info.id);
		printf("\tCodec: %s\n", streamTypeName(info.type));
		printf("\tEnabled: %s\n", info.isEnabled ? "true" : "false");
		printf("\tDefault: %s\n", info.isDefault ? "true" : "false");
		while (p.readData(i, buffer, size))
			printf("\tGot data of size: %ld\n", size);
	}

	return 0;
}
