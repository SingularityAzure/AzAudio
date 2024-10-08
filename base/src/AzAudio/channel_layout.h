/*
	File: channel_layout.h
	Author: Philip Haynes
	Describing channel positions for buffers
*/

#ifndef AZAUDIO_CHANNEL_LAYOUT_H
#define AZAUDIO_CHANNEL_LAYOUT_H

#include <stdint.h>
#include "header_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO: The most extreme setups can have 20 channels, but there may be no way to actually use that many channels effectively without a way to distinguish them (and the below are all you can get from the MMDevice API on Windows afaik). Should there be more information available, this part of the API will grow.
// TODO: Find a way to get more precise speaker placement information, if that's even possible.
/* These roughly correspond to the following physical positions.
Floor:
    6 2 7
  0       1
 9    H    10
  4   8   5

Ceiling:
  12 13 14
     H
  15 16 17
*/
enum azaPosition {
	AZA_POS_LEFT_FRONT         = 0,
	AZA_POS_RIGHT_FRONT        = 1,
	AZA_POS_CENTER_FRONT       = 2,
	AZA_POS_SUBWOOFER          = 3,
	AZA_POS_LEFT_BACK          = 4,
	AZA_POS_RIGHT_BACK         = 5,
	AZA_POS_LEFT_CENTER_FRONT  = 6,
	AZA_POS_RIGHT_CENTER_FRONT = 7,
	AZA_POS_CENTER_BACK        = 8,
	AZA_POS_LEFT_SIDE          = 9,
	AZA_POS_RIGHT_SIDE         =10,
	AZA_POS_CENTER_TOP         =11,
	AZA_POS_LEFT_FRONT_TOP     =12,
	AZA_POS_CENTER_FRONT_TOP   =13,
	AZA_POS_RIGHT_FRONT_TOP    =14,
	AZA_POS_LEFT_BACK_TOP      =15,
	AZA_POS_CENTER_BACK_TOP    =16,
	AZA_POS_RIGHT_BACK_TOP     =17,
};
// NOTE: This is more than we should ever see in reality, and definitely more than can be uniquely represented by the above positions. We're reserving more for later.
#define AZA_MAX_CHANNEL_POSITIONS 22
#define AZA_POS_ENUM_COUNT (AZA_POS_RIGHT_BACK_TOP+1)

enum azaFormFactor {
	AZA_FORM_FACTOR_SPEAKERS=0,
	AZA_FORM_FACTOR_HEADPHONES,
};

typedef struct azaChannelLayout {
	uint8_t count;
	uint8_t formFactor;
	uint8_t positions[AZA_MAX_CHANNEL_POSITIONS];
} azaChannelLayout;

static inline azaChannelLayout azaChannelLayoutOneChannel(azaChannelLayout src, uint8_t channel) {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 1,
		/* .formFactor = */ src.formFactor,
		/* .positions  = */ {src.positions[channel]},
	};
}

// Some standard layouts, for your convenience

static inline azaChannelLayout azaChannelLayoutMono() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 1,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ {AZA_POS_CENTER_FRONT},
	};
}

static inline azaChannelLayout azaChannelLayoutStereo() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 2,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT },
	};
}

static inline azaChannelLayout azaChannelLayoutHeadphones() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 2,
		/* .formFactor = */ AZA_FORM_FACTOR_HEADPHONES,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT },
	};
}

static inline azaChannelLayout azaChannelLayout_2_1() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 3,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT, AZA_POS_SUBWOOFER },
	};
}

static inline azaChannelLayout azaChannelLayout_3_0() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 3,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT, AZA_POS_CENTER_FRONT },
	};
}

static inline azaChannelLayout azaChannelLayout_3_1() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 4,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT, AZA_POS_CENTER_FRONT, AZA_POS_SUBWOOFER },
	};
}

static inline azaChannelLayout azaChannelLayout_4_0() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 4,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT, AZA_POS_LEFT_BACK, AZA_POS_RIGHT_BACK },
	};
}

static inline azaChannelLayout azaChannelLayout_4_1() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 5,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT, AZA_POS_SUBWOOFER, AZA_POS_LEFT_BACK, AZA_POS_RIGHT_BACK },
	};
}

static inline azaChannelLayout azaChannelLayout_5_0() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 5,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT, AZA_POS_CENTER_FRONT, AZA_POS_LEFT_BACK, AZA_POS_RIGHT_BACK },
	};
}

static inline azaChannelLayout azaChannelLayout_5_1() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 6,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT, AZA_POS_CENTER_FRONT, AZA_POS_SUBWOOFER, AZA_POS_LEFT_BACK, AZA_POS_RIGHT_BACK },
	};
}

static inline azaChannelLayout azaChannelLayout_7_0() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 7,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT, AZA_POS_CENTER_FRONT, AZA_POS_LEFT_BACK, AZA_POS_RIGHT_BACK, AZA_POS_LEFT_SIDE, AZA_POS_RIGHT_SIDE },
	};
}

static inline azaChannelLayout azaChannelLayout_7_1() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 8,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT, AZA_POS_CENTER_FRONT, AZA_POS_SUBWOOFER, AZA_POS_LEFT_BACK, AZA_POS_RIGHT_BACK, AZA_POS_LEFT_SIDE, AZA_POS_RIGHT_SIDE },
	};
}

static inline azaChannelLayout azaChannelLayout_9_0() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 9,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT, AZA_POS_CENTER_FRONT, AZA_POS_LEFT_BACK, AZA_POS_RIGHT_BACK, AZA_POS_LEFT_CENTER_FRONT, AZA_POS_RIGHT_CENTER_FRONT, AZA_POS_LEFT_SIDE, AZA_POS_RIGHT_SIDE },
	};
}

static inline azaChannelLayout azaChannelLayout_9_1() {
	return AZA_CLITERAL(azaChannelLayout) {
		/* .count      = */ 10,
		/* .formFactor = */ AZA_FORM_FACTOR_SPEAKERS,
		/* .positions  = */ { AZA_POS_LEFT_FRONT, AZA_POS_RIGHT_FRONT, AZA_POS_CENTER_FRONT, AZA_POS_SUBWOOFER, AZA_POS_LEFT_BACK, AZA_POS_RIGHT_BACK, AZA_POS_LEFT_CENTER_FRONT, AZA_POS_RIGHT_CENTER_FRONT, AZA_POS_LEFT_SIDE, AZA_POS_RIGHT_SIDE },
	};
}

// Make a reasonable guess about the layout for 1 to 10 channels. For more advanced layouts, such as with aerial speakers, you'll have to specify them manually, since there's no way to guess the setup in a meaningful way for aerials.
// To use the device layout on a Stream, just leave channels zeroed out.
static inline azaChannelLayout azaChannelLayoutStandardFromCount(uint8_t count) {
	switch (count) {
		case  1: return azaChannelLayoutMono();
		case  2: return azaChannelLayoutStereo();
		case  3: return azaChannelLayout_2_1();
		case  4: return azaChannelLayout_4_0();
		case  5: return azaChannelLayout_5_0();
		case  6: return azaChannelLayout_5_1();
		case  7: return azaChannelLayout_7_0();
		case  8: return azaChannelLayout_7_1();
		case  9: return azaChannelLayout_9_0();
		case 10: return azaChannelLayout_9_1();
		default: return AZA_CLITERAL(azaChannelLayout) { 0 };
	}
}

#ifdef __cplusplus
}
#endif

#endif // AZAUDIO_CHANNEL_LAYOUT_H