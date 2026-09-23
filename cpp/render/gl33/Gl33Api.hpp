#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace hg::render::gl33 {

using Enum = std::uint32_t;
using UInt = std::uint32_t;
using Int = std::int32_t;
using SizeI = std::int32_t;
using Bitfield = std::uint32_t;
using Boolean = std::uint8_t;
using Float = float;
using Char = char;
using UByte = unsigned char;
using SizePtr = std::ptrdiff_t;

constexpr Boolean kFalse = 0;
constexpr Boolean kTrue = 1;
constexpr Enum kVendor = 0x1F00;
constexpr Enum kRenderer = 0x1F01;
constexpr Enum kVersion = 0x1F02;
constexpr Enum kShadingLanguageVersion = 0x8B8C;
constexpr Enum kContextProfileMask = 0x9126;
constexpr Int kContextCoreProfileBit = 0x00000001;
constexpr Enum kColorBufferBit = 0x00004000;
constexpr Enum kDepthBufferBit = 0x00000100;
constexpr Enum kDepthTest = 0x0B71;
constexpr Enum kBlend = 0x0BE2;
constexpr Enum kCullFace = 0x0B44;
constexpr Enum kLessEqual = 0x0203;
constexpr Enum kOne = 1;
constexpr Enum kSourceAlpha = 0x0302;
constexpr Enum kOneMinusSourceAlpha = 0x0303;
constexpr Enum kArrayBuffer = 0x8892;
constexpr Enum kElementArrayBuffer = 0x8893;
constexpr Enum kStaticDraw = 0x88E4;
constexpr Enum kDynamicDraw = 0x88E8;
constexpr Enum kFloat = 0x1406;
constexpr Enum kUnsignedInt = 0x1405;
constexpr Enum kUnsignedByte = 0x1401;
constexpr Enum kTriangles = 0x0004;
constexpr Enum kTexture2D = 0x0DE1;
constexpr Enum kTexture0 = 0x84C0;
constexpr Enum kTextureMinFilter = 0x2801;
constexpr Enum kTextureMagFilter = 0x2800;
constexpr Enum kTextureWrapS = 0x2802;
constexpr Enum kTextureWrapT = 0x2803;
constexpr Enum kLinear = 0x2601;
constexpr Enum kLinearMipmapLinear = 0x2703;
constexpr Enum kRepeat = 0x2901;
constexpr Enum kClampToEdge = 0x812F;
constexpr Enum kNearest = 0x2600;
constexpr Enum kRgba = 0x1908;
constexpr Int kRgba8 = 0x8058;
constexpr Enum kDepthComponent = 0x1902;
constexpr Int kDepthComponent24 = 0x81A6;
constexpr Enum kFramebuffer = 0x8D40;
constexpr Enum kColorAttachment0 = 0x8CE0;
constexpr Enum kDepthAttachment = 0x8D00;
constexpr Enum kFramebufferComplete = 0x8CD5;
constexpr Enum kReadFramebuffer = 0x8CA8;
constexpr Enum kDrawFramebuffer = 0x8CA9;
constexpr Enum kUnpackAlignment = 0x0CF5;
constexpr Enum kNone = 0;
constexpr Enum kViewport = 0x0BA2;
constexpr Enum kPolygonOffsetFill = 0x8037;
constexpr Enum kTextureCompareMode = 0x884C;
constexpr Enum kTextureCompareFunc = 0x884D;
constexpr Enum kCompareRefToTexture = 0x884E;
constexpr Enum kVertexShader = 0x8B31;
constexpr Enum kFragmentShader = 0x8B30;
constexpr Enum kCompileStatus = 0x8B81;
constexpr Enum kLinkStatus = 0x8B82;
constexpr Enum kInfoLogLength = 0x8B84;

#if defined(_WIN32)
#define HG_GL33_APIENTRY __stdcall
#else
#define HG_GL33_APIENTRY
#endif

