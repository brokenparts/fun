#include "common_core.hh"

#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#define Msg(...) std::printf(__VA_ARGS__); std::printf("\n");

// ===

struct MTPoint {
	float x;
	float y;
};

struct MTContactEvent {
	int 	e_frame;
	double	e_ts;
	int	e_id;
	int	e_state;
	int	e_unk1;
	int	e_unk2;
	MTPoint pos;
	MTPoint vel;
};

using MTDeviceRef = void*;
using MTContactCallbackFunction = int(*)(int, MTContactEvent*, int, double, int);

extern "C" {
	MTDeviceRef 	MTDeviceCreateDefault();
	void 		MTRegisterContactFrameCallback(MTDeviceRef, MTContactCallbackFunction);
	void 		MTDeviceStart(MTDeviceRef, int);
}

// ===


f32 area_l = 0.1f;
f32 area_r = 1.0f - area_l;
f32 area_t = 0.1f;
f32 area_b = 1.0f - area_t;

MTPoint CalcRemap(MTPoint p) {
	return {
		.x = RemapClamp(p.x, area_l, area_r, 0.0f, 1.0f),
		.y = 1.0f - RemapClamp(p.y, area_t, area_b, 0.0f, 1.0f),
	};
}

static int MyCallback(int, MTContactEvent* e, int, double, int) {
	Msg("Callback { .pos = <%.2f, %.2f>", e->pos.x, e->pos.y);
	NSRect frame = [[NSScreen mainScreen] frame];
	MTPoint p = CalcRemap(e->pos);
	if (!isnan(p.x) && !isnan(p.y)) {
		CGWarpMouseCursorPosition(CGPointMake(p.x * (float)frame.size.width, p.y * (float)frame.size.height));
	}
	return 0;
}

int main(int argc, const char** argv) {
	MTDeviceRef dev = MTDeviceCreateDefault();
	MTRegisterContactFrameCallback(dev, MyCallback);
	MTDeviceStart(dev, 0);

	while(1);
}
