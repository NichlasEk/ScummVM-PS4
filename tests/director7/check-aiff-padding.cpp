#define FORBIDDEN_SYMBOL_ALLOW_ALL
#include "director/ps4-jonsson-aiff.h"
#include "common/memstream.h"
#include "common/substream.h"
#include "audio/audiostream.h"
#include "audio/decoders/raw.h"
#include "audio/decoders/aiff.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <cstring>

int main(int argc, char **argv) {
	assert(argc == 3);
	FILE *file = fopen(argv[2], "rb");
	assert(file);
	fseek(file, 0, SEEK_END);
	std::vector<byte> data(ftell(file));
	rewind(file);
	assert(fread(data.data(), 1, data.size(), file) == data.size());
	fclose(file);
	Common::MemoryReadStream input(data.data(), data.size());
	if (!strcmp(argv[1], "baseline")) {
		delete Audio::makeAIFFStream(new Common::MemoryReadStream(data.data(), data.size()), DisposeAfterUse::YES);
		return 0;
	}
	uint32 offset = 0, size = 0;
	input.seek(data.size() >= 7 ? 7 : 0);
	const int64 originalPos = input.pos();
	const bool matched = Director::findJonssonAIFFPadding(input, offset, size);
	assert(input.pos() == originalPos);
	assert(matched == !strcmp(argv[1], "padded"));
	if (!matched) {
		if (!strcmp(argv[1], "normal")) {
			input.seek(0);
			Audio::RewindableAudioStream *audio = Audio::makeAIFFStream(new Common::MemoryReadStream(data.data(), data.size()), DisposeAfterUse::YES);
			assert(audio);
			delete audio;
		}
		return 0;
	}
	Audio::SeekableAudioStream *audio = Audio::makeRawStream(
		new Common::SeekableSubReadStream(&input, offset, (uint64)offset + size),
		22050, Audio::FLAG_16BITS, DisposeAfterUse::YES);
	assert(audio && audio->getRate() == 22050 && !audio->isStereo());
	int16 samples[1024];
	uint32 position = 0;
	int count;
	while ((count = audio->readBuffer(samples, 1024)) > 0) {
		for (int i = 0; i < count; ++i, position += 2) {
			assert(position < size);
			assert(samples[i] == (int16)READ_BE_UINT16(data.data() + offset + position));
		}
	}
	assert(position == size && audio->endOfData());
	assert(audio->rewind());
	assert(audio->readBuffer(samples, 1) == 1);
	assert(samples[0] == (int16)READ_BE_UINT16(data.data() + offset));
	delete audio;
}