/// <summary>Function table loaded from the current OpenGL 3.3 Core context.</summary>
class Gl33Api final {
public:
  using GetStringProc = const UByte* (HG_GL33_APIENTRY*)(Enum);
  using GetIntegervProc = void (HG_GL33_APIENTRY*)(Enum, Int*);
  using ViewportProc = void (HG_GL33_APIENTRY*)(Int, Int, SizeI, SizeI);
  using ClearColorProc = void (HG_GL33_APIENTRY*)(Float, Float, Float, Float);
  using ClearProc = void (HG_GL33_APIENTRY*)(Bitfield);
  using EnableProc = void (HG_GL33_APIENTRY*)(Enum);
  using DisableProc = void (HG_GL33_APIENTRY*)(Enum);
  using DepthFuncProc = void (HG_GL33_APIENTRY*)(Enum);
  using BlendFuncProc = void (HG_GL33_APIENTRY*)(Enum, Enum);
  using DepthMaskProc = void (HG_GL33_APIENTRY*)(Boolean);
  using GenBuffersProc = void (HG_GL33_APIENTRY*)(SizeI, UInt*);
  using DeleteBuffersProc = void (HG_GL33_APIENTRY*)(SizeI, const UInt*);
  using BindBufferProc = void (HG_GL33_APIENTRY*)(Enum, UInt);
  using BufferDataProc = void (HG_GL33_APIENTRY*)(Enum, SizePtr, const void*, Enum);
  using GenVertexArraysProc = void (HG_GL33_APIENTRY*)(SizeI, UInt*);
  using DeleteVertexArraysProc = void (HG_GL33_APIENTRY*)(SizeI, const UInt*);
  using BindVertexArrayProc = void (HG_GL33_APIENTRY*)(UInt);
  using EnableVertexAttribArrayProc = void (HG_GL33_APIENTRY*)(UInt);
  using VertexAttribPointerProc = void (HG_GL33_APIENTRY*)(UInt, Int, Enum, Boolean, SizeI, const void*);
  using VertexAttribDivisorProc = void (HG_GL33_APIENTRY*)(UInt, UInt);
  using CreateShaderProc = UInt (HG_GL33_APIENTRY*)(Enum);
  using ShaderSourceProc = void (HG_GL33_APIENTRY*)(UInt, SizeI, const Char* const*, const Int*);
  using CompileShaderProc = void (HG_GL33_APIENTRY*)(UInt);
  using GetShaderivProc = void (HG_GL33_APIENTRY*)(UInt, Enum, Int*);
  using GetShaderInfoLogProc = void (HG_GL33_APIENTRY*)(UInt, SizeI, SizeI*, Char*);
  using DeleteShaderProc = void (HG_GL33_APIENTRY*)(UInt);
  using CreateProgramProc = UInt (HG_GL33_APIENTRY*)();
  using AttachShaderProc = void (HG_GL33_APIENTRY*)(UInt, UInt);
  using DetachShaderProc = void (HG_GL33_APIENTRY*)(UInt, UInt);
  using LinkProgramProc = void (HG_GL33_APIENTRY*)(UInt);
  using GetProgramivProc = void (HG_GL33_APIENTRY*)(UInt, Enum, Int*);
  using GetProgramInfoLogProc = void (HG_GL33_APIENTRY*)(UInt, SizeI, SizeI*, Char*);
  using DeleteProgramProc = void (HG_GL33_APIENTRY*)(UInt);
  using UseProgramProc = void (HG_GL33_APIENTRY*)(UInt);
  using GetUniformLocationProc = Int (HG_GL33_APIENTRY*)(UInt, const Char*);
  using UniformMatrix4fvProc = void (HG_GL33_APIENTRY*)(Int, SizeI, Boolean, const Float*);
  using Uniform1fProc = void (HG_GL33_APIENTRY*)(Int, Float);
  using Uniform1iProc = void (HG_GL33_APIENTRY*)(Int, Int);
  using Uniform3fProc = void (HG_GL33_APIENTRY*)(Int, Float, Float, Float);
  using Uniform4fProc = void (HG_GL33_APIENTRY*)(Int, Float, Float, Float, Float);
  using DrawElementsProc = void (HG_GL33_APIENTRY*)(Enum, SizeI, Enum, const void*);
  using DrawElementsInstancedProc = void (HG_GL33_APIENTRY*)(Enum, SizeI, Enum,
                                                              const void*, SizeI);
  using ActiveTextureProc = void (HG_GL33_APIENTRY*)(Enum);
  using GenTexturesProc = void (HG_GL33_APIENTRY*)(SizeI, UInt*);
  using DeleteTexturesProc = void (HG_GL33_APIENTRY*)(SizeI, const UInt*);
  using BindTextureProc = void (HG_GL33_APIENTRY*)(Enum, UInt);
  using TexParameteriProc = void (HG_GL33_APIENTRY*)(Enum, Enum, Int);
  using PixelStoreiProc = void (HG_GL33_APIENTRY*)(Enum, Int);
  using TexImage2DProc = void (HG_GL33_APIENTRY*)(Enum, Int, Int, SizeI, SizeI,
                                                  Int, Enum, Enum, const void*);
  using GenerateMipmapProc = void (HG_GL33_APIENTRY*)(Enum);
  using GenFramebuffersProc = void (HG_GL33_APIENTRY*)(SizeI, UInt*);
  using DeleteFramebuffersProc = void (HG_GL33_APIENTRY*)(SizeI, const UInt*);
  using BindFramebufferProc = void (HG_GL33_APIENTRY*)(Enum, UInt);
  using FramebufferTexture2DProc = void (HG_GL33_APIENTRY*)(Enum, Enum, Enum, UInt, Int);
  using CheckFramebufferStatusProc = Enum (HG_GL33_APIENTRY*)(Enum);
  using BlitFramebufferProc = void (HG_GL33_APIENTRY*)(
      Int, Int, Int, Int, Int, Int, Int, Int, Bitfield, Enum);
  using DrawBufferProc = void (HG_GL33_APIENTRY*)(Enum);
  using ReadBufferProc = void (HG_GL33_APIENTRY*)(Enum);
  using PolygonOffsetProc = void (HG_GL33_APIENTRY*)(Float, Float);

