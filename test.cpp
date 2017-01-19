#include <cstdio>
#include <stdexcept>

#include <matryona/matryona.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

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
	ssize_t target = -1;
	for (size_t i = 0; i < p.getNumStreams(); i++)
	{
		auto &info = p.getStreamInfo(i);
		printf("Stream %zd:\n", i);
		printf("\tID: %lx\n", info.id);
		printf("\tCodec: %s\n", streamTypeName(info.type));
		printf("\tEnabled: %s\n", info.isEnabled ? "true" : "false");
		printf("\tDefault: %s\n", info.isDefault ? "true" : "false");
		if (info.type == VIDEO_VP8)
			target = i;
	}

	if (target == -1)
		return 0;

	vpx_codec_ctx context;
	vpx_codec_dec_init(&context, vpx_codec_vp8_dx(), nullptr, 0);

	uint8_t *buffer;
	uint64_t size;
	p.readData(target, buffer, size);
	vpx_codec_decode(&context, buffer, size, nullptr, 0);

	vpx_codec_iter_t iter = nullptr;
	vpx_image_t *currentFrame = vpx_codec_get_frame(&context, &iter);
	if (!currentFrame)
		return 0;

	printf("Got a frame!\n");
	printf("%dx%d\n", currentFrame->d_w, currentFrame->d_h);

	for (int y = 0; y < currentFrame->d_h; ++y)
		fwrite(currentFrame->planes[0] + currentFrame->stride[0]*y, currentFrame->d_w, 1, stderr);
	for (int y = 0; y < currentFrame->d_h/2; ++y)
		fwrite(currentFrame->planes[1] + currentFrame->stride[1]*y, currentFrame->d_w/2, 1, stderr);
	for (int y = 0; y < currentFrame->d_h/2; ++y)
		fwrite(currentFrame->planes[2] + currentFrame->stride[2]*y, currentFrame->d_w/2, 1, stderr);

	vpx_codec_destroy(&context);

	return 0;
}
