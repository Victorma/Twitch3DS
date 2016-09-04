#include "gpu.h"

#include <3ds.h>
#include <citro3d.h>


#include <assert.h>

#include <libavformat/avformat.h>

#include "vshader_shbin.h"


#define CLEAR_COLOR 0x68B0D8FF

// Used to transfer the final rendered display to the framebuffer
#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

// Used to convert textures to 3DS tiled format
// Note: vertical flip flag set so 0,0 is top left of texture
#define TEXTURE_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

/*	{ 200.0f, 200.0f, 0.5f },
	{ 100.0f, 40.0f, 0.5f },
	{ 300.0f, 40.0f, 0.5f },*/

typedef struct { float position[3]; float texturecoord[2]; } vertex;
static const vertex test_mesh[] =
{
  {{200.0f, 200.0f, 0.5f}, {1.0f, 0.0f}},
  {{100.0f, 40.0f,  0.5f}, {0.0f, 1.0f}},
  {{300.0f, 40.0f,  0.5f}, {0.0f, 0.0f}},
};

#define vertex_list_count (sizeof(test_mesh)/sizeof(test_mesh[0]))

static void *test_data = NULL;
C3D_TexEnv* env;
C3D_RenderTarget* target;
C3D_Tex tex;

// projection
int uLoc_projection;
C3D_Mtx  projection;

//shader structure
DVLB_s *shader_dvlb;    //the header
shaderProgram_s shader; //the program

void gpuDisableEverything()
{
    C3D_CullFace(GPU_CULL_NONE);
    //No stencil test
    C3D_StencilTest(false, GPU_ALWAYS, 0x00, 0xFF, 0x00);
    C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
    //No blending color
    C3D_BlendingColor(0);
    //Fake disable AB. We just ignore the Blending part
    C3D_AlphaBlend(
        GPU_BLEND_ADD,
        GPU_BLEND_ADD,
        GPU_ONE, GPU_ZERO,
        GPU_ONE, GPU_ZERO
    );
    C3D_AlphaTest(false, GPU_ALWAYS, 0x00);
    C3D_DepthTest(true, GPU_ALWAYS, GPU_WRITE_ALL);

    GPUCMD_AddMaskedWrite(GPUREG_EARLYDEPTH_TEST1, 0x1, 0);
    GPUCMD_AddWrite(GPUREG_EARLYDEPTH_TEST2, 0);
}

void gpuInit()
{

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

  	// Initialize the render target
    target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  	C3D_RenderTargetSetClear(target, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
  	C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
    /**
    * Load our vertex shader and its uniforms
    * Check http://3dbrew.org/wiki/SHBIN for more informations about the shader binaries
    */
    shader_dvlb = DVLB_ParseFile((u32 *) vshader_shbin, vshader_shbin_size);//load our vertex shader binary
    shaderProgramInit(&shader);
    shaderProgramSetVsh(&shader, &shader_dvlb->DVLE[0]);
		C3D_BindProgram(&shader);


    uLoc_projection   = shaderInstanceGetUniformLocation(shader.vertexShader, "projection");
	  Mtx_OrthoTilt(&projection, 0.0, 400.0, 0.0, 240.0, 0.0, 1.0, true);

    // Configure attributes for use with the vertex shader
  	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
  	AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
  	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord0

    test_data = linearAlloc(sizeof(test_mesh));     //allocate our vbo on the linear heap
    memcpy(test_data, test_mesh, sizeof(test_mesh)); //Copy our data

		// Configure buffers
		C3D_BufInfo* bufInfo = C3D_GetBufInfo();
		BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, test_data, sizeof(vertex), 2, 0x10);

		env = C3D_GetTexEnv(0);
		C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
		C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
		C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

		gpuDisableEverything();
}

void gpuExit()
{

    if (test_data)
    {
        linearFree(test_data);
    }

    shaderProgramFree(&shader);
    DVLB_Free(shader_dvlb);
    C3D_Fini();
}


void gpuRenderFrame(StreamState *ss)
{
    C3D_TexInit(&tex, ss->outFrame->width, ss->outFrame->height, GPU_RGBA8);
    C3D_TexSetWrap(&tex, GPU_TEXTURE_WRAP_S(1), GPU_TEXTURE_WRAP_T(1));
  	C3D_TexSetFilter(&tex, GPU_LINEAR, GPU_NEAREST);

		C3D_SafeDisplayTransfer ((u32 *) osConvertVirtToPhys((u32) ss->outFrame->data[0]), GX_BUFFER_DIM(ss->outFrame->width,ss->outFrame->height), (u32*)tex.data, GX_BUFFER_DIM(ss->outFrame->width,ss->outFrame->height), TEXTURE_TRANSFER_FLAGS);
		gspWaitForPPF();


		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C3D_FrameDrawOn(target);

  		C3D_TexBind(0, &tex);

    //  GPUCMD_AddWrite(GPUREG_TEXUNIT0_BORDER_COLOR, 0xFFFF0000);

      //Display the buffers data
      C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
      C3D_DrawArrays(GPU_TRIANGLES, 0, 3);

  /*
      setTexturePart(test_data, 0.0, 1.0f - ss->pCodecCtx->height / (float) ss->outFrame->height,
                     ss->pCodecCtx->width / (float) ss->outFrame->width, 1.0f);*/
  //    setTexturePart(test_data,0.0,0.0,1.0,1.0);


		C3D_FrameEnd(0);
}
