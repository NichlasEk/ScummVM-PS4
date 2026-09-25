#include <stdio.h>
#include <string.h>

#ifndef USE_OPENGNM
#include <orbis/GnmDriver.h>
#endif

#include <gnm/drawcommandbuffer.h>
#include <gnm/gpuaddr/gpuaddr.h>
#include <gnm/platform.h>

#include "u/utility.h"

#include "displayctx.h"
#include "misc.h"

static FILE *probe_log;
#define printf(...) fprintf(probe_log, __VA_ARGS__)
#define puts(s) fprintf(probe_log, "%s\n", (s))

int main(void) {
	probe_log = fopen("/data/scummvm-opengnm-uniforms.log", "w");
	if (!probe_log) return 2;
	setvbuf(probe_log, NULL, _IONBF, 0);
	puts("OpenGNM uniforms: native GNM + open-source shader compiler; no Piglet");
	MemoryAllocator garlicmem = memalloc_init(
		64 * 1024 * 1024,  // 64mB
		ORBIS_KERNEL_PROT_CPU_READ | ORBIS_KERNEL_PROT_CPU_RW |
			ORBIS_KERNEL_PROT_GPU_READ | ORBIS_KERNEL_PROT_GPU_WRITE,
		ORBIS_KERNEL_WC_GARLIC
	);

	// init video
	DisplayContext display = {0};
	if (!displayctx_init(&display)) {
		fatal("Failed to init video");
	}

	printf("Display resolution: %ux%u\n", display.screenw, display.screenh);

	// create color render target
	// only one is needed since this program flips a frame only once
	GnmRenderTarget fbtarget = {0};
	if (!initcolortarget(
			&fbtarget, &garlicmem, display.screenw, display.screenh,
			GNM_FMT_R8G8B8A8_SRGB, gnmGpuMode()
		)) {
		fatal("Failed to init color render target");
	}

	if (!displayctx_setrts(&display, &fbtarget, 1)) {
		fatal("Failed to init video");
	}

	// create command buffer
	const size_t cmdmemsize = GNM_INDIRECT_BUFFER_MAX_BYTESIZE;
	void* cmdmem =
		memalloc_alloc(&garlicmem, cmdmemsize, GNM_ALIGNMENT_BUFFER_BYTES);
	if (!cmdmem) {
		fatal("Failed to allocate cmdbuffer");
	}

	GnmCommandBuffer cmdbuf = gnmCmdInit(cmdmem, cmdmemsize, NULL, NULL);
	gnmDrawCmdInitDefaultHardwareState(&cmdbuf);
	gnmDrawCmdSetNumInstances(&cmdbuf, 1);
	// Screen, window, generic and viewport scissors intersect. Upstream only
	// sets the screen scissor, leaving a zero-sized clip rectangle elsewhere.
	const uint32_t scissor_regs[] = {0x28204, 0x28240, 0x28250};
	for (unsigned i = 0; i < 3; ++i) {
		*cmdbuf.cmdptr++ = 0xc0026900; // SET_CONTEXT_REG, three payload words
		*cmdbuf.cmdptr++ = (scissor_regs[i] - 0x28000) / 4;
		*cmdbuf.cmdptr++ = 0x80000000; // top-left zero, no window offset
		*cmdbuf.cmdptr++ = (display.screenh << 16) | display.screenw;
	}

	// Diagnostic CPU prefill: establishes presentation independently of drawing.
	uint64_t rtbytes = 0; uint32_t rtalign = 0;
	gnmRtCalcByteSize(&rtbytes, &rtalign, &fbtarget);
	memset(gnmRtGetBaseAddr(&fbtarget), 0x7f, rtbytes);
	const GnmDepthStencilControl no_depth = {0};
	gnmDrawCmdSetDepthStencilControl(&cmdbuf, &no_depth);

	// wait for clears to write
	gnmDrawCmdWaitGraphicsWrite(
		&cmdbuf, GNM_ACQUIRE_TARGET_CB0 | GNM_ACQUIRE_TARGET_DB
	);

	const GnmPrimitiveSetup primsetup = {
		.cullmode = GNM_CULL_NONE,
		.frontface = GNM_FACE_CCW,
		.frontmode = GNM_FILL_SOLID,
		.backmode = GNM_FILL_SOLID,
		.frontoffsetmode = false,
		.backoffsetmode = false,
		.vertexwindowoffsetenable = false,
		.provokemode = GNM_PROVOKINGVTX_FIRST,
		.perspectivecorrectiondisable = false,
	};
	gnmDrawCmdSetPrimitiveSetup(&cmdbuf, &primsetup);

	gnmDrawCmdSetRenderTarget(&cmdbuf, 0, &fbtarget);
	gnmDrawCmdSetRenderTargetMask(&cmdbuf, 0xf);

	// setup viewport
	setupviewport(&cmdbuf, 0, 0, display.screenw, display.screenh, 0.5f, 0.5f);

	GnmVsShader* vshader = NULL;
	GnmPsShader* pshader = NULL;
	if (!loadvshader(&vshader, &garlicmem, "/app0/assets/misc/tri.vert.sb")) {
		fatal("Failed to load vertex shader");
	}
	if (!loadpshader(&pshader, &garlicmem, "/app0/assets/misc/tri.frag.sb")) {
		fatal("Failed to load fragment shader");
	}

	gnmDrawCmdSetVsShader(&cmdbuf, &vshader->registers, 0);
	gnmDrawCmdSetPsShader(&cmdbuf, &pshader->registers);

	gnmDrawCmdSetPsInputUsage(
		&cmdbuf, gnmVsShaderExportSemanticTable(vshader),
		vshader->numexportsemantics, gnmPsShaderInputSemanticTable(pshader),
		pshader->numinputsemantics
	);

	// Two buffer descriptors exercise binding selection and member offsets.
	float *parameters = memalloc_alloc(&garlicmem, 12 * sizeof(float), 16);
	if (!parameters) { puts("FAIL allocating uniforms"); return 2; }
	const float values[12] = {9, 9, 9, 9, .5f, .5f, .5f, 1, 0, .1f, .25f, 0};
	memcpy(parameters, values, sizeof(values));
	GnmBuffer *descriptors = memalloc_alloc(&garlicmem, 2 * sizeof(GnmBuffer), 16);
	if (!descriptors) { puts("FAIL allocating descriptor table"); return 2; }
	descriptors[0] = gnmCreateConstBuffer(parameters, 8 * sizeof(float));
	descriptors[1] = gnmCreateConstBuffer(parameters + 8, 4 * sizeof(float));
	printf("UNIFORMS table=%p data=%p (full 64-bit pointers)\n", descriptors, parameters);
	const GnmInputUsageSlot *slots = gnmPsShaderInputUsageSlotTable(pshader);
	unsigned descriptor_register = 16;
	for (unsigned i = 0; i < pshader->common.numinputusageslots; ++i)
		if (slots[i].usagetype == GNM_SHINPUTUSAGE_PTR_INDIRECTRESOURCETABLE)
			descriptor_register = slots[i].startregister;
	if (descriptor_register > 14) { puts("FAIL missing descriptor pointer metadata"); return 2; }
	printf("UNIFORMS user register=%u\n", descriptor_register);
	gnmDrawCmdSetPointerUserData(&cmdbuf, GNM_STAGE_PS, descriptor_register, descriptors);

	// draw triangle
	gnmDrawCmdSetPrimitiveType(&cmdbuf, GNM_PT_TRILIST);
	gnmDrawCmdDrawIndexAuto(&cmdbuf, 3);

	// setup sync point
	uint64_t* label =
		memalloc_alloc(&garlicmem, sizeof(uint64_t), sizeof(uint64_t));
	if (!label) {
		fatal("Failed to allocate sync label");
	}

	gnmDrawCmdEventWriteEop(
		&cmdbuf, GNM_CACHE_FLUSH_TS, (uint64_t)label, GNM_DATA_SEL_SEND_DATA64,
		1
	);

	// Poll the EOP label without requesting a CP interrupt. shadPS4 lacks
	// the upstream INT_SEL=3 case; INT_SEL=0 + DATA_SEL=2 writes the same label.
	cmdbuf.cmdptr[-3] &= ~(3u << 24);

	void* dcbaddr = cmdbuf.beginptr;
	uint32_t dcbsize = (cmdbuf.cmdptr - cmdbuf.beginptr) * sizeof(uint32_t);
	void* ccbaddr = NULL;
	uint32_t ccbsize = 0;

	FILE *dump = fopen("/data/opengnm-commands.bin", "wb");
	if (dump) { fwrite(dcbaddr, 1, dcbsize, dump); fclose(dump); }

	for (unsigned frame = 0; frame < 120; ++frame) {
		*label = 9;

		int submit = sceGnmSubmitCommandBuffers(1, &dcbaddr, &dcbsize, &ccbaddr, &ccbsize);
		if (submit) { printf("FAIL submit: 0x%x\n", submit); return 2; }

		unsigned waited = 0;
		while (*(volatile uint64_t*)label != 1) {
			if (++waited > 5000) { puts("FAIL GPU fence timeout"); return 2; }
			sceKernelUsleep(1000);
		}

		if (!displayctx_flip(&display, 0)) {
			puts("FAIL displayflip");
			return 2;
		}

		sceGnmSubmitDone();
		if (frame == 0 || frame == 119) printf("FRAME %u submitted and flipped\n", frame);
	}

	puts("PASS submission/flip loop; pixels require separate inspection");
	sceKernelUsleep(10000000);
	displayctx_destroy(&display);
	memalloc_destroy(&garlicmem);

	return 0;
}
