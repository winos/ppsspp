#include "GPU/Common/FragmentShaderGeneratorCommon.h"
#include "GPU/Common/GPUStateUtils.h"
#include "GPU/GPUState.h"

#define WRITE p+=sprintf

char *WriteReplaceBlend(char *p, const FShaderID &id, const ShaderCompat &compat) {
	ReplaceBlendType replaceBlend = static_cast<ReplaceBlendType>(id.Bits(FS_BIT_REPLACE_BLEND, 3));
	GEBlendSrcFactor replaceBlendFuncA = (GEBlendSrcFactor)id.Bits(FS_BIT_BLENDFUNC_A, 4);
	GEBlendDstFactor replaceBlendFuncB = (GEBlendDstFactor)id.Bits(FS_BIT_BLENDFUNC_B, 4);
	GEBlendMode replaceBlendEq = (GEBlendMode)id.Bits(FS_BIT_BLENDEQ, 3);

	if (replaceBlend == REPLACE_BLEND_2X_SRC) {
		WRITE(p, "  v.rgb = v.rgb * 2.0;\n");
	}

	if (replaceBlend == REPLACE_BLEND_PRE_SRC || replaceBlend == REPLACE_BLEND_PRE_SRC_2X_ALPHA) {
		const char *srcFactor = "ERROR";
		switch (replaceBlendFuncA) {
		case GE_SRCBLEND_DSTCOLOR:          srcFactor = "ERROR"; break;
		case GE_SRCBLEND_INVDSTCOLOR:       srcFactor = "ERROR"; break;
		case GE_SRCBLEND_SRCALPHA:          srcFactor = "vec3(v.a)"; break;
		case GE_SRCBLEND_INVSRCALPHA:       srcFactor = "vec3(1.0 - v.a)"; break;
		case GE_SRCBLEND_DSTALPHA:          srcFactor = "ERROR"; break;
		case GE_SRCBLEND_INVDSTALPHA:       srcFactor = "ERROR"; break;
		case GE_SRCBLEND_DOUBLESRCALPHA:    srcFactor = "vec3(v.a * 2.0)"; break;
		case GE_SRCBLEND_DOUBLEINVSRCALPHA: srcFactor = "vec3(1.0 - v.a * 2.0)"; break;
		// PRE_SRC for REPLACE_BLEND_PRE_SRC_2X_ALPHA means "double the src."
		// It's close to the same, but clamping can still be an issue.
		case GE_SRCBLEND_DOUBLEDSTALPHA:    srcFactor = "vec3(2.0)"; break;
		case GE_SRCBLEND_DOUBLEINVDSTALPHA: srcFactor = "ERROR"; break;
		case GE_SRCBLEND_FIXA:              srcFactor = "u_blendFixA"; break;
		default:                            srcFactor = "u_blendFixA"; break;
		}

		WRITE(p, "  v.rgb = v.rgb * %s;\n", srcFactor);
	}

	if (replaceBlend == REPLACE_BLEND_COPY_FBO) {
		if (gstate_c.Supports(GPU_SUPPORTS_ANY_FRAMEBUFFER_FETCH)) {
			WRITE(p, "  lowp vec4 destColor = %s;\n", compat.lastFragData);
		} else if (!compat.texelFetch) {
			WRITE(p, "  lowp vec4 destColor = %s(fbotex, gl_FragCoord.xy * u_fbotexSize.xy);\n", compat.texture);
		} else {
			WRITE(p, "  lowp vec4 destColor = %s(fbotex, ivec2(gl_FragCoord.x, gl_FragCoord.y), 0);\n", compat.texelFetch);
		}

		const char *srcFactor = "vec3(1.0)";
		const char *dstFactor = "vec3(0.0)";

		switch (replaceBlendFuncA) {
		case GE_SRCBLEND_DSTCOLOR:          srcFactor = "destColor.rgb"; break;
		case GE_SRCBLEND_INVDSTCOLOR:       srcFactor = "(vec3(1.0) - destColor.rgb)"; break;
		case GE_SRCBLEND_SRCALPHA:          srcFactor = "vec3(v.a)"; break;
		case GE_SRCBLEND_INVSRCALPHA:       srcFactor = "vec3(1.0 - v.a)"; break;
		case GE_SRCBLEND_DSTALPHA:          srcFactor = "vec3(destColor.a)"; break;
		case GE_SRCBLEND_INVDSTALPHA:       srcFactor = "vec3(1.0 - destColor.a)"; break;
		case GE_SRCBLEND_DOUBLESRCALPHA:    srcFactor = "vec3(v.a * 2.0)"; break;
		case GE_SRCBLEND_DOUBLEINVSRCALPHA: srcFactor = "vec3(1.0 - v.a * 2.0)"; break;
		case GE_SRCBLEND_DOUBLEDSTALPHA:    srcFactor = "vec3(destColor.a * 2.0)"; break;
		case GE_SRCBLEND_DOUBLEINVDSTALPHA: srcFactor = "vec3(1.0 - destColor.a * 2.0)"; break;
		case GE_SRCBLEND_FIXA:              srcFactor = "u_blendFixA"; break;
		default:                            srcFactor = "u_blendFixA"; break;
		}
		switch (replaceBlendFuncB) {
		case GE_DSTBLEND_SRCCOLOR:          dstFactor = "v.rgb"; break;
		case GE_DSTBLEND_INVSRCCOLOR:       dstFactor = "(vec3(1.0) - v.rgb)"; break;
		case GE_DSTBLEND_SRCALPHA:          dstFactor = "vec3(v.a)"; break;
		case GE_DSTBLEND_INVSRCALPHA:       dstFactor = "vec3(1.0 - v.a)"; break;
		case GE_DSTBLEND_DSTALPHA:          dstFactor = "vec3(destColor.a)"; break;
		case GE_DSTBLEND_INVDSTALPHA:       dstFactor = "vec3(1.0 - destColor.a)"; break;
		case GE_DSTBLEND_DOUBLESRCALPHA:    dstFactor = "vec3(v.a * 2.0)"; break;
		case GE_DSTBLEND_DOUBLEINVSRCALPHA: dstFactor = "vec3(1.0 - v.a * 2.0)"; break;
		case GE_DSTBLEND_DOUBLEDSTALPHA:    dstFactor = "vec3(destColor.a * 2.0)"; break;
		case GE_DSTBLEND_DOUBLEINVDSTALPHA: dstFactor = "vec3(1.0 - destColor.a * 2.0)"; break;
		case GE_DSTBLEND_FIXB:              dstFactor = "u_blendFixB"; break;
		default:                            dstFactor = "u_blendFixB"; break;
		}

		switch (replaceBlendEq) {
		case GE_BLENDMODE_MUL_AND_ADD:
			WRITE(p, "  v.rgb = v.rgb * %s + destColor.rgb * %s;\n", srcFactor, dstFactor);
			break;
		case GE_BLENDMODE_MUL_AND_SUBTRACT:
			WRITE(p, "  v.rgb = v.rgb * %s - destColor.rgb * %s;\n", srcFactor, dstFactor);
			break;
		case GE_BLENDMODE_MUL_AND_SUBTRACT_REVERSE:
			WRITE(p, "  v.rgb = destColor.rgb * %s - v.rgb * %s;\n", dstFactor, srcFactor);
			break;
		case GE_BLENDMODE_MIN:
			WRITE(p, "  v.rgb = min(v.rgb, destColor.rgb);\n");
			break;
		case GE_BLENDMODE_MAX:
			WRITE(p, "  v.rgb = max(v.rgb, destColor.rgb);\n");
			break;
		case GE_BLENDMODE_ABSDIFF:
			WRITE(p, "  v.rgb = abs(v.rgb - destColor.rgb);\n");
			break;
		}
	}

	if (replaceBlend == REPLACE_BLEND_2X_ALPHA || replaceBlend == REPLACE_BLEND_PRE_SRC_2X_ALPHA) {
		WRITE(p, "  v.a = v.a * 2.0;\n");
	}
	return p;
}