  /// <summary>Loads every function required by the first Core renderer stage.</summary>
  bool load();
  /// <returns>Human-readable name of the first unavailable function.</returns>
  const std::string& loadError() const;

  GetStringProc GetString = nullptr;
  GetIntegervProc GetIntegerv = nullptr;
  ViewportProc Viewport = nullptr;
  ClearColorProc ClearColor = nullptr;
  ClearProc Clear = nullptr;
  EnableProc Enable = nullptr;
  DisableProc Disable = nullptr;
  DepthFuncProc DepthFunc = nullptr;
  BlendFuncProc BlendFunc = nullptr;
  DepthMaskProc DepthMask = nullptr;
  GenBuffersProc GenBuffers = nullptr;
  DeleteBuffersProc DeleteBuffers = nullptr;
  BindBufferProc BindBuffer = nullptr;
  BufferDataProc BufferData = nullptr;
  GenVertexArraysProc GenVertexArrays = nullptr;
  DeleteVertexArraysProc DeleteVertexArrays = nullptr;
  BindVertexArrayProc BindVertexArray = nullptr;
  EnableVertexAttribArrayProc EnableVertexAttribArray = nullptr;
  VertexAttribPointerProc VertexAttribPointer = nullptr;
  VertexAttribDivisorProc VertexAttribDivisor = nullptr;
  CreateShaderProc CreateShader = nullptr;
  ShaderSourceProc ShaderSource = nullptr;
  CompileShaderProc CompileShader = nullptr;
  GetShaderivProc GetShaderiv = nullptr;
  GetShaderInfoLogProc GetShaderInfoLog = nullptr;
  DeleteShaderProc DeleteShader = nullptr;
  CreateProgramProc CreateProgram = nullptr;
  AttachShaderProc AttachShader = nullptr;
  DetachShaderProc DetachShader = nullptr;
  LinkProgramProc LinkProgram = nullptr;
  GetProgramivProc GetProgramiv = nullptr;
  GetProgramInfoLogProc GetProgramInfoLog = nullptr;
  DeleteProgramProc DeleteProgram = nullptr;
  UseProgramProc UseProgram = nullptr;
  GetUniformLocationProc GetUniformLocation = nullptr;
  UniformMatrix4fvProc UniformMatrix4fv = nullptr;
  Uniform1fProc Uniform1f = nullptr;
  Uniform1iProc Uniform1i = nullptr;
  Uniform3fProc Uniform3f = nullptr;
  Uniform4fProc Uniform4f = nullptr;
  DrawElementsProc DrawElements = nullptr;
  DrawElementsInstancedProc DrawElementsInstanced = nullptr;
  ActiveTextureProc ActiveTexture = nullptr;
  GenTexturesProc GenTextures = nullptr;
  DeleteTexturesProc DeleteTextures = nullptr;
  BindTextureProc BindTexture = nullptr;
  TexParameteriProc TexParameteri = nullptr;
  PixelStoreiProc PixelStorei = nullptr;
  TexImage2DProc TexImage2D = nullptr;
  GenerateMipmapProc GenerateMipmap = nullptr;
  GenFramebuffersProc GenFramebuffers = nullptr;
  DeleteFramebuffersProc DeleteFramebuffers = nullptr;
  BindFramebufferProc BindFramebuffer = nullptr;
  FramebufferTexture2DProc FramebufferTexture2D = nullptr;
  CheckFramebufferStatusProc CheckFramebufferStatus = nullptr;
  BlitFramebufferProc BlitFramebuffer = nullptr;
  DrawBufferProc DrawBuffer = nullptr;
  ReadBufferProc ReadBuffer = nullptr;
  PolygonOffsetProc PolygonOffset = nullptr;

private:
  std::string loadError_;
};

/// <returns>The process-wide Core function table.</returns>
Gl33Api& api();

#undef HG_GL33_APIENTRY

}  // namespace hg::render::gl33
