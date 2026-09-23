#include "render/gl33/Gl33Api.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace hg::render::gl33 {

namespace {

template <typename Proc>
bool loadFunction(Proc& destination, const char* name, std::string& error) {
  destination = reinterpret_cast<Proc>(glfwGetProcAddress(name));
  if (destination != nullptr) {
    return true;
  }
  if (error.empty()) {
    error = std::string("OpenGL 3.3 function unavailable: ") + name;
  }
  return false;
}

}  // namespace

bool Gl33Api::load() {
  loadError_.clear();
  bool loaded = true;
#define HG_LOAD_GL33(member) loaded = loadFunction(member, "gl" #member, loadError_) && loaded
  HG_LOAD_GL33(GetString);
  HG_LOAD_GL33(GetIntegerv);
  HG_LOAD_GL33(Viewport);
  HG_LOAD_GL33(ClearColor);
  HG_LOAD_GL33(Clear);
  HG_LOAD_GL33(Enable);
  HG_LOAD_GL33(Disable);
  HG_LOAD_GL33(DepthFunc);
  HG_LOAD_GL33(BlendFunc);
  HG_LOAD_GL33(DepthMask);
  HG_LOAD_GL33(GenBuffers);
  HG_LOAD_GL33(DeleteBuffers);
  HG_LOAD_GL33(BindBuffer);
  HG_LOAD_GL33(BufferData);
  HG_LOAD_GL33(GenVertexArrays);
  HG_LOAD_GL33(DeleteVertexArrays);
  HG_LOAD_GL33(BindVertexArray);
  HG_LOAD_GL33(EnableVertexAttribArray);
  HG_LOAD_GL33(VertexAttribPointer);
  HG_LOAD_GL33(VertexAttribDivisor);
  HG_LOAD_GL33(CreateShader);
  HG_LOAD_GL33(ShaderSource);
  HG_LOAD_GL33(CompileShader);
  HG_LOAD_GL33(GetShaderiv);
  HG_LOAD_GL33(GetShaderInfoLog);
  HG_LOAD_GL33(DeleteShader);
  HG_LOAD_GL33(CreateProgram);
  HG_LOAD_GL33(AttachShader);
  HG_LOAD_GL33(DetachShader);
  HG_LOAD_GL33(LinkProgram);
  HG_LOAD_GL33(GetProgramiv);
  HG_LOAD_GL33(GetProgramInfoLog);
  HG_LOAD_GL33(DeleteProgram);
  HG_LOAD_GL33(UseProgram);
  HG_LOAD_GL33(GetUniformLocation);
  HG_LOAD_GL33(UniformMatrix4fv);
  HG_LOAD_GL33(Uniform1f);
  HG_LOAD_GL33(Uniform1i);
  HG_LOAD_GL33(Uniform3f);
  HG_LOAD_GL33(Uniform4f);
  HG_LOAD_GL33(DrawElements);
  HG_LOAD_GL33(DrawElementsInstanced);
  HG_LOAD_GL33(ActiveTexture);
  HG_LOAD_GL33(GenTextures);
  HG_LOAD_GL33(DeleteTextures);
  HG_LOAD_GL33(BindTexture);
  HG_LOAD_GL33(TexParameteri);
  HG_LOAD_GL33(PixelStorei);
  HG_LOAD_GL33(TexImage2D);
  HG_LOAD_GL33(GenerateMipmap);
  HG_LOAD_GL33(GenFramebuffers);
  HG_LOAD_GL33(DeleteFramebuffers);
  HG_LOAD_GL33(BindFramebuffer);
  HG_LOAD_GL33(FramebufferTexture2D);
  HG_LOAD_GL33(CheckFramebufferStatus);
  HG_LOAD_GL33(BlitFramebuffer);
#undef HG_LOAD_GL33
  return loaded;
}

const std::string& Gl33Api::loadError() const {
  return loadError_;
}

Gl33Api& api() {
  static Gl33Api instance;
  return instance;
}

}  // namespace hg::render::gl33
