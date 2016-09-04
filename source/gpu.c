#include "gpu.h"

#include <3ds.h>
#include <citro3d.h>


#include <assert.h>

#include <libavformat/avformat.h>

#include <shader_shbin.h>


typedef struct
{
    float x, y;
} vector_2f;

typedef struct
{
    float x, y, z;
} vector_3f;

typedef struct
{
    float r, g, b, a;
} vector_4f;

typedef struct
{
    u8 r, g, b, a;
} vector_4u8;

typedef struct __attribute__((__packed__))
{
    vector_3f position;
    vector_4u8 color;
    vector_2f texpos;
} vertex_pos_col;

#define CLEAR_COLOR 0x000000FF
#define ABGR8(r, g, b, a) ((((r)&0xFF)<<24) | (((g)&0xFF)<<16) | (((b)&0xFF)<<8) | (((a)&0xFF)<<0))
#define BG_COLOR_U8 {0xFF,0xFF,0xFF,0xFF}
#define C3D_CMD_SIZE 0x40000

// Used to transfer the final rendered display to the framebuffer
#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

// Used to convert textures to 3DS tiled format
// Note: vertical flip flag set so 0,0 is top left of texture
#define TEXTURE_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))


u32 clearColor = ABGR8(0x68, 0xB0, 0xD8, 0xFF);

static const vertex_pos_col test_mesh[] =
{
	{{-1.0f, -1.0f, -0.5f}, BG_COLOR_U8, {1.0f, 0.0f}},
	{{1.0f,  -1.0f, -0.5f}, BG_COLOR_U8, {1.0f, 1.0f}},
	{{1.0f,  1.0f,  -0.5f}, BG_COLOR_U8, {0.0f, 1.0f}},
	{{-1.0f, 1.0f,  -0.5f}, BG_COLOR_U8, {0.0f, 0.0f}},
};

static void *test_data = NULL;
C3D_TexEnv* env;
C3D_RenderTarget* target;
C3D_Tex tex;
//GPU framebuffer address
u32 *gpuFBuffer = NULL;
//GPU depth buffer address
u32 *gpuDBuffer = NULL;
//GPU command buffers
u32 *gpuCmd = NULL;

//shader structure
DVLB_s *shader_dvlb;    //the header
shaderProgram_s shader; //the program

static int uLoc_projection;
static C3D_Mtx projection;

Result projUniformRegister = -1;
Result modelviewUniformRegister = -1;

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
    Result res = 0;

		test_data = linearAlloc(sizeof(test_mesh));     //allocate our vbo on the linear heap
    memcpy(test_data, test_mesh, sizeof(test_mesh)); //Copy our data
    //Allocate the GPU render buffers
    gpuFBuffer = vramMemAlign(400 * 240 * 4 * 2 * 2, 0x100);
    gpuDBuffer = vramMemAlign(400 * 240 * 4 * 2 * 2, 0x100);


    //In this example we are only rendering in "2D mode", so we don't need one command buffer per eye
    gpuCmd = linearAlloc(C3D_CMD_SIZE * (sizeof *gpuCmd)); //Don't forget that commands size is 4 (hence the sizeof)

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);//initialize GPU

		// Initialize the render target
		target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
		C3D_RenderTargetSetClear(target, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
		C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
    /**
    * Load our vertex shader and its uniforms
    * Check http://3dbrew.org/wiki/SHBIN for more informations about the shader binaries
    */
    shader_dvlb = DVLB_ParseFile((u32 *) shader_shbin, shader_shbin_size);//load our vertex shader binary
    shaderProgramInit(&shader);
    shaderProgramSetVsh(&shader, &shader_dvlb->DVLE[0]);
		C3D_BindProgram(&shader);

    projUniformRegister = shaderInstanceGetUniformLocation(shader.vertexShader, "projection");

    Mtx_OrthoTilt(&projection, 0.0f, 240.0f, 0.0f, 400.0f, 0.0f, 1.0f, true); // A basic projection for 2D drawings
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

    // Configure attributes for use with the vertex shader
  	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
  	AttrInfo_Init(attrInfo);
  	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
  	AttrInfo_AddLoader(attrInfo, 1, GPU_UNSIGNED_BYTE, 4); // v1=color
  	AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2); // v2=uvs

		// Configure buffers
		C3D_BufInfo* bufInfo = C3D_GetBufInfo();
		BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, test_data, sizeof(vertex_pos_col), 4, 0x210);

		env = C3D_GetTexEnv(0);
		C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
		C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
		C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

		env->srcAlpha = env->srcRgb = GPU_TEVSOURCES(GPU_TEXTURE0, GPU_TEXTURE0, 0);
		env->opAlpha = env->opRgb = GPU_TEVOPERANDS(0, 0, 0);
		env->funcAlpha = env->funcRgb = GPU_REPLACE;
		env->color = 0xAABBCCDD;
    C3D_SetTexEnv(0, env);

		gpuDisableEverything();
}

void gpuExit()
{

    if (test_data)
    {
        linearFree(test_data);
    }
    //do things properly
    linearFree(gpuCmd);
    vramFree(gpuDBuffer);
    vramFree(gpuFBuffer);
    shaderProgramFree(&shader);
    DVLB_Free(shader_dvlb);
    C3D_Fini();
}


void gpuRenderFrame(StreamState *ss)
{

		C3D_TexInit(&tex, ss->outFrame->width, ss->outFrame->height, GPU_RGBA8);
		C3D_SafeDisplayTransfer (osConvertVirtToPhys((u32*)ss->outFrame->data[0]), GX_BUFFER_DIM(ss->outFrame->width,ss->outFrame->height), (u32*)tex.data, GX_BUFFER_DIM(ss->outFrame->width,ss->outFrame->height), TEXTURE_TRANSFER_FLAGS);
		gspWaitForPPF();

		C3D_TexSetWrap(&tex, GPU_TEXTURE_WRAP_S(1), GPU_TEXTURE_WRAP_T(1));
		C3D_TexSetFilter(&tex, GPU_LINEAR, GPU_NEAREST);

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C3D_FrameDrawOn(target);

  		C3D_TexBind(GPU_TEXUNIT0, &tex);

      GPUCMD_AddWrite(GPUREG_TEXUNIT0_BORDER_COLOR, 0xFFFF0000);

      //Display the buffers data
      C3D_DrawArrays(GPU_TRIANGLE_FAN, 0, sizeof(vertex_pos_col)*4);

  /*
      setTexturePart(test_data, 0.0, 1.0f - ss->pCodecCtx->height / (float) ss->outFrame->height,
                     ss->pCodecCtx->width / (float) ss->outFrame->width, 1.0f);*/
  //    setTexturePart(test_data,0.0,0.0,1.0,1.0);
      /*C3D_SetAttributeBuffers(
              3, // number of attributes
              (u32 *) osConvertVirtToPhys((u32) test_data),
              GPU_ATTRIBFMT(0, 3, GPU_FLOAT) | GPU_ATTRIBFMT(1, 4, GPU_UNSIGNED_BYTE) | GPU_ATTRIBFMT(2, 2, GPU_FLOAT),
              0xFFF8,//Attribute mask, in our case 0b1110 since we use only the first one
              0x210,//Attribute permutations (here it is the identity)
              1, //number of buffers
              (u32[]) {0x0}, // buffer offsets (placeholders)
              (u64[]) {0x210}, // attribute permutations for each buffer (identity again)
              (u8[]) {3} // number of attributes for each buffer
      );*/



		C3D_FrameEnd(0);
    gfxSwapBuffers();
}
